#include <cstdio>

#define BEACON_TIM_PRESCALER  10000U  // PSC = 9999
#define BEACON_TIM_PERIOD     5764U   // ARR = 5763

                        // User C++ libraries
#include "si5351.h"
#include "RotaryEncoder.h"
#include "Beacon.h"
#include "st7789.h"
#include "ssd1306.h"
#include "tft_fonts.h"
#include "TinyGPSPlus.h"
#include "TinyGPSPlus_UART.h"
#include "AD9850.h"

extern "C" {
                        // HAL libraries
    #include "main.h"
    #include "i2c.h"
	#include "spi.h"
    #include "tim.h"
    #include "usart.h"
    #include "gpio.h"
                        // Font definitions (C header, no name conflicts)
    #include "fonts.h"
}

/* -------------------------------------------------------------------------
 * Hardware object instantiation
 * ---------------------------------------------------------------------- */
Si5351 si5351(&hi2c1);
AD9850 dds(AD9850_CLOCK_GPIO_Port, AD9850_CLOCK_Pin, AD9850_RESET_Pin, AD9850_LOAD_Pin, AD9850_DATA_Pin);
TinyGPSPlusUART_Polling gpsUART(huart1);

RotaryEncoder encoder(
    ENCODER_CLOCK_GPIO_Port, ENCODER_CLOCK_Pin,
    ENCODER_DATA_GPIO_Port,  ENCODER_DATA_Pin,
    ENCODER_SWITCH_GPIO_Port, ENCODER_SWITCH_Pin
);

//Beacon beacon(si5351, &htim2, GPIOC, GPIO_PIN_13);
Beacon beacon(dds, &htim2, GPIOC, GPIO_PIN_13);

ST7789 tft(
    &hspi1,
    ST7789_RST_GPIO_Port, ST7789_RST_Pin,
    ST7789_DC_GPIO_Port,  ST7789_DC_Pin,
    ST7789_CS_GPIO_Port,  ST7789_CS_Pin,
    240, 320                    // USING_240X320, no shift needed
);

SSD1306 oled(hi2c1);

/* -------------------------------------------------------------------------
 * HAL callback — must remain extern "C"
 * ---------------------------------------------------------------------- */
extern "C" {
    void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
        beacon.notifySymbolClock(htim);
    }
}

/* -------------------------------------------------------------------------
 * Application globals
 * ---------------------------------------------------------------------- */
static char buffer[30] = "";
static char wspr_poruka[] = "YT1GS KN03";

static constexpr uint64_t TX_FREQ_HZ = 28079600ULL;
static constexpr uint32_t XO_FREQ_HZ = 0;
static constexpr int32_t  CORRECTION  = 0;

static uint64_t frequency                    = static_cast<uint64_t>(14.074e6);
static uint32_t update_display_time          = 100;
static uint32_t previous_update_display_time = 0;

static double latitude = 0;
static double longitude = 0;

/* -------------------------------------------------------------------------
 * User defined functions
 * ---------------------------------------------------------------------- */

static void formatFrequency(uint64_t hz);
static void updateDisplays(void);

/* -------------------------------------------------------------------------
 * Main application entry point (called from main.c)
 * ---------------------------------------------------------------------- */
int mainCpp()
{
    /* --- Encoder setup ------------------------------------------------- */
    encoder.begin();
    encoder
        .onRotate([](RotaryDirection dir, int32_t /*count*/) {
            frequency += static_cast<int64_t>(static_cast<int8_t>(dir)) * 1000;
        })
        .onPress([]() {
            encoder.resetCounter(0);
            frequency = static_cast<uint64_t>(14.074e6);
        });

    /* --- Display init -------------------------------------------------- */
    oled.Init();
    oled.Fill(SSD1306::Color::Black);

    tft.Init();
    tft.SetRotation(2);
    tft.FillColor(Color::BLACK);

    /* --- Initial screen content ---------------------------------------- */
    formatFrequency(TX_FREQ_HZ);

    oled.SetCursor(0, 36);
    oled.WriteString(wspr_poruka, Font_11x18, SSD1306::Color::White);
    tft.WriteString(0, 52, wspr_poruka, TFT_Font_16x26, Color::BLUE, Color::BLACK);
    updateDisplays();

    /* --- Si5351 + beacon init ------------------------------------------ */
    si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0);
    si5351.driveStrength(SI5351_CLK0, SI5351_DRIVE_8MA);
    si5351.outputEnable(SI5351_CLK0, 0);

    /* --- AD9850 DDS init ----------------------------------------------- */
    dds.begin();
    dds.outputEnable(true);

    beacon.init(TX_FREQ_HZ, XO_FREQ_HZ, CORRECTION);
    beacon.transmit(wspr_poruka);

    /* --- Clear displays after TX --------------------------------------- */
    oled.Fill(SSD1306::Color::Black);
    oled.UpdateScreen();
    tft.FillColor(Color::BLACK);

    /* --- Main loop ----------------------------------------------------- */
    while (true) {
        encoder.update();
        gpsUART.update();
        TinyGPSPlus &gps = gpsUART.gps;

        if (HAL_GetTick() - previous_update_display_time > update_display_time) {
        	if (gps.location.isUpdated() && gps.location.isValid()) {
        		latitude = gps.location.lat();
        	    longitude = gps.location.lng();
        	}

            formatFrequency(frequency);
            updateDisplays();

            si5351.setFreq(frequency * 100ULL, SI5351_CLK0);
            dds.setFrequency(static_cast<float>(frequency));
            dds.outputEnable(true);
            si5351.outputEnable(SI5351_CLK0, 0);

            previous_update_display_time = HAL_GetTick();
        }
    }
}


/* -------------------------------------------------------------------------
 * Helper — split Hz into MHz + kHz parts and format into buffer
 * ---------------------------------------------------------------------- */
static void formatFrequency(uint64_t hz)
{
    const int mhz = static_cast<int>(hz / 1'000'000ULL);
    const int khz = static_cast<int>((hz - mhz * 1'000'000ULL) / 1'000ULL);
    snprintf(buffer, sizeof(buffer), "%d.%03d MHz", mhz, khz);
}

/* -------------------------------------------------------------------------
 * Helper — update both displays with current frequency string
 * ---------------------------------------------------------------------- */
static void updateDisplays(void)
{
    // SSD1306
	oled.Fill(SSD1306::Color::Black);
    oled.SetCursor(0, 0);
    oled.WriteString("Frequency: ", Font_11x18, SSD1306::Color::White);
    oled.SetCursor(0, 18);
    oled.WriteString(buffer, Font_11x18, SSD1306::Color::White);
    oled.UpdateScreen();

    // ST7789
    tft.FillColor(Color::BLACK);
    tft.WriteString(0,  0, "Frequency: ", TFT_Font_16x26, Color::RED,   Color::BLACK);
    tft.WriteString(0, 26, buffer,        TFT_Font_16x26, Color::GREEN, Color::BLACK);
}
