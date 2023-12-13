/*
 * uart-spi.c
 *
 *  Created on: Dec 13, 2023
 *      Author: chuyec
 */

#include "uart-spi.h"

#include "cmsis_os.h"
#include "stream_buffer.h"

#include <assert.h>

// ============================================================================

#define CHUNK_BUFF_SIZE            128

// ============================================================================

static void uart_task(void *arg);
static void spi_task(void *arg);

static int uart_rx_start(void);
static int uart_tx(const void *data, size_t length);
static int uart_wait_tx_ready(uint32_t timeout_ms);
static void uart_tx_abort(void);
static void uart_tx_complete_callback(UART_HandleTypeDef *huart);
static void uart_rx_complete_callback(UART_HandleTypeDef *huart);
static void uart_error_callback(UART_HandleTypeDef *huart);

// ============================================================================

static UART_HandleTypeDef *huart = NULL;
static SPI_HandleTypeDef *hspi = NULL;

static osThreadId_t uart_task_handle = NULL;
static osThreadId_t spi_task_handle = NULL;

static StreamBufferHandle_t uart_rx_stream = NULL;
static StreamBufferHandle_t spi_rx_stream = NULL;

static osSemaphoreId_t uart_tx_sema = NULL;

static uint8_t rx_byte;

// ============================================================================

int uart_spi_start(uart_spi_params_t *params)
{
    assert(params);
    assert(params->huart);
    assert(params->hspi);

    huart = params->huart;
    hspi = params->hspi;

    // ----------------------

    HAL_StatusTypeDef status;

    status = HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID, uart_tx_complete_callback);
    assert(status == HAL_OK);

    status = HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID, uart_rx_complete_callback);
    assert(status == HAL_OK);

    status = HAL_UART_RegisterCallback(huart, HAL_UART_ERROR_CB_ID, uart_error_callback);
    assert(status == HAL_OK);

    // ----------------------

    uart_rx_stream = xStreamBufferCreate(1024, 1);
    assert(uart_rx_stream);

    // Semaphore is released at initial
    uart_tx_sema = osSemaphoreNew(1, 1, NULL);
    assert(uart_tx_sema);

    uart_task_handle = osThreadNew(uart_task, NULL, NULL);
    assert(uart_task_handle);

    // ----------------------

    spi_rx_stream = xStreamBufferCreate(1024, 1);
    assert(spi_rx_stream);

    spi_task_handle = osThreadNew(spi_task, NULL, NULL);
    assert(spi_task_handle);

    return 0;
}

// ============================================================================

static void uart_task(void *arg)
{
    static char chunk_buff[CHUNK_BUFF_SIZE];

    uart_rx_start();

    while (1) {
        size_t length = xStreamBufferReceive(spi_rx_stream, chunk_buff, CHUNK_BUFF_SIZE, portMAX_DELAY);
        if (length > 0) {
            if (uart_tx(chunk_buff, length) != HAL_OK) {
                // Error. Just continue;
                continue;
            }

            if (uart_wait_tx_ready(100) != 0) {
                // Abort ongoing transmitting in case of timeout
                uart_tx_abort();
            }
        }
    }
}

static void spi_task(void *arg)
{
//    static char chunk_buff[CHUNK_BUFF_SIZE];

    while (1) {

    }
}

// ----------------------------------------------------------------------------

static int uart_rx_start(void)
{
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(huart, &rx_byte, 1);

    return status == HAL_OK ? 0 : -1;
}

static int uart_tx(const void *data, size_t length)
{
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(huart, data, length);

    return status == HAL_OK ? 0 : -1;
}

static int uart_wait_tx_ready(uint32_t timeout_ms)
{
    osStatus_t status = osSemaphoreAcquire(uart_tx_sema, pdMS_TO_TICKS(timeout_ms));

    return status == osOK ? 0 : -1;
}

static void uart_tx_abort(void)
{
    HAL_UART_AbortTransmit_IT(huart);
}

static void uart_tx_complete_callback(UART_HandleTypeDef *huart)
{
    UNUSED(huart);

    osSemaphoreRelease(uart_tx_sema);
}

static void uart_rx_complete_callback(UART_HandleTypeDef *huart)
{
    xStreamBufferSend(uart_rx_stream, &rx_byte, 1, 0);

    uart_rx_start();
}

static void uart_error_callback(UART_HandleTypeDef *huart)
{
    UNUSED(huart);

    osSemaphoreRelease(uart_tx_sema);
}
