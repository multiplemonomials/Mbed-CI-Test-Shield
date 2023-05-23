/*
 * Copyright (c) 2023 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// check if I2C is supported on this device
#if !DEVICE_SPI
#error [NOT_SUPPORTED] SPI not supported on this platform, add 'DEVICE_SPI' definition to your platform.
#endif

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "ci_test_config.h"

using namespace utest::v1;

// Single instance of SPI used in the test.
// Prefer to use a single instance so that, if it gets in a bad state and cannot execute further
// transactions, this will be visible in the test.
SPI * spi;

// Bytes of the data message that each test sends
uint8_t const messageBytes[] = {0x01, 0x02, 0x04, 0x08};

// Long data message used in a few tests
uint8_t const longMessage[32] = {};

const uint32_t spiFreq = 1000000;

void write_single_byte()
{
    spi->select();
    for(size_t byteIdx = 0; byteIdx < sizeof(messageBytes); byteIdx++)
    {
        spi->write(messageBytes[byteIdx]);
    }
    spi->deselect();
}

void write_transactional_tx_only()
{
    spi->write(messageBytes, sizeof(messageBytes), nullptr, 0);
}

void write_transactional_tx_rx()
{
    uint8_t rxBytes[sizeof(messageBytes)];
    spi->write(messageBytes, sizeof(messageBytes), rxBytes, sizeof(messageBytes));
}

#if DEVICE_SPI_ASYNCH

template<DMAUsage dmaUsage>
void write_async_tx_only()
{
    spi->set_dma_usage(dmaUsage);
    spi->transfer_and_wait<uint8_t>(messageBytes, sizeof(messageBytes), nullptr, 0);
}

template<DMAUsage dmaUsage>
void write_async_tx_rx()
{
    spi->set_dma_usage(dmaUsage);
    uint8_t rxBytes[sizeof(messageBytes)];
    spi->transfer_and_wait(messageBytes, sizeof(messageBytes), rxBytes, sizeof(messageBytes));
    printf("Got: %hhx %hhx %hhx %hhx\n", rxBytes[0], rxBytes[1], rxBytes[2], rxBytes[3]);
}

/*
 * This test measures how long it takes to do an asynchronous transaction and how much of that time is spent
 *
 */
template<DMAUsage dmaUsage>
void benchmark_async_transaction()
{
    spi->set_dma_usage(dmaUsage);

    Timer transactionTimer;
    Timer backgroundTimer;

    volatile bool transactionDone = false;

    event_callback_t transferCallback([&](int event) {
        transactionDone = true;
    });

    // Kick off the transaction in the main thread
    transactionTimer.start();
    spi->transfer<uint8_t>(longMessage, sizeof(longMessage), nullptr, 0, transferCallback);

    // Now count how much time we have free while the transaction executes in the background
    backgroundTimer.start();
    while(!transactionDone)
    {}
    backgroundTimer.stop();
    transactionTimer.stop();

    printf("Transferred %zu bytes @ %" PRIu32 "kHz in %" PRIi64 "us, with %" PRIi64 "us occurring in the background.\n",
           sizeof(longMessage), spiFreq / 1000,
           std::chrono::duration_cast<std::chrono::microseconds>(transactionTimer.elapsed_time()).count(),
           std::chrono::duration_cast<std::chrono::microseconds>(backgroundTimer.elapsed_time()).count());
    auto oneClockPeriod = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<float>(1.0/spiFreq));
    printf("Note: Based on the byte count and frequency, the theoretical best time for this SPI transaction is %" PRIi64 "us\n",
            std::chrono::duration_cast<std::chrono::microseconds>(oneClockPeriod * sizeof(longMessage) * 8).count());
    printf("Note: the above background time does not include overhead from interrupts, which may be significant.\n");
}

#endif

utest::v1::status_t test_setup(const size_t number_of_cases)
{
    // Create SPI.  For now, we won't use any CS pin, because we don't want to trigger the MicroSD card
    // to actually respond.
    spi = new SPI(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCLK, PIN_SPI_CS, use_gpio_ssel);
    spi->frequency(spiFreq);
    spi->format(8, 0); // Mode 0, the bus pirate will be set for this

    // For starters, don't use DMA, but we will use it later
    spi->set_dma_usage(DMA_USAGE_NEVER);

    // Set I2C_EN to 0 so that SPI is routed to the onboard logic analyzer
    static DigitalOut i2cEn(PIN_I2C_EN, 0);

    // Also make sure the SD card is NOT selected
    static DigitalOut cs(PIN_SPI_CS, 1);

    // Setup Greentea using a reasonable timeout in seconds
    GREENTEA_SETUP(20, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

void test_teardown(const size_t passed, const size_t failed, const failure_t failure)
{
    delete spi;
    return greentea_test_teardown_handler(passed, failed, failure);
}

Case cases[] = {
        Case("Send Data via Single Byte API", write_single_byte),
        Case("Send Data via Transactional API (Tx only)", write_transactional_tx_only),
        Case("Send Data via Transactional API (Tx/Rx)", write_transactional_tx_rx),
#if DEVICE_SPI_ASYNCH
// TODO transaction aborting test
// TODO multi-transaction queueing test
        Case("Send Data via Async Interrupt API (Tx only)", write_async_tx_only<DMA_USAGE_NEVER>),
        Case("Send Data via Async Interrupt API (Tx/Rx)", write_async_tx_rx<DMA_USAGE_NEVER>),
        Case("Benchmark Async SPI via Interrupts", benchmark_async_transaction<DMA_USAGE_NEVER>),
        Case("Send Data via Async DMA API (Tx only)", write_async_tx_only<DMA_USAGE_ALWAYS>),
        Case("Send Data via Async DMA API (Tx/Rx)", write_async_tx_rx<DMA_USAGE_ALWAYS>),
        Case("Benchmark Async SPI via DMA", benchmark_async_transaction<DMA_USAGE_NEVER>),
#endif
};

Specification specification(test_setup, cases, test_teardown, greentea_continue_handlers);

// Entry point into the tests
int main()
{
    return !Harness::run(specification);
}
