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

// Test which mounts the filesystem and creates a file
template<bool use_gpio_cs, uint64_t spiFreq>
void mount_fs_create_file()
{
    SDBlockDevice * sdDev = constructSDBlockDev(use_gpio_cs, spiFreq);
    FATFileSystem fs("sd");

	int ret = sdDev->init();
    TEST_ASSERT_MESSAGE(ret == BD_ERROR_OK, "Failed to connect to SD card");

    ret = fs.mount(sdDev);

    if(ret)
	{
		// This is expected if the SD card was not formatted previously
		ret = fs.reformat(sdDev);
	}

    TEST_ASSERT_MESSAGE(ret==0,"SD file system mount failed.");

    FILE * file = fopen("/sd/card-present.txt", "w+");

    TEST_ASSERT_MESSAGE(file != nullptr,"Failed to create file");

	fclose(file);

    ret = fs.unmount();
    TEST_ASSERT_MESSAGE(ret==0,"SD file system unmount failed.");

    destroySDBlockDev(sdDev);
}

// Test which writes, reads, and deletes a file.
template<bool use_gpio_cs, uint64_t spiFreq>
void test_sd_file()
{
    SDBlockDevice * sdDev = constructSDBlockDev(use_gpio_cs, spiFreq);
    FATFileSystem fs("sd");

	int ret = sdDev->init();
    TEST_ASSERT_MESSAGE(ret == BD_ERROR_OK, "Failed to connect to SD card");

    ret = fs.mount(sdDev);
	TEST_ASSERT_MESSAGE(ret==0,"SD file system mount failed.");

	// Write the test string to a file.
    FILE * file = fopen("/sd/test_sd_w.txt", "w");
    TEST_ASSERT_MESSAGE(file != nullptr,"Failed to create file");
	init_string();
    TEST_ASSERT_MESSAGE(fprintf(file, SD_TEST_STRING) > 0,"Writing file to sd card failed");
    fclose(file);

	// Now open it and read the string back
	char read_string[SD_TEST_STRING_MAX] = {0};
    file = fopen("/sd/test_sd_w.txt", "r");
	TEST_ASSERT_MESSAGE(file != nullptr,"Failed to open file");

	ret = fread(read_string, sizeof(char), sizeof(SD_TEST_STRING), file);
	TEST_ASSERT_MESSAGE(ret == SD_TEST_STRING_MAX, "Failed to read data");
	DEBUG_PRINTF("\r\n****\r\nRead '%s' in read test\r\n, string comparison returns %d\r\n****\r\n",read_string,strcmp(read_string,SD_TEST_STRING));
	TEST_ASSERT_MESSAGE(strcmp(read_string,SD_TEST_STRING) == 0,"String read does not match string written");

	// Check that reading one additional char causes an EOF error
	ret = fread(read_string, sizeof(char), 1, file);
	TEST_ASSERT_MESSAGE(ret < 1, "fread did not return error?");
	TEST_ASSERT(feof(file));

	fclose(file);

	// Delete the file and make sure it's gone
	remove("/sd/test_sd_w.txt");
	TEST_ASSERT(fopen("/sd/test_sd_w.txt", "w") == nullptr);

	// Clean up
	ret = fs.unmount();
    TEST_ASSERT_MESSAGE(ret==0,"SD file system unmount failed.");

    destroySDBlockDev(sdDev);
}

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
		Case("SPI - Mount FS, Create File (1MHz, HW CS)", mount_fs_create_file<false, 1000000>),
		Case("SPI - Mount FS, Create File (10MHz, HW CS)", mount_fs_create_file<false, 10000000>),
		Case("SPI - Write, Read, and Delete File (1MHz, HW CS)", test_sd_file<false, 1000000>),
		Case("SPI - Write, Read, and Delete File (10MHz, HW CS))", test_sd_file<false, 10000000>),
#endif

        Case("SPI - Object Definable (GPIO CS)", test_object<true>),
        Case("SPI - SD card present (1MHz, GPIO CS)", test_card_present<true, 1000000>),
        Case("SPI - SD card present (10MHz, GPIO CS)", test_card_present<true, 10000000>),
		Case("SPI - Mount FS, Create File (1MHz, GPIO CS)", mount_fs_create_file<true, 1000000>),
		Case("SPI - Mount FS, Create File (10MHz, GPIO CS)", mount_fs_create_file<true, 10000000>),
		Case("SPI - Write, Read, and Delete File (1MHz, GPIO CS)", test_sd_file<true, 1000000>),
		Case("SPI - Write, Read, and Delete File (10MHz, GPIO CS))", test_sd_file<true, 10000000>),
};

Specification specification(test_setup, cases, greentea_continue_handlers);

// // Entry point into the tests
int main() {
    return !Harness::run(specification);
}
