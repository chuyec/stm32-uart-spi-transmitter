/**
 * @file uart-spi.c
 * @author Denis Shreiber (chuyecd@gmail.com)
 * @date 2023-12-13
 */

#include "uart-spi.h"

#include "cmsis_os.h"
#include "stream_buffer.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================

#define CHUNK_BUFF_SIZE            128

// ============================================================================

static void uart_task(void *arg);
static void spi_task(void *arg);

static int uart_rx_start(void);
static int uart_tx_async(const void *data, size_t length);
static int uart_wait_tx_ready(uint32_t timeout_ms);
static void uart_tx_abort(void);
static void uart_tx_complete_callback(UART_HandleTypeDef *huart);
static void uart_rx_complete_callback(UART_HandleTypeDef *huart);
static void uart_error_callback(UART_HandleTypeDef *huart);

static int spi_tx_rx(const void *txd, void *rxd, size_t length);
static int spi_wait_ready(uint32_t timeout_ms);
static void spi_abort(void);
static void spi_tx_rx_complete_callback(SPI_HandleTypeDef *hspi);
static void spi_error_callback(SPI_HandleTypeDef *hspi);

// ============================================================================

static UART_HandleTypeDef *huart = NULL;
static SPI_HandleTypeDef *hspi = NULL;

static osThreadId_t uart_task_handle = NULL;
static osThreadId_t spi_task_handle = NULL;

static StreamBufferHandle_t uart_rx_stream = NULL;
static StreamBufferHandle_t spi_rx_stream = NULL;

static osSemaphoreId_t uart_tx_sema = NULL;
static osSemaphoreId_t spi_tx_rx_sema = NULL;

static uint8_t uart_rx_byte;

// ============================================================================

int uart_spi_start(uart_spi_params_t *params)
{
    assert(params);
    assert(params->huart);
    assert(params->hspi);

    huart = params->huart;
    hspi = params->hspi;

    HAL_StatusTypeDef status;

    // ----------------------

    // Register some HAL UART callbacks

    status = HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID, uart_tx_complete_callback);
    assert(status == HAL_OK);

    status = HAL_UART_RegisterCallback(huart, HAL_UART_RX_COMPLETE_CB_ID, uart_rx_complete_callback);
    assert(status == HAL_OK);

    status = HAL_UART_RegisterCallback(huart, HAL_UART_ERROR_CB_ID, uart_error_callback);
    assert(status == HAL_OK);

    // Create UART-to-SPI stream buffer
    uart_rx_stream = xStreamBufferCreate(1024, 1);
    assert(uart_rx_stream);

    // Semaphore is released at initial
    uart_tx_sema = osSemaphoreNew(1, 1, NULL);
    assert(uart_tx_sema);

    // ----------------------

    // Register some HAL SPI callbacks

    status = HAL_SPI_RegisterCallback(hspi, HAL_SPI_TX_RX_COMPLETE_CB_ID, spi_tx_rx_complete_callback);
    assert(status == HAL_OK);

    status = HAL_SPI_RegisterCallback(hspi, HAL_SPI_ERROR_CB_ID, spi_error_callback);
    assert(status == HAL_OK);

    // Create SPI-to-UART stream buffer
    spi_rx_stream = xStreamBufferCreate(1024, 1);
    assert(spi_rx_stream);

    // Semaphore is released at initial
    spi_tx_rx_sema = osSemaphoreNew(1, 1, NULL);
    assert(spi_tx_rx_sema);

    // ----------------------

    // Create UART and SPI tasks

    uart_task_handle = osThreadNew(uart_task, NULL, NULL);
    assert(uart_task_handle);

    spi_task_handle = osThreadNew(spi_task, NULL, NULL);
    assert(spi_task_handle);

    return 0;
}

// ============================================================================

/**
 * @brief UART communication task
 * 
 * The main logic.
 * 
 * The task waits for data from the SPI interface using the SPI-to-UART stream buffer.
 * Once the data has been read from the stream buffer,
 * it is asynchronously transmitted to the UART.
 * Data is transmitted to the UART in blocks of @ref CHUNK_BUFF_SIZE bytes or less
 * 
 * UART data reception is performed in byte-by-byte interrupt.
 * Each byte is sent to the ART-to-SPI stream
 * 
 * @param arg Arguments. Unused
 */
static void uart_task(void *arg)
{
    UNUSED(arg);

    static char chunk_buff[CHUNK_BUFF_SIZE];

    uart_rx_start();

    while (1) {
        // Continuously wait and receive data from the SPI-to-UART stream. Up to CHUNK_BUFF_SIZE bytes
        size_t length = xStreamBufferReceive(spi_rx_stream, chunk_buff, CHUNK_BUFF_SIZE, portMAX_DELAY);
        if (length > 0) {
            if (uart_tx_async(chunk_buff, length) != 0) {
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

/**
 * @brief SPI communication task
 * 
 * The main logic.
 * 
 * The task continuously executes SPI transactions and checks if the slave has data.
 * Reception and transmission are performed simultaneously.
 * 
 * Any slave data that is not zero is sent to the SPI-to-UART stream.
 * The string termination indicator '\0' is also sent to the stream.
 * 
 * Data is transmitted to the SPI in blocks of @ref CHUNK_BUFF_SIZE bytes or less
 * 
 * @param arg Arguments. Unused
 */
static void spi_task(void *arg)
{
    UNUSED(arg);

    static char chunk_buff_tx[CHUNK_BUFF_SIZE];
    static char chunk_buff_rx[CHUNK_BUFF_SIZE];

    bool message_receiving = false;

    while (1) {
        // Receive the UART-to-SPI stream data if it is exist
        size_t length = xStreamBufferReceive(uart_rx_stream, chunk_buff_tx, CHUNK_BUFF_SIZE, 0);
        if (length == 0) {
            // If no data in the stream then fill chunk_buff by zero
            // for following transmittion to the SPI

            length = CHUNK_BUFF_SIZE;
            memset(chunk_buff_tx, 0, length);
        }

        if (spi_tx_rx(chunk_buff_tx, chunk_buff_rx, length) != 0) {
            // Error. Just continue;
            continue;
        }

        if (spi_wait_ready(100) != 0) {
            // Abort ongoing transaction in case of timeout
            spi_abort();
        }

        
        // Iterate the UART-to-SPI stream data to find the not-zero data
        for (int q = 0; q < length; q++) {
            if (chunk_buff_rx[q] != '\0') {
                if (!message_receiving) {
                    // Start message receiving
                    message_receiving = true;
                }

                xStreamBufferSend(spi_rx_stream, &chunk_buff_rx[q], 1, 0);
            }
            else {
                if (message_receiving) {
                    // NULL terminated string received
                    message_receiving = false;

                    // Send '\0' 
                    xStreamBufferSend(spi_rx_stream, &chunk_buff_rx[q], 1, 0);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------

static int uart_rx_start(void)
{
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(huart, &uart_rx_byte, 1);

    return status == HAL_OK ? 0 : -1;
}

static int uart_tx_async(const void *data, size_t length)
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
    UNUSED(huart);

    xStreamBufferSend(uart_rx_stream, &uart_rx_byte, 1, 0);

    uart_rx_start();
}

static void uart_error_callback(UART_HandleTypeDef *huart)
{
    UNUSED(huart);

    osSemaphoreRelease(uart_tx_sema);
}

// ----------------------------------------------------------------------------

static int spi_tx_rx(const void *txd, void *rxd, size_t length)
{
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(hspi, (void *)txd, rxd, length);

    return status == HAL_OK ? 0 : -1;
}

static int spi_wait_ready(uint32_t timeout_ms)
{
    osStatus_t status = osSemaphoreAcquire(spi_tx_rx_sema, pdMS_TO_TICKS(timeout_ms));

    return status == osOK ? 0 : -1;
}

static void spi_abort(void)
{
    HAL_SPI_Abort_IT(hspi);
}

static void spi_tx_rx_complete_callback(SPI_HandleTypeDef *hspi)
{
    UNUSED(hspi);

    osSemaphoreRelease(spi_tx_rx_sema);
}

static void spi_error_callback(SPI_HandleTypeDef *hspi)
{
    UNUSED(hspi);

    osSemaphoreRelease(spi_tx_rx_sema);
}
