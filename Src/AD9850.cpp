/**
 * @file    AD9850.cpp
 * @brief   C++ HAL driver implementation for the AD9850 DDS synthesizer.
 */

#include "AD9850.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AD9850::AD9850(GPIO_TypeDef* port,
               uint16_t      pinClock,
               uint16_t      pinReset,
               uint16_t      pinLoad,
               uint16_t      pinData,
               float         refClockHz)
    : m_port(port),
      m_pinClock(pinClock),
      m_pinReset(pinReset),
      m_pinLoad(pinLoad),
      m_pinData(pinData),
      m_refClock(refClockHz),
      m_lastFrequency(0.0f)
{
}

// ─── Public API ──────────────────────────────────────────────────────────────

void AD9850::begin()
{
    /* Configure all four pins as push-pull outputs, no pull, low speed.
     * The peripheral clock for the port must already be enabled by the caller
     * before begin() is called, e.g.:
     *   __HAL_RCC_GPIOA_CLK_ENABLE();
     */
    GPIO_InitTypeDef gpio = {};
    gpio.Pin   = m_pinClock | m_pinReset | m_pinLoad | m_pinData;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(m_port, &gpio);

    /* Drive all lines low to start in a known state. */
    HAL_GPIO_WritePin(m_port,
                      m_pinClock | m_pinReset | m_pinLoad | m_pinData,
                      GPIO_PIN_RESET);
}

void AD9850::reset()
{
    /* Ensure all lines start low. */
    HAL_GPIO_WritePin(m_port,
                      m_pinClock | m_pinReset | m_pinLoad | m_pinData,
                      GPIO_PIN_RESET);

    /* The AD9850 datasheet serial-mode initialisation sequence:
     *   1. Pulse RESET high.
     *   2. Pulse W_CLK high.
     *   3. Pulse FQ_UD high.
     * After this the device is ready to accept a 40-bit serial word.
     */
    pulse(m_pinReset);
    pulse(m_pinClock);
    pulse(m_pinLoad);
}

void AD9850::setFrequency(float frequencyHz)
{
    m_lastFrequency = frequencyHz;

    /* Compute 32-bit frequency tuning word:
     *   FTW = (f_out / f_ref) * 2^32
     *
     * The cast through double avoids precision loss for high frequencies
     * before the result is truncated to uint32_t.
     */
    const uint32_t tuningWord =
        static_cast<uint32_t>((static_cast<double>(frequencyHz) /
                                static_cast<double>(m_refClock)) * 4294967296.0);

    /* Send 40 bits: 32-bit tuning word (LSB first) + 8 control/phase bits.
     * The control byte is 0x00 for normal operation (power-up, no phase offset).
     */
    writeByte(static_cast<uint8_t>( tuningWord        & 0xFF));
    writeByte(static_cast<uint8_t>((tuningWord >>  8) & 0xFF));
    writeByte(static_cast<uint8_t>((tuningWord >> 16) & 0xFF));
    writeByte(static_cast<uint8_t>((tuningWord >> 24) & 0xFF));
    writeByte(0x00); // phase/control byte

    /* Strobe FQ_UD to latch the new frequency word into the DAC. */
    pulse(m_pinLoad);
}

void AD9850::outputEnable(bool enable)
{
    /* The AD9850 has no dedicated enable pin.  Write 0 Hz to silence the
     * carrier (DAC output sits at DC mid-scale), or restore the last
     * programmed frequency to resume transmission.
     *
     * Note: setFrequency(0) would overwrite m_lastFrequency, so we call
     * the low-level path directly when disabling.
     */
    if (enable)
    {
        setFrequency(m_lastFrequency);
    }
    else
    {
        /* Send a zero tuning word without touching m_lastFrequency. */
        writeByte(0x00);
        writeByte(0x00);
        writeByte(0x00);
        writeByte(0x00);
        writeByte(0x00); // phase/control byte
        pulse(m_pinLoad);
    }
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void AD9850::writeByte(uint8_t byte)
{
    /* Shift out 8 bits, LSB first. */
    for (uint8_t i = 0; i < 8; ++i)
    {
        const GPIO_PinState level =
            ((byte >> i) & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        HAL_GPIO_WritePin(m_port, m_pinData, level);

        /* Clock the bit in. */
        pulse(m_pinClock);
    }
}

void AD9850::pulse(uint16_t pin)
{
    HAL_GPIO_WritePin(m_port, pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(m_port, pin, GPIO_PIN_RESET);
}
