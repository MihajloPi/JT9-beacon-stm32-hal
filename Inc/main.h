/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
int mainCpp(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_ERROR_Pin GPIO_PIN_13
#define LED_ERROR_GPIO_Port GPIOC
#define AD9850_CLOCK_Pin GPIO_PIN_1
#define AD9850_CLOCK_GPIO_Port GPIOA
#define ST7789_DC_Pin GPIO_PIN_2
#define ST7789_DC_GPIO_Port GPIOA
#define ST7789_RST_Pin GPIO_PIN_3
#define ST7789_RST_GPIO_Port GPIOA
#define ST7789_CS_Pin GPIO_PIN_4
#define ST7789_CS_GPIO_Port GPIOA
#define DDS_CLK_Pin GPIO_PIN_0
#define DDS_CLK_GPIO_Port GPIOB
#define DDS_WD_Pin GPIO_PIN_1
#define DDS_WD_GPIO_Port GPIOB
#define DDS_DATA_Pin GPIO_PIN_2
#define DDS_DATA_GPIO_Port GPIOB
#define DDS_RESET_Pin GPIO_PIN_10
#define DDS_RESET_GPIO_Port GPIOB
#define ENCODER_SWITCH_Pin GPIO_PIN_15
#define ENCODER_SWITCH_GPIO_Port GPIOB
#define ENCODER_DATA_Pin GPIO_PIN_8
#define ENCODER_DATA_GPIO_Port GPIOA
#define ENCODER_CLOCK_Pin GPIO_PIN_9
#define ENCODER_CLOCK_GPIO_Port GPIOA
#define AD9850_RESET_Pin GPIO_PIN_11
#define AD9850_RESET_GPIO_Port GPIOA
#define AD9850_LOAD_Pin GPIO_PIN_12
#define AD9850_LOAD_GPIO_Port GPIOA
#define AD9850_DATA_Pin GPIO_PIN_13
#define AD9850_DATA_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
