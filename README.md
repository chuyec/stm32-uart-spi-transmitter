# UART-SPI Retranslator

This is the test project for the **UART-SPI Retranslator** module.

The module can be found in `components/uart-spi`.

## Environment

| Component    | Version | Note                             |
|--------------|---------|----------------------------------|
| STM32CubeIDE | 1.14.0  | With STM32CubeMX 6.10.0 included |
| STM32CubeG0  | 1.6.1   | HAL Firmware Package             |

## Key Features

1. The module retranslates all strings including null terminator
2. A strings can consist of all service, basic and extended ascii characters (1-255 values)
3. '\0' symbols are ignored on SPI MISO excluding string null terminator
4. A data retranslates from periphery to periphery as is without any modification and significant delay
5. Each peripheral has 1K input buffer that queue data
6. The FreeRTOS task is used for both UART and SPI peripheral operating
7. The module uses CMSIS-RTOS2 API as a wrapper over the FreeRTOS
8. Some specific FreeRTOS API is used

## How to use

Call `uart_spi_start()` function to run the module. A data will transmitted between UART and SPI periphery automatically.

``` c
    uart_spi_params_t uart_spi_params = {
        .huart = &huart1,
        .hspi = &hspi1
    };

    uart_spi_start(&uart_spi_params);
```
