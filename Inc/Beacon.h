// Beacon.h — JT9 beacon driver for STM32 (HAL)
//
// Wraps the JT9 encoder and the Si5351 HAL library into a single class that
// drives a complete JT9-1 transmission.  All Arduino / AVR specifics have
// been replaced with STM32 HAL equivalents:
//
//   Arduino                │  STM32 HAL
//   ───────────────────────┼──────────────────────────────────────────────
//   Timer1 ISR             │  HAL_TIM_PeriodElapsedCallback (TIM update IT)
//   digitalWrite()         │  HAL_GPIO_WritePin()
//   si5351.set_freq(f,0,c) │  si5351.set_freq(f, c)   ← 2-arg STM32 API
//   si5351.init(load,0)    │  si5351.init(load, xo_hz, corr)
//
// TIMER SETUP (CubeMX / HAL — for JT9-1, symbol period ≈ 576.37 ms)
// ─────────────────────────────────────────────────────────────────────
//   Timer clock source : APBx timer clock (e.g. 84 MHz on STM32F411)
//   Prescaler          : (BEACON_TIM_PRESCALER - 1), default 8399
//                        → tick rate = 84 000 000 / 8400 = 10 000 Hz
//   Counter Period     : (BEACON_TIM_PERIOD - 1),    default 5763
//                        → interrupt every 5763 / 10000 = 576.3 ms  ✓
//   Counter Mode       : Up
//   Auto-Reload Preload: Enable
//   Trigger interrupt  : TIM Update interrupt enabled in NVIC
//
// Adjust BEACON_TIM_PRESCALER / BEACON_TIM_PERIOD in Beacon.h if your
// APB timer clock differs from 84 MHz.
//
// INTEGRATION STEPS
// ─────────────────
//   1. Generate your STM32 project with CubeMX; enable I2C and a spare TIM.
//   2. Add JT9/JT9.h, JT9/JT9.cpp, Beacon/Beacon.h, Beacon/Beacon.cpp to
//      the project source tree.
//   3. In main.cpp, construct a Beacon, call init(), and call transmit().
//   4. In stm32f4xx_it.cpp (or wherever HAL timer callbacks live), forward
//      the TIM update callback to Beacon::notifySymbolClock().
//   5. Sync the first transmit() call to a UTC minute boundary.
//
// MIT License — see JT9.cpp for full text.

#pragma once

#include "stm32f4xx_hal.h"
#include "si5351.h"
#include "JT9.h"

// ---------------------------------------------------------------------------
// Timer tuning constants — adjust for your APB timer clock
// ---------------------------------------------------------------------------

/// Prescaler value (written to TIMx_PSC as BEACON_TIM_PRESCALER - 1).
/// Default targets a 10 kHz tick from an 84 MHz timer clock.
#ifndef BEACON_TIM_PRESCALER
#define BEACON_TIM_PRESCALER  8400U
#endif

/// Auto-reload value (written to TIMx_ARR as BEACON_TIM_PERIOD - 1).
/// Default gives a 576.3 ms period (JT9-1 symbol rate ≈ 1.736 Hz).
#ifndef BEACON_TIM_PERIOD
#define BEACON_TIM_PERIOD     5763U
#endif

// ---------------------------------------------------------------------------
// Beacon class
// ---------------------------------------------------------------------------

class Beacon
{
public:
    /// Construct a Beacon instance.
    ///
    /// @param si5351     Reference to an initialised Si5351 object.
    /// @param htim       Pointer to the HAL timer handle used for symbol
    ///                   timing.  The timer must be configured for Update
    ///                   interrupt mode (see header comment for exact values).
    /// @param ledPort    GPIO port for the TX indicator LED (e.g. GPIOC).
    /// @param ledPin     GPIO pin mask for the LED (e.g. GPIO_PIN_13).
    Beacon(Si5351 &si5351,
           TIM_HandleTypeDef *htim,
           GPIO_TypeDef      *ledPort,
           uint16_t           ledPin);

    /// Initialise the Si5351 and prepare the symbol clock timer.
    ///
    /// @param freqHz     Transmit dial frequency in Hz (e.g. 14078600).
    /// @param xoFreqHz   Reference oscillator frequency in Hz (0 = 25 MHz).
    /// @param correction Frequency correction in parts-per-billion.
    ///
    /// @return true if the Si5351 was found on the bus, false otherwise.
    bool init(uint64_t freqHz,
              uint32_t xoFreqHz   = 0,
              int32_t  correction = 0);

    /// Encode and transmit a JT9 message.
    ///
    /// Blocks until all 85 symbols have been sent.  Each symbol boundary is
    /// gated by the hardware timer; call this only after init() succeeds.
    ///
    /// @param message  Null-terminated string (≤13 valid JT9 characters).
    ///                 Modified in-place: upper-cased and space-padded.
    ///
    /// @return true if every character was valid; false if any were replaced.
    bool transmit(char *message);

    /// Forward the HAL timer period-elapsed callback to this instance.
    ///
    /// Call this from HAL_TIM_PeriodElapsedCallback() in stm32f4xx_it.cpp:
    ///
    ///   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    ///       beacon.notifySymbolClock(htim);
    ///   }
    void notifySymbolClock(TIM_HandleTypeDef *htim);

private:
    Si5351            &_si5351;
    TIM_HandleTypeDef *_htim;
    GPIO_TypeDef      *_ledPort;
    uint16_t           _ledPin;
    uint64_t           _freqHz;       ///< Dial frequency stored by init()
    JT9                _jt9;
    uint8_t            _txBuffer[JT9::SYMBOL_COUNT];

    /// Set by notifySymbolClock(); tested and cleared by transmit().
    volatile bool _symbolTick;

    /// Compute the 0.01 Hz unit frequency for a given symbol index.
    /// freq_centihz = (dialHz × 100) + (symbolIndex × TONE_SPACING)
    inline uint64_t symbolFreq(uint8_t symbol) const {
        return (_freqHz * SI5351_FREQ_MULT)
             + (static_cast<uint64_t>(symbol) * JT9::TONE_SPACING);
    }
};
