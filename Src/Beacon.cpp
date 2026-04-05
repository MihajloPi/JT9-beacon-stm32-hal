// Beacon.cpp — JT9 beacon driver for STM32 (HAL)
//
// See Beacon.h for full documentation, timer configuration, and integration
// steps.
//
// MIT License — see JT9.cpp for full text.

#include "Beacon.h"

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Beacon::Beacon(Si5351            &si5351,
               TIM_HandleTypeDef *htim,
               GPIO_TypeDef      *ledPort,
               uint16_t           ledPin)
    : _oscType(OscType::Si5351),
      _htim(htim),
      _ledPort(ledPort),
      _ledPin(ledPin),
      _freqHz(0),
      _symbolTick(false)
{
    _osc.si5351 = &si5351;
}

Beacon::Beacon(AD9850             &ad9850,
               TIM_HandleTypeDef  *htim,
               GPIO_TypeDef       *ledPort,
               uint16_t            ledPin)
    : _oscType(OscType::AD9850),
      _htim(htim),
      _ledPort(ledPort),
      _ledPin(ledPin),
      _freqHz(0),
      _symbolTick(false)
{
    _osc.ad9850 = &ad9850;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

bool Beacon::init(uint64_t freqHz, uint32_t xoFreqHz, int32_t correction)
{
    _freqHz = freqHz;

    if (_oscType == OscType::Si5351)
    {
        // Si5351::init() waits for SYS_INIT to clear, applies crystal load,
        // reference frequency, and the ppb correction.
        if (!_osc.si5351->init(SI5351_CRYSTAL_LOAD_8PF, xoFreqHz, correction)) {
            return false; // Si5351 not found on I²C bus
        }

        // Pre-tune to symbol 0 and set drive strength.  Leave RF off until
        // transmit() is called so the PA is not keyed up prematurely.
        _osc.si5351->setFreq(symbolFreqCentihz(0), SI5351_CLK0);
        _osc.si5351->driveStrength(SI5351_CLK0, SI5351_DRIVE_8MA);
        _osc.si5351->outputEnable(SI5351_CLK0, 0);
    }
    else // OscType::AD9850
    {
        // xoFreqHz and correction are not used for the AD9850: the reference
        // clock is fixed at construction time via AD9850::AD9850(..., refClockHz).
        // begin() configures the GPIO pins; reset() puts the chip in serial mode.
        _osc.ad9850->begin();
        _osc.ad9850->reset();

        // Silence the output until transmit() is called.
        _osc.ad9850->setFrequency(0.0f);
    }

    // Ensure the LED is off.
    HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_RESET);

    // Start the symbol-clock timer in interrupt mode.
    // The timer must already be configured for the correct period (see
    // BEACON_TIM_PRESCALER / BEACON_TIM_PERIOD in Beacon.h).
    HAL_TIM_Base_Start_IT(_htim);

    return true;
}

// ---------------------------------------------------------------------------
// transmit
// ---------------------------------------------------------------------------

bool Beacon::transmit(char *message)
{
    // Encode the message; returns false if any characters were invalid.
    const bool valid = _jt9.encode(message, _txBuffer);

    // Signal invalid input with a brief double-blink before transmitting.
    if (!valid) {
        for (uint8_t i = 0; i < 2; i++) {
            HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_SET);
            HAL_Delay(100);
            HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_RESET);
            HAL_Delay(100);
        }
    }

    // Enable RF output and the TX indicator.
    rfEnable(true);
    HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_SET);

    for (uint8_t i = 0; i < JT9::SYMBOL_COUNT; i++) {
        // Tune the oscillator to the frequency for this symbol.
        setSymbolFreq(_txBuffer[i]);

        // Wait for the next timer tick, then clear the flag.
        _symbolTick = false;
        while (!_symbolTick) {
            // Spin — the CPU is otherwise idle between symbols.
            // Replace with __WFI() to sleep; see caveat in original header.
        }
    }

    // Disable RF output and the TX indicator.
    rfEnable(false);
    HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_RESET);

    return valid;
}

// ---------------------------------------------------------------------------
// notifySymbolClock
// ---------------------------------------------------------------------------

void Beacon::notifySymbolClock(TIM_HandleTypeDef *htim)
{
    // Only act on the timer this beacon owns.
    if (htim->Instance == _htim->Instance) {
        _symbolTick = true;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void Beacon::rfEnable(bool enable)
{
    if (_oscType == OscType::Si5351)
    {
        _osc.si5351->outputEnable(SI5351_CLK0, enable ? 1 : 0);
    }
    else // OscType::AD9850
    {
        _osc.ad9850->outputEnable(enable);
    }
}

void Beacon::setSymbolFreq(uint8_t symbol)
{
    if (_oscType == OscType::Si5351)
    {
        _osc.si5351->setFreq(symbolFreqCentihz(symbol), SI5351_CLK0);
    }
    else // OscType::AD9850
    {
        _osc.ad9850->setFrequency(symbolFreqHz(symbol));
    }
}
