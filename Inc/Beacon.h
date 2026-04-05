// Beacon.h — JT9 beacon driver for STM32 (HAL)
//
// Supports two local oscillator backends:
//   • Si5351  — I²C synthesiser, 0.01 Hz resolution, explicit output enable
//   • AD9850  — SPI/bit-bang DDS, 1 Hz resolution, silenced by writing 0 Hz
//
// All other behaviour (timer-gated symbol transmission, LED indicator,
// JT9 encoding) is identical regardless of which oscillator is used.
//
// See the constructor notes below for per-oscillator wiring and init details.
//
// TIMER SETUP (CubeMX / HAL — for JT9-1, symbol period ≈ 576.37 ms)
// ─────────────────────────────────────────────────────────────────────
//   Timer clock source : APBx timer clock (e.g. 84 MHz on STM32F411)
//   Prescaler          : (BEACON_TIM_PRESCALER - 1), default 8399
//                        → tick rate = 84 000 000 / 8400 = 10 000 Hz
//   Counter Period     : (BEACON_TIM_PERIOD - 1),    default 5763
//                        → interrupt every 5763 / 10 000 = 576.3 ms  ✓
//   Counter Mode       : Up
//   Auto-Reload Preload: Enable
//   Trigger interrupt  : TIM Update interrupt enabled in NVIC
//
// Adjust BEACON_TIM_PRESCALER / BEACON_TIM_PERIOD if your APB timer clock
// differs from 84 MHz.
//
// INTEGRATION STEPS
// ─────────────────
//   1. Generate your STM32 project with CubeMX; enable I2C (Si5351) or the
//      GPIO port (AD9850), plus a spare TIM.
//   2. Add JT9/JT9.h, JT9/JT9.cpp, Beacon/Beacon.h, Beacon/Beacon.cpp,
//      si5351.h/.cpp (or AD9850.h/.cpp) to the project source tree.
//   3. In main.cpp, construct a Beacon with the desired oscillator, call
//      init(), then call transmit().
//   4. In stm32f4xx_it.cpp, forward the TIM update callback:
//
//        void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
//            beacon.notifySymbolClock(htim);
//        }
//
//   5. Sync the first transmit() call to a UTC minute boundary.
//
// MIT License — see JT9.cpp for full text.

#pragma once

#include "stm32f4xx_hal.h"
#include "si5351.h"
#include "AD9850.h"
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
    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------

    /// Construct a Beacon using a Si5351 I²C synthesiser.
    ///
    /// The Si5351 object must already exist; Beacon stores a reference to it.
    /// Beacon::init() will call Si5351::init() internally.
    ///
    /// @param si5351     Reference to an (uninitialised) Si5351 object.
    /// @param htim       HAL timer handle used for symbol timing.
    /// @param ledPort    GPIO port for the TX indicator LED (e.g. GPIOC).
    /// @param ledPin     GPIO pin mask for the LED (e.g. GPIO_PIN_13).
    Beacon(Si5351            &si5351,
           TIM_HandleTypeDef *htim,
           GPIO_TypeDef      *ledPort,
           uint16_t           ledPin);

    /// Construct a Beacon using an AD9850 bit-bang DDS.
    ///
    /// The AD9850 object must already exist; Beacon stores a reference to it.
    /// Beacon::init() will call AD9850::begin() and AD9850::reset() internally.
    ///
    /// @note  The AD9850 driver silences the carrier between transmissions via
    ///        AD9850::outputEnable(false), which writes a zero tuning word
    ///        without overwriting the last programmed frequency.  If your
    ///        hardware has an additional MUTE/enable line, drive it separately.
    ///
    /// @param ad9850     Reference to an (uninitialised) AD9850 object.
    /// @param htim       HAL timer handle used for symbol timing.
    /// @param ledPort    GPIO port for the TX indicator LED (e.g. GPIOC).
    /// @param ledPin     GPIO pin mask for the LED (e.g. GPIO_PIN_13).
    Beacon(AD9850             &ad9850,
           TIM_HandleTypeDef  *htim,
           GPIO_TypeDef       *ledPort,
           uint16_t            ledPin);

    // -----------------------------------------------------------------------
    // Public API  (identical for both oscillator backends)
    // -----------------------------------------------------------------------

    /// Initialise the oscillator and prepare the symbol-clock timer.
    ///
    /// For Si5351: calls Si5351::init() and returns its result.
    /// For AD9850:  calls AD9850::begin() and AD9850::reset(); always returns
    ///              true (no bus to detect).
    ///
    /// @param freqHz     Transmit dial frequency in Hz (e.g. 14078600).
    /// @param xoFreqHz   Reference oscillator frequency in Hz.
    ///                   Si5351: 0 defaults to 25 MHz internally.
    ///                   AD9850: passed to AD9850 constructor (refClockHz);
    ///                           this parameter is ignored here because the
    ///                           AD9850 ref clock is fixed at construction
    ///                           time — pass 0 for consistency.
    /// @param correction Frequency correction in ppb (Si5351 only; ignored
    ///                   for AD9850).
    ///
    /// @return true on success; false only when Si5351 is not found on I²C.
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
    /// Call from HAL_TIM_PeriodElapsedCallback() in stm32f4xx_it.cpp:
    ///
    ///   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    ///       beacon.notifySymbolClock(htim);
    ///   }
    void notifySymbolClock(TIM_HandleTypeDef *htim);

private:
    // -----------------------------------------------------------------------
    // Oscillator backend
    // -----------------------------------------------------------------------

    /// Discriminator so the correct pointer in _osc is selected at runtime.
    enum class OscType { Si5351, AD9850 };

    OscType _oscType;

    /// Tagged union — only the member matching _oscType is valid.
    union OscUnion {
        Si5351 *si5351;
        AD9850 *ad9850;

        // Unions with pointer members need explicit constructors in C++11.
        OscUnion() : si5351(nullptr) {}
    } _osc;

    // -----------------------------------------------------------------------
    // Oscillator helpers
    // -----------------------------------------------------------------------

    /// Enable or disable the RF carrier.
    /// Si5351: uses outputEnable().
    /// AD9850: writes 0 Hz to silence, or the current dial frequency to unmute.
    void rfEnable(bool enable);

    /// Tune the oscillator to the frequency for the given JT9 symbol index.
    void setSymbolFreq(uint8_t symbol);

    /// Compute the centihz frequency word used by Si5351 for a given symbol.
    inline uint64_t symbolFreqCentihz(uint8_t symbol) const {
        return (_freqHz * SI5351_FREQ_MULT)
             + (static_cast<uint64_t>(symbol) * JT9::TONE_SPACING);
    }

    /// Compute the Hz frequency (as float) used by AD9850 for a given symbol.
    /// JT9::TONE_SPACING is in centihz (0.01 Hz units), so divide by 100.
    inline float symbolFreqHz(uint8_t symbol) const {
        return static_cast<float>(_freqHz)
             + (static_cast<float>(symbol) * static_cast<float>(JT9::TONE_SPACING) / 100.0f);
    }

    // -----------------------------------------------------------------------
    // Common members
    // -----------------------------------------------------------------------

    TIM_HandleTypeDef *_htim;
    GPIO_TypeDef      *_ledPort;
    uint16_t           _ledPin;
    uint64_t           _freqHz;       ///< Dial frequency stored by init()
    JT9                _jt9;
    uint8_t            _txBuffer[JT9::SYMBOL_COUNT];

    /// Set by notifySymbolClock(); tested and cleared by transmit().
    volatile bool _symbolTick;
};
