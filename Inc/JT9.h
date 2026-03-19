// JT9.h — Portable C++ library for JT9 symbol encoding
//
// Encodes an arbitrary Type-6 free-text message (up to 13 valid characters)
// into the 85-symbol JT9 transmission vector.  Hardware I/O is intentionally
// excluded; the library only performs the DSP pipeline so it can be reused
// with any RF front-end or platform.
//
// Valid message characters: A–Z, 0–9, space, + - . / ?
// Messages are silently upper-cased and right-padded with spaces to 13 chars.
//
// Porting notes vs. the AVR version:
//   - All AVR-specific PROGMEM / pgm_read_byte() removed.
//   - Tables are plain `static const` arrays; the STM32 toolchain places
//     const data in flash automatically (LMA in flash, no SRAM copy).
//   - <avr/pgmspace.h> replaced with <stdint.h> / <stddef.h>.
//   - No other platform dependencies; include this in any C++ project.
//
// MIT License — see JT9.cpp for full text.

#pragma once

#include <stdint.h>

class JT9
{
public:
    // -----------------------------------------------------------------------
    // Public constants
    // -----------------------------------------------------------------------

    /// Total symbols in one JT9 transmission (data + sync interleaved).
    static const uint8_t SYMBOL_COUNT = 85;

    /// Number of distinct tone bins (0–8).
    static const uint8_t TONES = 9;

    /// Tone spacing in units of 0.01 Hz (≈ 1.736 Hz between adjacent bins).
    static const uint16_t TONE_SPACING = 174;

    // -----------------------------------------------------------------------
    // Public interface
    // -----------------------------------------------------------------------

    /// Encode a free-text message into JT9 symbols.
    ///
    /// @param message  Null-terminated string, up to 13 valid characters.
    ///                 Modified in-place: upper-cased and space-padded to 13.
    /// @param symbols  Output buffer of at least SYMBOL_COUNT bytes.
    ///                 Each element is a tone index in [0, TONES).
    ///
    /// @return  true on success; false if any character was invalid (those
    ///          characters are silently replaced with space before encoding).
    bool encode(char *message, uint8_t symbols[SYMBOL_COUNT]);

    /// Map a single character to its JT9 alphabet index (0–41).
    /// Returns 255 for characters outside the allowed set.
    static uint8_t charToCode(char c);

private:
    // Pipeline stages — pure static functions; no instance state needed.
    static void packMessage(const char *msg, uint8_t c[13]);
    static void convolve(const uint8_t c[13], uint8_t s[206]);
    static void interleave(const uint8_t s[206], uint8_t d[206]);
    static void packAndGray(const uint8_t d[206], uint8_t g[69]);
    static void mergeSync(const uint8_t g[69], uint8_t symbols[SYMBOL_COUNT]);

    static inline uint8_t grayCode(uint8_t c) { return (c >> 1) ^ c; }
};
