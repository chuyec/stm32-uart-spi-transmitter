/**
 * @file app.c
 * @author Denis Shreiber (chuyecd@gmail.com)
 * @date 2023-12-13
 */

#include "cmsis_os.h"

#include "uart-spi.h"

// ============================================================================

static int app_init(void);

// ============================================================================

void app_task(void *argument)
{
    app_init();

    for(;;)
    {
        osDelay(1000);
    }
}

// ============================================================================

static int app_init(void)
{
    int result;

    uart_spi_params_t uart_spi_params = {
        .huart = &huart1,
        .hspi = &hspi1
    };

    result = uart_spi_start(&uart_spi_params);

    return result;
}
