#include "st7789.h"
#include <algorithm>

/* =========================================================================
 * Register map (local to this translation unit)
 * ======================================================================= */
namespace {
    constexpr uint8_t REG_COLMOD  = 0x3A;
    constexpr uint8_t REG_CASET   = 0x2A;
    constexpr uint8_t REG_RASET   = 0x2B;
    constexpr uint8_t REG_RAMWR   = 0x2C;
    constexpr uint8_t REG_MADCTL  = 0x36;
    constexpr uint8_t REG_INVOFF  = 0x20;
    constexpr uint8_t REG_INVON   = 0x21;
    constexpr uint8_t REG_SLPOUT  = 0x11;
    constexpr uint8_t REG_NORON   = 0x13;
    constexpr uint8_t REG_DISPON  = 0x29;
    constexpr uint8_t REG_TEON    = 0x35;
    constexpr uint8_t REG_TEOFF   = 0x34;

    constexpr uint8_t MADCTL_MY   = 0x80;
    constexpr uint8_t MADCTL_MX   = 0x40;
    constexpr uint8_t MADCTL_MV   = 0x20;
    constexpr uint8_t MADCTL_RGB  = 0x00;

    constexpr uint8_t COLOR_MODE_16BIT = 0x55;
}

/* =========================================================================
 * Constructor
 * ======================================================================= */
ST7789::ST7789(SPI_HandleTypeDef *spi,
               GPIO_TypeDef *rstPort, uint16_t rstPin,
               GPIO_TypeDef *dcPort,  uint16_t dcPin,
               GPIO_TypeDef *csPort,  uint16_t csPin,
               uint16_t width, uint16_t height,
               uint16_t xShift, uint16_t yShift)
    : _spi(spi),
      _rstPort(rstPort), _rstPin(rstPin),
      _dcPort(dcPort),   _dcPin(dcPin),
      _csPort(csPort),   _csPin(csPin),
      _width(width), _height(height),
      _xShift(xShift), _yShift(yShift),
      _dispBuf{}
{}

/* =========================================================================
 * GPIO helpers
 * ======================================================================= */
inline void ST7789::Select()   const { if (_csPort)  HAL_GPIO_WritePin(_csPort,  _csPin,  GPIO_PIN_RESET); }
inline void ST7789::UnSelect() const { if (_csPort)  HAL_GPIO_WritePin(_csPort,  _csPin,  GPIO_PIN_SET);   }
inline void ST7789::DC_Set()   const { HAL_GPIO_WritePin(_dcPort,  _dcPin,  GPIO_PIN_SET);   }
inline void ST7789::DC_Clr()   const { HAL_GPIO_WritePin(_dcPort,  _dcPin,  GPIO_PIN_RESET); }
inline void ST7789::RST_Set()  const { HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_SET);   }
inline void ST7789::RST_Clr()  const { HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_RESET); }

/* =========================================================================
 * Low-level SPI
 * ======================================================================= */
void ST7789::WriteCommand(uint8_t cmd)
{
    Select();
    DC_Clr();
    HAL_SPI_Transmit(_spi, &cmd, 1, HAL_MAX_DELAY);
    UnSelect();
}

void ST7789::WriteData(const uint8_t *buff, size_t size)
{
    Select();
    DC_Set();
    while (size > 0) {
        uint16_t chunk = static_cast<uint16_t>(size > 65535u ? 65535u : size);
#ifdef USE_DMA
        if (chunk >= DMA_MIN_SIZE) {
            HAL_SPI_Transmit_DMA(_spi, const_cast<uint8_t *>(buff), chunk);
            while (_spi->hdmatx->State != HAL_DMA_STATE_READY) {}
        } else {
            HAL_SPI_Transmit(_spi, const_cast<uint8_t *>(buff), chunk, HAL_MAX_DELAY);
        }
#else
        HAL_SPI_Transmit(_spi, const_cast<uint8_t *>(buff), chunk, HAL_MAX_DELAY);
#endif
        buff += chunk;
        size -= chunk;
    }
    UnSelect();
}

void ST7789::WriteSmallData(uint8_t data)
{
    Select();
    DC_Set();
    HAL_SPI_Transmit(_spi, &data, 1, HAL_MAX_DELAY);
    UnSelect();
}

/* =========================================================================
 * Address window
 * ======================================================================= */
void ST7789::SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    const uint16_t xs = x0 + _xShift, xe = x1 + _xShift;
    const uint16_t ys = y0 + _yShift, ye = y1 + _yShift;

    WriteCommand(REG_CASET);
    { uint8_t d[] = { uint8_t(xs>>8), uint8_t(xs), uint8_t(xe>>8), uint8_t(xe) };
      WriteData(d, sizeof(d)); }

    WriteCommand(REG_RASET);
    { uint8_t d[] = { uint8_t(ys>>8), uint8_t(ys), uint8_t(ye>>8), uint8_t(ye) };
      WriteData(d, sizeof(d)); }

    WriteCommand(REG_RAMWR);
}

/* =========================================================================
 * Init & rotation
 * ======================================================================= */
void ST7789::Init()
{
    std::fill(_dispBuf, _dispBuf + _width * HOR_LEN, uint16_t(0));

    HAL_Delay(10);
    RST_Clr();
    HAL_Delay(10);
    RST_Set();
    HAL_Delay(20);

    WriteCommand(REG_COLMOD);   WriteSmallData(COLOR_MODE_16BIT);

    WriteCommand(0xB2);         // Porch control
    { uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; WriteData(d, sizeof(d)); }

    SetRotation(2);

    WriteCommand(0xB7);  WriteSmallData(0x35);  // Gate control
    WriteCommand(0xBB);  WriteSmallData(0x19);  // VCOM
    WriteCommand(0xC0);  WriteSmallData(0x2C);  // LCMCTRL
    WriteCommand(0xC2);  WriteSmallData(0x01);  // VDV/VRH enable
    WriteCommand(0xC3);  WriteSmallData(0x12);  // VRH
    WriteCommand(0xC4);  WriteSmallData(0x20);  // VDV
    WriteCommand(0xC6);  WriteSmallData(0x0F);  // Frame rate 60 Hz
    WriteCommand(0xD0);  WriteSmallData(0xA4);  // Power control
                         WriteSmallData(0xA1);

    WriteCommand(0xE0);  // Positive gamma
    { uint8_t d[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
      WriteData(d, sizeof(d)); }

    WriteCommand(0xE1);  // Negative gamma
    { uint8_t d[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
      WriteData(d, sizeof(d)); }

    WriteCommand(REG_INVOFF);   // Inversion OFF — correct for this panel
    WriteCommand(REG_SLPOUT);
    WriteCommand(REG_NORON);
    WriteCommand(REG_DISPON);

    HAL_Delay(50);
    FillColor(Color::BLACK);
}

void ST7789::SetRotation(uint8_t m)
{
    WriteCommand(REG_MADCTL);
    switch (m) {
        case 0: WriteSmallData(MADCTL_MX | MADCTL_MY | MADCTL_RGB); break;
        case 1: WriteSmallData(MADCTL_MY | MADCTL_MV  | MADCTL_RGB); break;
        case 2: WriteSmallData(MADCTL_RGB);                           break;
        case 3: WriteSmallData(MADCTL_MX | MADCTL_MV  | MADCTL_RGB); break;
        default: break;
    }
}

/* =========================================================================
 * Basic drawing
 * ======================================================================= */
void ST7789::FillColor(uint16_t color)
{
    SetAddressWindow(0, 0, _width - 1, _height - 1);
    Select();

#ifdef USE_DMA
    /* Byte-swap before storing so DMA sends bytes in the correct wire order */
    const uint16_t swapped = Swap16(color);
    std::fill_n(_dispBuf, _width * HOR_LEN, swapped);

    for (uint16_t i = 0; i < _height / HOR_LEN; i++)
        WriteData(reinterpret_cast<const uint8_t *>(_dispBuf),
                  _width * HOR_LEN * sizeof(uint16_t));
#else
    uint8_t data[2] = { uint8_t(color >> 8), uint8_t(color) };
    for (uint32_t i = 0; i < uint32_t(_width) * _height; i++)
        HAL_SPI_Transmit(_spi, data, sizeof(data), HAL_MAX_DELAY);
#endif

    UnSelect();
}

void ST7789::DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= _width || y >= _height) return;
    SetAddressWindow(x, y, x, y);
    uint8_t data[] = { uint8_t(color >> 8), uint8_t(color) };
    WriteData(data, sizeof(data));
}

void ST7789::DrawPixel4px(uint16_t x, uint16_t y, uint16_t color)
{
    if (x == 0 || x > _width || y == 0 || y > _height) return;
    Fill(x - 1, y - 1, x + 1, y + 1, color);
}

void ST7789::Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    if (xEnd >= _width || yEnd >= _height) return;
    SetAddressWindow(xSta, ySta, xEnd, yEnd);
    uint8_t data[] = { uint8_t(color >> 8), uint8_t(color) };
    Select();
    for (uint16_t i = ySta; i <= yEnd; i++)
        for (uint16_t j = xSta; j <= xEnd; j++)
            HAL_SPI_Transmit(_spi, data, sizeof(data), HAL_MAX_DELAY);
    UnSelect();
}

/* =========================================================================
 * Lines & rectangles
 * ======================================================================= */
void ST7789::DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    auto swap = [](uint16_t &a, uint16_t &b) { uint16_t t = a; a = b; b = t; };

    bool steep = Abs(int16_t(y1-y0)) > Abs(int16_t(x1-x0));
    if (steep) { swap(x0, y0); swap(x1, y1); }
    if (x0 > x1) { swap(x0, x1); swap(y0, y1); }

    int16_t dx    = int16_t(x1 - x0);
    int16_t dy    = Abs(int16_t(y1 - y0));
    int16_t err   = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;
    int16_t y     = int16_t(y0);

    for (int16_t x = int16_t(x0); x <= int16_t(x1); x++) {
        if (steep) DrawPixel(uint16_t(y), uint16_t(x), color);
        else        DrawPixel(uint16_t(x), uint16_t(y), color);
        err -= dy;
        if (err < 0) { y += ystep; err += dx; }
    }
}

void ST7789::DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    DrawLine(x1, y1, x2, y1, color);
    DrawLine(x1, y1, x1, y2, color);
    DrawLine(x1, y2, x2, y2, color);
    DrawLine(x2, y1, x2, y2, color);
}

void ST7789::DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= _width || y >= _height) return;
    if (x + w >= _width)  w = _width  - x;
    if (y + h >= _height) h = _height - y;
    for (uint16_t i = 0; i <= h; i++)
        DrawLine(x, y + i, x + w, y + i, color);
}

/* =========================================================================
 * Circles
 * ======================================================================= */
void ST7789::DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    DrawPixel(x0,     y0 + r, color);
    DrawPixel(x0,     y0 - r, color);
    DrawPixel(x0 + r, y0,     color);
    DrawPixel(x0 - r, y0,     color);
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        DrawPixel(x0+x, y0+y, color); DrawPixel(x0-x, y0+y, color);
        DrawPixel(x0+x, y0-y, color); DrawPixel(x0-x, y0-y, color);
        DrawPixel(x0+y, y0+x, color); DrawPixel(x0-y, y0+x, color);
        DrawPixel(x0+y, y0-x, color); DrawPixel(x0-y, y0-x, color);
    }
}

void ST7789::DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    DrawPixel(uint16_t(x0), uint16_t(y0+r), color);
    DrawPixel(uint16_t(x0), uint16_t(y0-r), color);
    DrawPixel(uint16_t(x0+r), uint16_t(y0), color);
    DrawPixel(uint16_t(x0-r), uint16_t(y0), color);
    DrawLine(uint16_t(x0-r), uint16_t(y0), uint16_t(x0+r), uint16_t(y0), color);
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        DrawLine(uint16_t(x0-x), uint16_t(y0+y), uint16_t(x0+x), uint16_t(y0+y), color);
        DrawLine(uint16_t(x0+x), uint16_t(y0-y), uint16_t(x0-x), uint16_t(y0-y), color);
        DrawLine(uint16_t(x0+y), uint16_t(y0+x), uint16_t(x0-y), uint16_t(y0+x), color);
        DrawLine(uint16_t(x0+y), uint16_t(y0-x), uint16_t(x0-y), uint16_t(y0-x), color);
    }
}

/* =========================================================================
 * Triangles
 * ======================================================================= */
void ST7789::DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                           uint16_t x3, uint16_t y3, uint16_t color)
{
    DrawLine(x1,y1,x2,y2,color);
    DrawLine(x2,y2,x3,y3,color);
    DrawLine(x3,y3,x1,y1,color);
}

void ST7789::DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 uint16_t x3, uint16_t y3, uint16_t color)
{
    int16_t dx = Abs(int16_t(x2-x1)), dy = Abs(int16_t(y2-y1));
    int16_t x = int16_t(x1), y = int16_t(y1);
    int16_t xi1 = (x2>=x1)?1:-1, xi2 = xi1;
    int16_t yi1 = (y2>=y1)?1:-1, yi2 = yi1;

    if (dx >= dy) { xi1=0; yi2=0; }
    else          { xi2=0; yi1=0; int16_t temp = dx; dx = dy; dy = temp; }

    int16_t num = dx/2, pixels = dx;
    for (int16_t p = 0; p <= pixels; p++) {
        DrawLine(uint16_t(x), uint16_t(y), x3, y3, color);
        num += dy;
        if (num >= dx) { num -= dx; x += xi1; y += yi1; }
        x += xi2; y += yi2;
    }
}

/* =========================================================================
 * Image & text
 * ======================================================================= */
void ST7789::DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const uint16_t *data)
{
    if (x + w > _width || y + h > _height) return;
    SetAddressWindow(x, y, x + w - 1, y + h - 1);
    WriteData(reinterpret_cast<const uint8_t *>(data),
              static_cast<size_t>(w) * h * sizeof(uint16_t));
}

void ST7789::WriteChar(uint16_t x, uint16_t y, char ch,
                        TFT_FontDef font, uint16_t color, uint16_t bgcolor)
{
    SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);
    for (uint32_t row = 0; row < font.height; row++) {
        uint16_t bits = font.data[(uint8_t(ch) - 32) * font.height + row];
        for (uint32_t col = 0; col < font.width; col++) {
            uint16_t c = ((bits << col) & 0x8000) ? color : bgcolor;
            uint8_t d[] = { uint8_t(c >> 8), uint8_t(c) };
            WriteData(d, sizeof(d));
        }
    }
}

void ST7789::WriteString(uint16_t x, uint16_t y, const char *str,
                          TFT_FontDef font, uint16_t color, uint16_t bgcolor)
{
    while (*str) {
        if (x + font.width >= _width) {
            x  = 0;
            y += font.height;
            if (y + font.height >= _height) break;
            if (*str == ' ') { str++; continue; }
        }
        WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}

/* =========================================================================
 * Misc
 * ======================================================================= */
void ST7789::InvertColors(bool invert)
{
    WriteCommand(invert ? REG_INVON : REG_INVOFF);
}

void ST7789::TearEffect(bool enable)
{
    WriteCommand(enable ? REG_TEON : REG_TEOFF);
}

/* =========================================================================
 * Test
 * ======================================================================= */
void ST7789::Test()
{
    FillColor(Color::WHITE);   HAL_Delay(1000);
    WriteString(10, 20, "Speed Test", TFT_Font_11x18, Color::RED, Color::WHITE);
    HAL_Delay(1000);

    const uint16_t fills[] = {
        Color::CYAN, Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW,
        Color::BROWN, Color::DARKBLUE, Color::MAGENTA, Color::LIGHTGREEN,
        Color::LGRAY, Color::LBBLUE
    };
    for (uint16_t c : fills) { FillColor(c); HAL_Delay(500); }

    FillColor(Color::WHITE);
    WriteString(10, 10,  "Font test.",   TFT_Font_16x26, Color::CYAN,    Color::WHITE);
    WriteString(10, 50,  "Hello World!", TFT_Font_7x10,  Color::RED,     Color::WHITE);
    WriteString(10, 75,  "Hello World!", TFT_Font_11x18, Color::YELLOW,  Color::WHITE);
    WriteString(10, 100, "Hello World!", TFT_Font_16x26, Color::MAGENTA, Color::WHITE);
    HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Rect./Line.",  TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawRectangle(30,30,100,100, Color::WHITE);    HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Filled Rect.", TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawFilledRectangle(30,30,50,50, Color::WHITE);  HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Circle.",      TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawCircle(60,60,25, Color::WHITE);              HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Filled Cir.",  TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawFilledCircle(60,60,25, Color::WHITE);         HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Triangle",     TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawTriangle(30,30,30,70,60,40, Color::WHITE);   HAL_Delay(1000);

    FillColor(Color::RED);
    WriteString(10,10,"Filled Tri",   TFT_Font_11x18, Color::YELLOW, Color::BLACK);
    DrawFilledTriangle(30,30,30,70,60,40, Color::WHITE); HAL_Delay(1000);

    FillColor(Color::WHITE);
    DrawImage(0, 0, 128, 128, reinterpret_cast<const uint16_t *>(saber));
    HAL_Delay(3000);
}
