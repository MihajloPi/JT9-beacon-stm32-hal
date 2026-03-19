#pragma once

#include "stm32f4xx_hal.h"
#include <cstdint>
#include <functional>

// ---------------------------------------------------------------------------
// RotaryDirection
//
// Scoped enum (enum class) avoids name collisions and prevents implicit
// integer conversion — safer than the plain enum used in the C version.
// The underlying int8_t keeps the type small; values are still +1 / 0 / -1
// so counter += static_cast<int8_t>(dir) works as before.
// ---------------------------------------------------------------------------
enum class RotaryDirection : int8_t {
  CCW  = -1,
  None =  0,
  CW   =  1
};

// ---------------------------------------------------------------------------
// RotaryEncoder
//
// Wraps STM32 HAL GPIO calls for a standard incremental quadrature rotary
// encoder with an integrated push-button switch.
//
// Assumptions (CubeMX must satisfy these before calling begin()):
//   • CLK and DT pins are configured as GPIO_Input with Pull-up.
//   • SW  pin  is configured as GPIO_Input with Pull-up (active-LOW switch).
//   • The GPIO peripheral clocks are already enabled.
//
// Typical usage:
//
//   RotaryEncoder enc(GPIOA, GPIO_PIN_0,   // CLK
//                     GPIOA, GPIO_PIN_1,   // DT
//                     GPIOA, GPIO_PIN_2);  // SW
//   enc.begin();
//
//   // Optional: register callbacks instead of polling
//   enc.onRotate([](RotaryDirection dir, int32_t count) {
//     Display_Print(count);
//   });
//   enc.onPress([]() { /* handle reset */ });
//
//   while (1) { enc.update(); }
// ---------------------------------------------------------------------------
class RotaryEncoder {
public:
  // ---- Types --------------------------------------------------------------

  // Callback fired on every detected rotation step.
  // Receives the direction and the current internal counter value.
  using RotateCallback = std::function<void(RotaryDirection dir, int32_t counter)>;

  // Callback fired when the push-button transitions from released → pressed.
  using PressCallback  = std::function<void()>;

  // ---- Construction -------------------------------------------------------

  /**
   * @brief Construct a RotaryEncoder instance.
   *
   * No HAL calls are made here; hardware access begins in begin().
   *
   * @param clkPort / clkPin  CLOCK signal GPIO port and pin.
   * @param dtPort  / dtPin   DATA  signal GPIO port and pin.
   * @param swPort  / swPin   Switch signal GPIO port and pin.
   * @param debounceMs        Debounce window in milliseconds (default: 10).
   */
  RotaryEncoder(GPIO_TypeDef *clkPort, uint16_t clkPin,
                GPIO_TypeDef *dtPort,  uint16_t dtPin,
                GPIO_TypeDef *swPort,  uint16_t swPin,
                uint32_t debounceMs = 10U);

  // Non-copyable: each instance owns a specific set of hardware pins.
  RotaryEncoder(const RotaryEncoder &)            = delete;
  RotaryEncoder &operator=(const RotaryEncoder &) = delete;

  // ---- Lifecycle ----------------------------------------------------------

  /**
   * @brief Snapshot initial GPIO state.  Call once after MX_GPIO_Init().
   */
  void begin();

  /**
   * @brief Poll the encoder.  Call on every main-loop iteration, or from a
   *        periodic timer ISR (keep the period shorter than debounceMs).
   *
   *        If callbacks are registered they are invoked here.
   *        Otherwise check the return value and buttonPressed() directly.
   *
   * @return RotaryDirection::CW, CCW, or None.
   */
  RotaryDirection update();

  // ---- Callbacks (optional, fluent interface) -----------------------------

  /**
   * @brief Register a callback invoked on every rotation step.
   * @return *this for method chaining.
   */
  RotaryEncoder &onRotate(RotateCallback cb);

  /**
   * @brief Register a callback invoked on button press (falling edge only).
   * @return *this for method chaining.
   */
  RotaryEncoder &onPress(PressCallback cb);

  // ---- Direct state accessors ---------------------------------------------

  /** @brief True while the push-button is held down (active-LOW). */
  bool buttonPressed() const;

  /** @brief Current accumulated counter value. */
  int32_t counter() const;

  /** @brief Reset the internal counter to zero (or a specific value). */
  void resetCounter(int32_t value = 0);

private:
  // -- Pin descriptors --
  GPIO_TypeDef *const _clkPort;
  const uint16_t      _clkPin;
  GPIO_TypeDef *const _dtPort;
  const uint16_t      _dtPin;
  GPIO_TypeDef *const _swPort;
  const uint16_t      _swPin;

  // -- Configuration --
  const uint32_t _debounceMs;

  // -- Runtime state --
  uint32_t      _lastDebounceTime;
  GPIO_PinState _prevCLK;
  GPIO_PinState _prevDT;
  bool          _prevSwPressed;   // For press edge detection
  int32_t       _counter;

  // -- Callbacks --
  RotateCallback _rotateCb;
  PressCallback  _pressCb;
};
