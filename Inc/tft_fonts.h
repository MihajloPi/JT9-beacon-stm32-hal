#ifndef __TFT_FONT_H
#define __TFT_FONT_H

#include "stdint.h"

typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} TFT_FontDef;

//Font lib.
extern TFT_FontDef TFT_Font_7x10;
extern TFT_FontDef TFT_Font_11x18;
extern TFT_FontDef TFT_Font_16x26;

//16-bit(RGB565) Image lib.
/*******************************************
 *             CAUTION:
 *   If the MCU onchip flash cannot
 *  store such huge image data,please
 *           do not use it.
 * These pics are for test purpose only.
 *******************************************/

/* 128x128 pixel RGB565 image */
extern const uint16_t saber[][128];

/* 240x240 pixel RGB565 image 
extern const uint16_t knky[][240];
extern const uint16_t tek[][240];
extern const uint16_t adi1[][240];
*/
#endif
