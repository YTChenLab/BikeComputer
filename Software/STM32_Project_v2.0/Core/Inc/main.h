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
#include "stm32h7xx_hal.h"

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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_PGDN_Pin GPIO_PIN_13
#define KEY_PGDN_GPIO_Port GPIOC
#define LCD_RST_Pin GPIO_PIN_10
#define LCD_RST_GPIO_Port GPIOI
#define KEY_PWR_Pin GPIO_PIN_0
#define KEY_PWR_GPIO_Port GPIOA
#define BAT_CHG_N_Pin GPIO_PIN_4
#define BAT_CHG_N_GPIO_Port GPIOH
#define BAT_PGOOD_N_Pin GPIO_PIN_5
#define BAT_PGOOD_N_GPIO_Port GPIOH
#define LCD_SPI_CS_Pin GPIO_PIN_4
#define LCD_SPI_CS_GPIO_Port GPIOA
#define LCD_SPI_SCL_Pin GPIO_PIN_13
#define LCD_SPI_SCL_GPIO_Port GPIOB
#define KEY_BACK_Pin GPIO_PIN_11
#define KEY_BACK_GPIO_Port GPIOD
#define KEY_MENU_Pin GPIO_PIN_12
#define KEY_MENU_GPIO_Port GPIOD
#define KEY_PGUP_Pin GPIO_PIN_13
#define KEY_PGUP_GPIO_Port GPIOD
#define ENC_A_Pin GPIO_PIN_3
#define ENC_A_GPIO_Port GPIOG
#define ENC_A_EXTI_IRQn EXTI3_IRQn
#define ENC_B_Pin GPIO_PIN_4
#define ENC_B_GPIO_Port GPIOD
#define ENC_B_EXTI_IRQn EXTI4_IRQn
#define ENC_SW_Pin GPIO_PIN_12
#define ENC_SW_GPIO_Port GPIOG
#define LCD_SPI_SDA_Pin GPIO_PIN_5
#define LCD_SPI_SDA_GPIO_Port GPIOB
#define I2C_SDA_Pin GPIO_PIN_7
#define I2C_SDA_GPIO_Port GPIOB
#define I2C_SCL_Pin GPIO_PIN_8
#define I2C_SCL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
