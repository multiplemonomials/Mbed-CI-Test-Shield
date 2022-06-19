/*
 * Copyright (c) 2022 ARM Limited
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

// check if SPI is supported on this device
#if !DEVICE_SPI
#error [NOT_SUPPORTED] SPI is not supported on this platform, add 'DEVICE_SPI' definition to your platform.
#endif

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "ci_test_config.h"
#include "FATFileSystem.h"
#include "SDBlockDevice.h"

using namespace utest::v1;

#define SD_TEST_STRING_MAX 100

char SD_TEST_STRING[SD_TEST_STRING_MAX] = {0};

alignas(SDBlockDevice) uint8_t sdBlockDevMemory[sizeof(SDBlockDevice)];

void init_string()
{
    int x = 0;
    for(x = 0; x < SD_TEST_STRING_MAX-1; x++){
        SD_TEST_STRING[x] = 'A' + (rand() % 26);
    }
    SD_TEST_STRING[SD_TEST_STRING_MAX-1] = 0;

    DEBUG_PRINTF("\r\n****\r\nSD Test String = %s\r\n****\r\n",SD_TEST_STRING);
}

// Construct an SPI object in spiMemory
SDBlockDevice * constructSDBlockDev(bool use_gpio_cs, uint64_t spiFreq)
{
    if(use_gpio_cs)
    {
        return new (sdBlockDevMemory) SDBlockDevice(use_gpio_ssel, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCLK, PIN_SPI_CS, spiFreq, true);
    }
    else
    {
        return new (sdBlockDevMemory) SDBlockDevice(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCLK, PIN_SPI_CS, spiFreq, true);
    }
}

void destroySDBlockDev(SDBlockDevice * sdDev)
{
    // we used placement new so we don't need to call delete, just invoke the destructor
    sdDev->~SDBlockDevice();
}

// Test object constructor / destructor
template<bool use_gpio_cs>
void test_object()
{
    SDBlockDevice * sdDev = constructSDBlockDev(use_gpio_cs, 1000000);
    TEST_ASSERT_MESSAGE(true,"If the tests hangs here then there is a problem with the SD or SPI objects"); // helpful debug message for if the test hangs
    destroySDBlockDev(sdDev);
}

// Test for SD Card being present on the shield
template<bool use_gpio_cs, uint64_t spiFreq>
void test_card_present()
{
    SDBlockDevice * sdDev = constructSDBlockDev(use_gpio_cs, spiFreq);

    int ret = sdDev->init();
    TEST_ASSERT_MESSAGE(ret == BD_ERROR_OK, "Failed to connect to SD card");

    sdDev->deinit();
    destroySDBlockDev(sdDev);
}

//// Test for SD Card being present on the shield
//void test_card_present()
//{
//    int error = 0;
//    SDBlockDevice sd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO, MBED_CONF_APP_SPI_CLK, MBED_CONF_APP_SPI_CS);
//    FATFileSystem fs("sd");
//    sd.init();
//
//    error = fs.mount(&sd);
//    if(error)
//        error = fs.reformat(&sd);
//
//    TEST_ASSERT_MESSAGE(error==0,"SD file system mount failed.");
//
//    FILE *File = fopen("/sd/card-present.txt", "w+");
//
//    TEST_ASSERT_MESSAGE(File != NULL,"SD Card is not present. Please insert an SD Card.");
//
//    error = fs.unmount();
//    TEST_ASSERT_MESSAGE(error==0,"SD file system unmount failed.");
//
//    sd.deinit();
//}
//
//// Test SD Card Write
//void test_sd_w()
//{
//    int error = 0;
//    SDBlockDevice sd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO, MBED_CONF_APP_SPI_CLK, MBED_CONF_APP_SPI_CS);
//    FATFileSystem fs("sd", &sd);
//
//    sd.init();
//
//    error = fs.mount(&sd);
//
//    FILE *File = fopen("/sd/test_sd_w.txt", "w");
//    TEST_ASSERT_MESSAGE(File != NULL,"SD Card is not present. Please insert an SD Card.");
//    TEST_ASSERT_MESSAGE(fprintf(File, SD_TEST_STRING) > 0,"Writing file to sd card failed");             // write data
//    // TODO: test that the file was written correctly
//    fclose(File);                              // close file on SD
//    TEST_ASSERT(true);
//
//    error = fs.unmount();
//
//    sd.deinit();
//}
//
//// Test SD Card Read
//void test_sd_r()
//{
//    char read_string [SD_TEST_STRING_MAX] = {0};
//    int error = 0;
//    SDBlockDevice sd(MBED_CONF_APP_SPI_MOSI, MBED_CONF_APP_SPI_MISO, MBED_CONF_APP_SPI_CLK, MBED_CONF_APP_SPI_CS);//alvin add
//    FATFileSystem fs("sd", &sd);
//
//    sd.init();
//
//    error = fs.mount(&sd);
//
//    FILE *File = fopen("/sd/test_sd_w.txt", "r");
//    TEST_ASSERT_MESSAGE(File != NULL,"SD Card is not present. Please insert an SD Card.");
//    fgets(read_string,SD_TEST_STRING_MAX,File); // read string from the file
//    //fread(read_string,sizeof(char),sizeof(SD_TEST_STRING),File); // read the string and compare
//    DEBUG_PRINTF("\r\n****\r\nRead '%s' in read test\r\n, string comparison returns %d\r\n****\r\n",read_string,strcmp(read_string,SD_TEST_STRING));
//    TEST_ASSERT_MESSAGE(strcmp(read_string,SD_TEST_STRING) == 0,"String read does not match string written"); // test that strings match
//    fclose(File);    // close file on SD
//    TEST_ASSERT(true);
//
//    error = fs.unmount();
//
//    sd.deinit();
//}

utest::v1::status_t test_setup(const size_t number_of_cases)
{
    // Setup Greentea using a reasonable timeout in seconds
    GREENTEA_SETUP(40, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
#if TESTSHIELD_ENABLE_HW_SPI_CS
        Case("SPI - Object Definable (HW CS)", test_object<false>),
        Case("SPI - SD card present (1MHz, HW CS)", test_card_present<false, 1000000>),
        Case("SPI - SD card present (10MHz, HW CS)", test_card_present<false, 10000000>),
#endif

        Case("SPI - Object Definable (GPIO CS)", test_object<true>),
        Case("SPI - SD card present (1MHz, GPIO CS)", test_card_present<true, 1000000>),
        Case("SPI - SD card present (10MHz, GPIO CS)", test_card_present<true, 10000000>),

//        Case("SPI - SD Write",     test_sd_w,greentea_failure_handler),
//        Case("SPI - SD Read",     test_sd_r,greentea_failure_handler),
//        Case("SPI - SD 2nd Write",     test_sd_w,greentea_failure_handler),
//        Case("SPI - SD 2nd Read",     test_sd_r,greentea_failure_handler),
};

Specification specification(test_setup, cases, greentea_continue_handlers);

// // Entry point into the tests
int main() {
    init_string();
    return !Harness::run(specification);
}
