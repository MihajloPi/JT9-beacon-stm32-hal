#include "RotaryEncoder.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
RotaryEncoder::RotaryEncoder(GPIO_TypeDef *clkPort, uint16_t clkPin,
                             GPIO_TypeDef *dtPort,  uint16_t dtPin,
                             GPIO_TypeDef *swPort,  uint16_t swPin,
                             uint32_t debounceMs)
  : _clkPort(clkPort), _clkPin(clkPin),
    _dtPort(dtPort),   _dtPin(dtPin),
    _swPort(swPort),   _swPin(swPin),
    _debounceMs(debounceMs),
    _lastDebounceTime(0U),
    _prevCLK(GPIO_PIN_RESET),
    _prevDT(GPIO_PIN_RESET),
    _prevSwPressed(false),
    _counter(0)
{}

// ---------------------------------------------------------------------------
// begin()
//
// Snapshots the current GPIO state so the first update() call has a valid
// baseline to compare against.  Must be called after MX_GPIO_Init().
// ---------------------------------------------------------------------------
void RotaryEncoder::begin() {
  _prevCLK          = HAL_GPIO_ReadPin(_clkPort, _clkPin);
  _prevDT           = HAL_GPIO_ReadPin(_dtPort,  _dtPin);
  _prevSwPressed    = buttonPressed();
  _lastDebounceTime = HAL_GetTick();
}

// ---------------------------------------------------------------------------
// update()
//
// Direction logic (identical across all three versions of this library):
//
//   A quadrature encoder drives CLK and DT 90° out of phase.
//   Whenever CLK changes state, DT's relationship to CLK reveals direction:
//
//     currentDT != currentCLK  →  Clockwise        (RotaryDirection::CW)
//     currentDT == currentCLK  →  Counter-clockwise (RotaryDirection::CCW)
//
// Button edge detection:
//   Rather than reporting "button is held", we fire the callback only on the
//   falling edge (released → pressed transition). This prevents the reset
//   action from repeating while the finger is held down.
// ---------------------------------------------------------------------------
RotaryDirection RotaryEncoder::update() {
  // --- Respect the debounce window ----------------------------------------
  if ((HAL_GetTick() - _lastDebounceTime) < _debounceMs) {
    return RotaryDirection::None;
  }

  const GPIO_PinState currentCLK = HAL_GPIO_ReadPin(_clkPort, _clkPin);
  const GPIO_PinState currentDT  = HAL_GPIO_ReadPin(_dtPort,  _dtPin);
  RotaryDirection direction      = RotaryDirection::None;

  // --- Quadrature decoding ------------------------------------------------
  if (currentCLK != _prevCLK) {
    direction = (currentDT != currentCLK)
                ? RotaryDirection::CW
                : RotaryDirection::CCW;

    _counter += static_cast<int8_t>(direction);

    if (_rotateCb) {
      _rotateCb(direction, _counter);
    }
  }

  // --- Button falling-edge detection --------------------------------------
  const bool currentSwPressed = buttonPressed();
  if (currentSwPressed && !_prevSwPressed) {
    if (_pressCb) {
      _pressCb();
    }
  }
  _prevSwPressed = currentSwPressed;

  // --- Advance state ------------------------------------------------------
  _prevCLK          = currentCLK;
  _prevDT           = currentDT;
  _lastDebounceTime = HAL_GetTick();

  return direction;
}

// ---------------------------------------------------------------------------
// Callback registration (fluent)
// ---------------------------------------------------------------------------
RotaryEncoder &RotaryEncoder::onRotate(RotateCallback cb) {
  _rotateCb = cb;
  return *this;
}

RotaryEncoder &RotaryEncoder::onPress(PressCallback cb) {
  _pressCb = cb;
  return *this;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
bool RotaryEncoder::buttonPressed() const {
  // Switch is active-LOW: GPIO_PIN_RESET means the pin is pulled to GND.
  return HAL_GPIO_ReadPin(_swPort, _swPin) == GPIO_PIN_RESET;
}

int32_t RotaryEncoder::counter() const {
  return _counter;
}

void RotaryEncoder::resetCounter(int32_t value) {
  _counter = value;
}
