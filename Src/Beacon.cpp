// Beacon.cpp — JT9 beacon driver for STM32 (HAL)
//
// See Beacon.h for full documentation, timer configuration, and integration
// steps.
//
// MIT License — see JT9.cpp for full text.

#include "Beacon.h"

// Static member definition
//Beacon *Beacon::_instance = nullptr;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Beacon::Beacon(Si5351 &si5351,
               TIM_HandleTypeDef *htim,
               GPIO_TypeDef      *ledPort,
               uint16_t           ledPin)
    : _si5351(si5351),
      _htim(htim),
      _ledPort(ledPort),
      _ledPin(ledPin),
      _freqHz(0),
      _symbolTick(false)
{
//	_instance = this; // last constructed instance wins
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
bool Beacon::init(uint64_t freqHz, uint32_t xoFreqHz, int32_t correction)
{
    _freqHz = freqHz;

    // Initialise the Si5351.  init() waits for SYS_INIT to clear internally,
    // then applies crystal load, reference frequency, and correction.
    if (!_si5351.init(SI5351_CRYSTAL_LOAD_8PF, xoFreqHz, correction)) {
        return false; // Si5351 not found on I2C bus
    }

    // Pre-set the output frequency and drive strength; leave RF disabled
    // until transmit() is called so we don't key up the PA prematurely.
    _si5351.set_freq(symbolFreq(0), SI5351_CLK0);
    _si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    _si5351.output_enable(SI5351_CLK0, 0);

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
    _si5351.output_enable(SI5351_CLK0, 1);
    HAL_GPIO_WritePin(_ledPort, _ledPin, GPIO_PIN_SET);

    for (uint8_t i = 0; i < JT9::SYMBOL_COUNT; i++) {
        // Set the Si5351 to the frequency for this symbol.
        _si5351.set_freq(symbolFreq(_txBuffer[i]), SI5351_CLK0);

        // Wait for the next timer tick, then clear the flag.
        _symbolTick = false;
        while (!_symbolTick) {
            // Spin — the CPU is otherwise idle between symbols.
            // Insert __WFI() here if you want to sleep between ticks:
            //   __WFI();
            // Be aware that __WFI() may delay the wakeup by one tick if the
            // interrupt fires before the sleep instruction in some pipelines.
        }
    }

    // Disable RF output and the TX indicator.
    _si5351.output_enable(SI5351_CLK0, 0);
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
/*
extern "C"
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (Beacon::_instance != nullptr) {
        Beacon::_instance->notifySymbolClock(htim);
    }
}
*/
