// JT9.cpp — Portable C++ library for JT9 symbol encoding
//
// Implements the five-stage DSP pipeline:
//   packMessage → convolve → interleave → packAndGray → mergeSync
//
// Tables are plain `static const` arrays.  On STM32 (arm-none-eabi-g++) the
// linker script places all const-qualified data in flash automatically; no
// __attribute__((section)) decoration is needed.
//
// The convolutional encoder uses __builtin_parity() (a GCC intrinsic present
// in both avr-gcc and arm-none-eabi-gcc) instead of a 32-iteration loop.
//
// MIT License
//
// Copyright (c) 2024 — derived from work by Jason Milldrum NT7S
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include "JT9.h"

#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
namespace {

    static const uint8_t  MSG_LEN      = 13;
    static const uint8_t  CONV_BITS    = 206;
    static const uint8_t  DATA_SYMBOLS = 69;

    static const uint32_t POLY_0 = 0xF2D05351UL;
    static const uint32_t POLY_1 = 0xE4613C47UL;

    // Bit-reversal interleave permutation: the first 206 values produced by
    // iterating i = 0..254 and keeping bit_reverse_8(i) only when < 206.
    // Values >= 206 are skipped, so the table is NOT a simple 16-wide grid.
    // Plain const array — the linker places this in flash on all common targets.
    static const uint8_t INTERLEAVE_TABLE[206] = {
        // i=0..14   (i=7→224, i=11→208, i=13→240 skipped)
          0, 128,  64, 192,  32, 160,  96,
         16, 144,  80,
         48, 176, 112,
        // i=16..30  (i=23→232, i=27→216, i=31→248 skipped)
          8, 136,  72, 200,  40, 168, 104,
         24, 152,  88,
         56, 184, 120,
        // i=32..46  (i=39→228, i=43→212, i=47→244 skipped)
          4, 132,  68, 196,  36, 164, 100,
         20, 148,  84,
         52, 180, 116,
        // i=48..62  (i=55→236, i=59→220, i=63→252 skipped)
         12, 140,  76, 204,  44, 172, 108,
         28, 156,  92,
         60, 188, 124,
        // i=64..78  (i=71→226, i=75→210, i=79→242 skipped)
          2, 130,  66, 194,  34, 162,  98,
         18, 146,  82,
         50, 178, 114,
        // i=80..94  (i=87→234, i=91→218, i=95→250 skipped)
         10, 138,  74, 202,  42, 170, 106,
         26, 154,  90,
         58, 186, 122,
        // i=96..110 (i=103→230, i=107→214, i=111→246 skipped)
          6, 134,  70, 198,  38, 166, 102,
         22, 150,  86,
         54, 182, 118,
        // i=112..126 (i=115→206, i=119→238, i=123→222, i=127→254 skipped)
         14, 142,  78,
         46, 174, 110,
         30, 158,  94,
         62, 190, 126,
        // i=128..142 (i=135→225, i=139→209, i=143→241 skipped)
          1, 129,  65, 193,  33, 161,  97,
         17, 145,  81,
         49, 177, 113,
        // i=144..158 (i=151→233, i=155→217, i=159→249 skipped)
          9, 137,  73, 201,  41, 169, 105,
         25, 153,  89,
         57, 185, 121,
        // i=160..174 (i=167→229, i=171→213, i=175→245 skipped)
          5, 133,  69, 197,  37, 165, 101,
         21, 149,  85,
         53, 181, 117,
        // i=176..190 (i=183→237, i=187→221, i=191→253 skipped)
         13, 141,  77, 205,  45, 173, 109,
         29, 157,  93,
         61, 189, 125,
        // i=192..206 (i=199→227, i=203→211, i=207→243 skipped)
          3, 131,  67, 195,  35, 163,  99,
         19, 147,  83,
         51, 179, 115,
        // i=208..222 (i=215→235, i=219→219, i=223→251 skipped)
         11, 139,  75, 203,  43, 171, 107,
         27, 155,  91,
         59, 187, 123,
        // i=224..238 (i=231→231, i=235→215, i=239→247 skipped)
          7, 135,  71, 199,  39, 167, 103,
         23, 151,  87,
         55, 183, 119,
        // i=240..254 (i=243→207, i=247→239, i=251→223, i=255 not visited)
         15, 143,  79,
         47, 175, 111,
         31, 159,  95,
         63, 191, 127,
    };

    // Sync vector: 1 = sync tone (bin 0), 0 = next data symbol (bin g[j]+1).
    static const uint8_t SYNC_VECTOR[JT9::SYMBOL_COUNT] = {
        1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,
        0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,1,
        0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,1,0,1
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public — charToCode
// ---------------------------------------------------------------------------
uint8_t JT9::charToCode(char c)
{
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c - 'A' + 10);
    switch (c) {
        case ' ': return 36;
        case '+': return 37;
        case '-': return 38;
        case '.': return 39;
        case '/': return 40;
        case '?': return 41;
        default:  return 255;
    }
}

// ---------------------------------------------------------------------------
// Public — encode
// ---------------------------------------------------------------------------
bool JT9::encode(char *message, uint8_t symbols[SYMBOL_COUNT])
{
    bool valid = true;

    // Upper-case the input.
    for (uint8_t i = 0; message[i] != '\0' && i < MSG_LEN; i++) {
        if (islower(static_cast<unsigned char>(message[i]))) {
            message[i] = static_cast<char>(toupper(static_cast<unsigned char>(message[i])));
        }
    }

    // Right-pad with spaces to MSG_LEN.
    const uint8_t len = static_cast<uint8_t>(strlen(message));
    for (uint8_t i = len; i < MSG_LEN; i++) {
        message[i] = ' ';
    }

    // Validate; replace any remaining invalid characters with space.
    for (uint8_t i = 0; i < MSG_LEN; i++) {
        if (charToCode(message[i]) == 255) {
            message[i] = ' ';
            valid = false;
        }
    }

    // Pipeline.
    uint8_t packed[MSG_LEN];
    packMessage(message, packed);

    uint8_t convolved[CONV_BITS];
    convolve(packed, convolved);

    uint8_t interleaved[CONV_BITS];
    interleave(convolved, interleaved);

    uint8_t gray[DATA_SYMBOLS];
    packAndGray(interleaved, gray);

    mergeSync(gray, symbols);

    return valid;
}

// ---------------------------------------------------------------------------
// Private — Stage 1: bit packing
// ---------------------------------------------------------------------------
void JT9::packMessage(const char *msg, uint8_t c[13])
{
    uint32_t n1 = charToCode(msg[0]);
    n1 = n1 * 42 + charToCode(msg[1]);
    n1 = n1 * 42 + charToCode(msg[2]);
    n1 = n1 * 42 + charToCode(msg[3]);
    n1 = n1 * 42 + charToCode(msg[4]);

    uint32_t n2 = charToCode(msg[5]);
    n2 = n2 * 42 + charToCode(msg[6]);
    n2 = n2 * 42 + charToCode(msg[7]);
    n2 = n2 * 42 + charToCode(msg[8]);
    n2 = n2 * 42 + charToCode(msg[9]);

    uint32_t n3 = charToCode(msg[10]);
    n3 = n3 * 42 + charToCode(msg[11]);
    n3 = n3 * 42 + charToCode(msg[12]);

    // Overflow the two MSBs of N3 into N1 and N2, then set the freeform flag.
    n1 = (n1 << 1) | ((n3 >> 15) & 1);
    n2 = (n2 << 1) | ((n3 >> 16) & 1);
    n3 = (n3 & 0x7FFFUL) | 0x8000UL;

    // Serialise N1 (28 bits) → c[0..3], upper nibble of c[3].
    c[3] = static_cast<uint8_t>((n1 & 0x0F) << 4);  n1 >>= 4;
    c[2] = static_cast<uint8_t>(n1 & 0xFF);          n1 >>= 8;
    c[1] = static_cast<uint8_t>(n1 & 0xFF);          n1 >>= 8;
    c[0] = static_cast<uint8_t>(n1 & 0xFF);

    // Serialise N2 (28 bits) → lower nibble of c[3] and c[4..6].
    c[6] = static_cast<uint8_t>(n2 & 0xFF);          n2 >>= 8;
    c[5] = static_cast<uint8_t>(n2 & 0xFF);          n2 >>= 8;
    c[4] = static_cast<uint8_t>(n2 & 0xFF);          n2 >>= 8;
    c[3] |= static_cast<uint8_t>(n2 & 0x0F);

    // Serialise N3 (16 bits) → c[7..8].
    c[8] = static_cast<uint8_t>(n3 & 0xFF);          n3 >>= 8;
    c[7] = static_cast<uint8_t>(n3 & 0xFF);

    // Zero-pad to 13 bytes (appends the 31 trailing zero bits required by the spec).
    c[9] = c[10] = c[11] = c[12] = 0;
}

// ---------------------------------------------------------------------------
// Private — Stage 2: rate-1/2 convolutional encoding (K=32)
// ---------------------------------------------------------------------------
void JT9::convolve(const uint8_t c[13], uint8_t s[206])
{
    uint32_t reg0 = 0, reg1 = 0;
    uint8_t  bitCount = 0;

    for (uint8_t i = 0; i < MSG_LEN && bitCount < CONV_BITS; i++) {
        for (uint8_t j = 0; j < 8 && bitCount < CONV_BITS; j++) {
            const uint8_t inputBit = (c[i] >> (7u - j)) & 1u;

            reg0 = (reg0 << 1) | inputBit;
            reg1 = (reg1 << 1) | inputBit;

            s[bitCount++] = static_cast<uint8_t>(__builtin_parity(reg0 & POLY_0));
            s[bitCount++] = static_cast<uint8_t>(__builtin_parity(reg1 & POLY_1));
        }
    }
}

// ---------------------------------------------------------------------------
// Private — Stage 3: bit-reversal interleave
// ---------------------------------------------------------------------------
void JT9::interleave(const uint8_t s[206], uint8_t d[206])
{
    for (uint8_t i = 0; i < CONV_BITS; i++) {
        d[INTERLEAVE_TABLE[i]] = s[i];
    }
}

// ---------------------------------------------------------------------------
// Private — Stage 4: pack bits into 3-bit symbols and apply Gray code
// ---------------------------------------------------------------------------
void JT9::packAndGray(const uint8_t d[206], uint8_t g[69])
{
    uint8_t k = 0;
    for (uint8_t i = 0; i < DATA_SYMBOLS; i++) {
        const uint8_t sym = ((d[k] & 1u) << 2)
                          | ((d[k + 1] & 1u) << 1)
                          |  (d[k + 2] & 1u);
        k += 3;
        g[i] = grayCode(sym);
    }
}

// ---------------------------------------------------------------------------
// Private — Stage 5: merge with sync vector
// ---------------------------------------------------------------------------
void JT9::mergeSync(const uint8_t g[69], uint8_t symbols[SYMBOL_COUNT])
{
    uint8_t j = 0;
    for (uint8_t i = 0; i < SYMBOL_COUNT; i++) {
        symbols[i] = SYNC_VECTOR[i] ? 0 : g[j++] + 1;
    }
}
