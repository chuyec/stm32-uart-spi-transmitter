/**
 * @file uart-spi.h
 * @author Denis Shreiber (chuyecd@gmail.com)
 * @date 2023-12-13
 */

#ifndef UART_SPI_H_
#define UART_SPI_H_

#include "usart.h"
#include "spi.h"

// ============================================================================

/**
 * @brief \c uart-spi module parameters structure
 * 
 */
typedef struct {
    UART_HandleTypeDef *huart;  /// The pointer to the HAL UART handle
    SPI_HandleTypeDef *hspi;    /// The pointer to the HAL SPI handle
} uart_spi_params_t;

// ============================================================================

/**
 * @brief Start the \c uart-spi retranslator module
 * 
 * @param params The pointer to the \ref uart_spi_params_t structure
 * @return 0 - on success, -1 - on error
 * 
 * @note The UART and SPI peripherals pointed to the \c params argument
 * must be already initialized before call this function
 */
int uart_spi_start(uart_spi_params_t *params);

#endif /* UART_SPI_H_ */
