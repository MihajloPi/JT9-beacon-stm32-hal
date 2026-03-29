#pragma once

#include "tft_fonts.h"
#include "main.h"
#include <cstdint>
#include <cstddef>


#define USE_DMA

/* -----------------------------------------------------------------------
 * RGB565 colour constants
 * --------------------------------------------------------------------- */
namespace Color {
    constexpr uint16_t WHITE      = 0xFFFF;
    constexpr uint16_t BLACK      = 0x0000;
    constexpr uint16_t BLUE       = 0x001F;
    constexpr uint16_t RED        = 0xF800;
    constexpr uint16_t MAGENTA    = 0xF81F;
    constexpr uint16_t GREEN      = 0x07E0;
    constexpr uint16_t CYAN       = 0x07FF;   // fixed: was 0x7FFF
    constexpr uint16_t YELLOW     = 0xFFE0;
    constexpr uint16_t GRAY       = 0x8430;
    constexpr uint16_t BROWN      = 0xBC40;
    constexpr uint16_t DARKBLUE   = 0x01CF;
    constexpr uint16_t LIGHTBLUE  = 0x7D7C;
    constexpr uint16_t GRAYBLUE   = 0x5458;
    constexpr uint16_t LIGHTGREEN = 0x07E4;   // fixed: was lavender 0x841F
    constexpr uint16_t LGRAY      = 0xC618;
    constexpr uint16_t LGRAYBLUE  = 0x5D1C;   // fixed: was sage green 0xA651
    constexpr uint16_t LBBLUE     = 0x2B12;
}

/* -----------------------------------------------------------------------
 * ST7789 TFT driver
 *
 * Usage:
 *   ST7789 tft(&hspi1,
 *              ST7789_RST_GPIO_Port, ST7789_RST_Pin,
 *              ST7789_DC_GPIO_Port,  ST7789_DC_Pin,
 *              ST7789_CS_GPIO_Port,  ST7789_CS_Pin,  // nullptr/0 if no CS
 *              240, 320);
 *   tft.Init();
 * --------------------------------------------------------------------- */
class ST7789 {
public:
#ifdef USE_DMA
    /* DMA: minimum transfer size to bother using DMA over blocking SPI  */
    static constexpr uint16_t DMA_MIN_SIZE = 16;

#endif

    /* DMA framebuffer strip height — increase if you have spare RAM      */
    static constexpr uint16_t HOR_LEN = 5;

    ST7789(SPI_HandleTypeDef *spi,
           GPIO_TypeDef *rstPort, uint16_t rstPin,
           GPIO_TypeDef *dcPort,  uint16_t dcPin,
           GPIO_TypeDef *csPort,  uint16_t csPin,   // pass nullptr/0 for no CS
           uint16_t width, uint16_t height,
           uint16_t xShift = 0, uint16_t yShift = 0);

    /* Core */
    void Init();
    void SetRotation(uint8_t m);

    /* Basic drawing */
    void FillColor(uint16_t color);
    void DrawPixel(uint16_t x, uint16_t y, uint16_t color);
    void DrawPixel4px(uint16_t x, uint16_t y, uint16_t color);
    void Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color);

    /* Primitives */
    void DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
    void DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    void DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                      uint16_t x3, uint16_t y3, uint16_t color);
    void DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                            uint16_t x3, uint16_t y3, uint16_t color);

    /* Image & text */
    void DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
    void WriteChar(uint16_t x, uint16_t y, char ch,
                   TFT_FontDef font, uint16_t color, uint16_t bgcolor);
    void WriteString(uint16_t x, uint16_t y, const char *str,
                     TFT_FontDef font, uint16_t color, uint16_t bgcolor);

    /* Misc */
    void InvertColors(bool invert);
    void TearEffect(bool enable);
    void Test();

    uint16_t Width()  const { return _width;  }
    uint16_t Height() const { return _height; }

private:
    SPI_HandleTypeDef *_spi;
    GPIO_TypeDef      *_rstPort;  uint16_t _rstPin;
    GPIO_TypeDef      *_dcPort;   uint16_t _dcPin;
    GPIO_TypeDef      *_csPort;   uint16_t _csPin;   // nullptr = no CS
    uint16_t _width, _height, _xShift, _yShift;

    /* DMA framebuffer — HOR_LEN rows of pixels */
    uint16_t _dispBuf[240 * HOR_LEN];   // sized for max supported width

    inline void Select()   const;
    inline void UnSelect() const;
    inline void DC_Set()   const;
    inline void DC_Clr()   const;
    inline void RST_Set()  const;
    inline void RST_Clr()  const;

    void WriteCommand(uint8_t cmd);
    void WriteData(const uint8_t *buff, size_t size);
    void WriteSmallData(uint8_t data);
    void SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    /* Byte-swap a RGB565 value for correct DMA wire order */
    static constexpr uint16_t Swap16(uint16_t x) {
        return static_cast<uint16_t>((x << 8) | (x >> 8));
    }
    static constexpr int16_t Abs(int16_t x) { return x >= 0 ? x : -x; }
};
