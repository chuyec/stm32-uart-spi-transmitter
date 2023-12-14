/*
 * app.c
 *
 *  Created on: Dec 13, 2023
 *      Author: chuyec
 */

#include "uart-spi.h"

#include "cmsis_os.h"

// ============================================================================

void app_task(void *argument)
{
    uart_spi_params_t uart_spi_params = {
        .huart = &huart1,
        .hspi = &hspi1
    };

    uart_spi_start(&uart_spi_params);

    for(;;)
    {
        osDelay(1000);
    }
}
