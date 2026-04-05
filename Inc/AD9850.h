#ifndef AD9850_HPP
#define AD9850_HPP

/**
 * @file    AD9850.hpp
 * @brief   C++ HAL driver for the AD9850 DDS synthesizer module.
 *          Targets STM32F411 with the STM32 HAL library.
 *
 * Wiring (defaults match the original libopencm3 example):
 *   AD9850 W_CLK  -> PA2
 *   AD9850 RESET  -> PA3
 *   AD9850 FQ_UD  -> PA4   (Frequency Update / LOAD)
 *   AD9850 D7     -> PA5   (serial data, LSB-first)
 *
 * Any GPIO port/pins may be used – pass them to the constructor.
 */

#include "stm32f4xx_hal.h"
#include <cstdint>

class AD9850
{
public:
    /**
     * @brief  Construct an AD9850 driver instance.
     *
     * @param  port       GPIO port used for all four signals (e.g. GPIOA).
     * @param  pinClock   GPIO pin for W_CLK.
     * @param  pinReset   GPIO pin for RESET.
     * @param  pinLoad    GPIO pin for FQ_UD (frequency update / load).
     * @param  pinData    GPIO pin for serial data (D7).
     * @param  refClockHz Reference oscillator frequency in Hz (default 125 MHz).
     */
    AD9850(GPIO_TypeDef* port,
           uint16_t      pinClock,
           uint16_t      pinReset,
           uint16_t      pinLoad,
           uint16_t      pinData,
           float         refClockHz = 125.0e6f);

    /**
     * @brief  Initialise the GPIO pins as push-pull outputs.
     *         Call once after the peripheral clock has been enabled for the
     *         chosen port (e.g. __HAL_RCC_GPIOA_CLK_ENABLE()).
     */
    void begin();

    /**
     * @brief  Reset the AD9850 and put it in serial-programming mode.
     *         Must be called at least once before the first frequency update.
     */
    void reset();

    /**
     * @brief  Set the output frequency.
     * @param  frequencyHz  Desired output frequency in Hz.
     *                      Valid range: 0 Hz to ~(refClock / 2) Hz.
     */
    void setFrequency(float frequencyHz);

    /**
     * @brief  Enable or disable the RF output.
     *
     * The AD9850 has no dedicated output-enable pin; this function silences
     * the carrier by writing a 0 Hz tuning word when @p enable is false, and
     * restores the last programmed frequency when @p enable is true.
     *
     * @param  enable  true  → restore and output the last set frequency.
     *                 false → write 0 Hz (carrier off / DAC at DC).
     */
    void outputEnable(bool enable);

    /**
     * @brief  Return the reference clock frequency passed to the constructor.
     */
    float getRefClock() const { return m_refClock; }

private:
    /* Write one byte to the AD9850, LSB first. */
    void writeByte(uint8_t byte);

    /* Pulse a pin high then low. */
    void pulse(uint16_t pin);

    GPIO_TypeDef* m_port;
    uint16_t      m_pinClock;
    uint16_t      m_pinReset;
    uint16_t      m_pinLoad;
    uint16_t      m_pinData;
    float         m_refClock;
    float         m_lastFrequency; /* cached by setFrequency(); restored by output_enable(true) */
};

#endif // AD9850_HPP
