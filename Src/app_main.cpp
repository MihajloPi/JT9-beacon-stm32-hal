#include <cstdio>

#define BEACON_TIM_PRESCALER  10000U  // PSC = 9999
#define BEACON_TIM_PERIOD     5764U   // ARR = 5763

						//User C++ libraries
#include "si5351.h"
#include "RotaryEncoder.h"
#include "Beacon.h"

extern "C" {
						//HAL libraries
	#include "main.h"
	#include "i2c.h"
	#include "tim.h"
	#include "usart.h"
	#include "gpio.h"
						//User C libraries
    #include "ssd1306.h"
	#include "st7789.h"
	#include "tft_fonts.h"
    #include "fonts.h"
}

Si5351 si5351(&hi2c1);
RotaryEncoder encoder(
  ENCODER_CLOCK_GPIO_Port, ENCODER_CLOCK_Pin,   // CLK
  ENCODER_DATA_GPIO_Port, ENCODER_DATA_Pin,   // DT
  ENCODER_SWITCH_GPIO_Port, ENCODER_SWITCH_Pin    // SW
  // Optional 4th arg: debounce window in ms, default is 10
);
Beacon beacon(si5351, &htim2, GPIOC, GPIO_PIN_13);

extern "C" {
	void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
		beacon.notifySymbolClock(htim);
	}
}

char buffer[30] = "";
char wspr_poruka[] = "YT1GS KN03"; // Up to 13 chars: A-Z 0-9 space + - . / ?
const uint64_t TX_FREQ_HZ  = 14078600ULL; // Dial frequency, Hz
const uint32_t XO_FREQ_HZ  = 0;           // 0 = use 25 MHz default
const int32_t  CORRECTION  = 0;            // Frequency correction, ppb
uint64_t frequency = 14.074e6; // Hz
uint32_t update_display_time = 100;
uint32_t previous_update_display_time = 0;

int mainCpp() {
	encoder.begin();
	encoder
	    .onRotate([](RotaryDirection dir, int32_t count) {
	      // Fired automatically inside update() on every detected step.
	      // dir   == RotaryDirection::CW (+1) or CCW (-1)
	      // count == current accumulated counter value
		frequency += static_cast<int8_t>(dir) * 1e3;
	    })
	    .onPress([]() {
	      // Fired on the falling edge of the switch (press, not hold).
	      // Chained directly on the same encoder instance (fluent interface).
	      encoder.resetCounter(0);
	      frequency = 14.074e6;
	    });

    ssd1306_Init(&hi2c1);
    ssd1306_Fill(Black);

    ST7789_Init();
    ST7789_SetRotation(2);
    ST7789_Fill_Color(WHITE);
    //ST7789_WriteString(0, 0, "Hello world!", TFT_Font_16x26, WHITE, BLACK);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Frequency: ", Font_11x18, White);
    ssd1306_SetCursor(0, 18);

    int mhz_part = (int)(TX_FREQ_HZ / 1e6);
    int khz_part = (int)((TX_FREQ_HZ - mhz_part * 1e6) / 1e3);

    snprintf(buffer, sizeof(buffer), "%d.%03d MHz", mhz_part, khz_part);

    ssd1306_WriteString(buffer, Font_11x18, White);
    ssd1306_SetCursor(0, 36);
    ssd1306_WriteString(wspr_poruka, Font_11x18, White);
    ssd1306_UpdateScreen(&hi2c1);

    ST7789_WriteString(0, 0, "Frequency: ", TFT_Font_16x26, RED, WHITE);
    ST7789_WriteString(0, 176, buffer, TFT_Font_16x26, GREEN, WHITE);
    ST7789_WriteString(16, 0, wspr_poruka, TFT_Font_16x26, GREEN, WHITE);

    si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0);
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    si5351.output_enable(SI5351_CLK0, 0);

    beacon.init(TX_FREQ_HZ, XO_FREQ_HZ, CORRECTION);
    beacon.transmit(wspr_poruka);

    ssd1306_Fill(Black);
    ST7789_Fill_Color(BLACK);

    while(1) {
        // Your logic
    	encoder.update();

    	if (HAL_GetTick() - previous_update_display_time > update_display_time) {
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("Frequency: ", Font_11x18, White);
            ssd1306_SetCursor(0, 18);

            int mhz_part = (int)(frequency / 1e6);
            int khz_part = (int)((frequency - mhz_part * 1e6) / 1e3);

            snprintf(buffer, sizeof(buffer), "%d.%03d MHz", mhz_part, khz_part);

            ssd1306_WriteString(buffer, Font_11x18, White);
            ssd1306_UpdateScreen(&hi2c1);

            ST7789_WriteString(0, 0, "Frequency: ", TFT_Font_16x26, RED, BLACK);
            ST7789_WriteString(176, 0, buffer, TFT_Font_16x26, GREEN, BLACK);

            si5351.set_freq(frequency * 1e2, SI5351_CLK0);
            //si5351.output_enable(SI5351_CLK0, 1);

    		previous_update_display_time = HAL_GetTick();
    	}
    }
}
