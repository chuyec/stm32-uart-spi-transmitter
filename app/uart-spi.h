/*
 * uart-spi.h
 *
 *  Created on: Dec 13, 2023
 *      Author: chuyec
 */

#ifndef UART_SPI_H_
#define UART_SPI_H_

#include "usart.h"
#include "spi.h"

// ============================================================================

typedef struct {
    UART_HandleTypeDef *huart;
    SPI_HandleTypeDef *hspi;
} uart_spi_params_t;

// ============================================================================

int uart_spi_start(uart_spi_params_t *params);

#endif /* UART_SPI_H_ */
