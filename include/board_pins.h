#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32l4xx_hal.h"

#define RGB_DATA_PORT   GPIOA
#define RGB_DATA_PIN    GPIO_PIN_8

#define RGB_CLK_PORT    GPIOB
#define RGB_CLK_PIN     GPIO_PIN_10

#define LIGHT_ADC_CHANNEL  ADC_CHANNEL_9
#define LIGHT_GPIO_PORT    GPIOA
#define LIGHT_GPIO_PIN     GPIO_PIN_4

#define BUTTON1_PORT GPIOA
#define BUTTON1_PIN  GPIO_PIN_10

#define BUTTON2_PORT GPIOB
#define BUTTON2_PIN  GPIO_PIN_3

#define GROVE_I2C_HANDLE hi2c1

#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_5

#define SD_SPI_HANDLE hspi1

#endif
