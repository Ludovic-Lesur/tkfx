/*
 * stm32l0xx_drivers_flags.h
 *
 *  Created on: 11 aug. 2024
 *      Author: Ludo
 */

#ifndef __STM32L0XX_DRIVERS_FLAGS_H__
#define __STM32L0XX_DRIVERS_FLAGS_H__

/*** STM32L0xx drivers compilation flags ***/

#define STM32L0XX_DRIVERS_DMA_CHANNEL_MASK              0x00

#ifdef HW1_0
#define STM32L0XX_DRIVERS_EXTI_GPIO_MASK                0x1002
#endif
#ifdef HW1_1
#define STM32L0XX_DRIVERS_EXTI_GPIO_MASK                0x1001
#endif

//#define STM32L0XX_DRIVERS_I2C_FAST_MODE

#define STM32L0XX_DRIVERS_LPUART_MODE                   0
//#define STM32L0XX_DRIVERS_LPUART_DISABLE_TX_0

//#define STM32L0XX_DRIVERS_RCC_HSE_ENABLE
#define STM32L0XX_DRIVERS_RCC_HSE_FREQUENCY_HZ          16000000
#define STM32L0XX_DRIVERS_RCC_LSE_MODE                  1
#define STM32L0XX_DRIVERS_RCC_LSE_FREQUENCY_HZ          32768

#define STM32L0XX_DRIVERS_RTC_WAKEUP_PERIOD_SECONDS     10
#define STM32L0XX_DRIVERS_RTC_ALARM_MASK                0x00

#define STM32L0XX_DRIVERS_TIM_MODE_MASK                 0x06

#define STM32L0XX_DRIVERS_USART_MODE                    0
#define STM32L0XX_DRIVERS_USART_DISABLE_TX_0

#endif /* __STM32L0XX_DRIVERS_FLAGS_H__ */
