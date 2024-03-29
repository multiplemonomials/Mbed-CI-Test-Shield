/*
 * Copyright (c) 2016 ARM Limited
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
#if !DEVICE_I2C
#error [NOT_SUPPORTED] I2C not supported on this platform, add 'DEVICE_I2C' definition to your platform.
#endif

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include <I2CEEBlockDevice.h>
#include "ci_test_config.h"

using namespace utest::v1;

// Configuration for 24FC02-I/SN
#define EEPROM_I2C_ADDRESS 0xA0
#define EEPROM_SIZE 2048
#define EEPROM_BLOCK_SIZE 8
#define EEPROM_ADDRESS_8_BIT true

// Fill array with random characters
void init_string(char* buffer, int len)
{
	int x = 0;
	for(x = 0; x < len; x++){
		buffer[x] = 'A' + (rand() % 26);
	}
	buffer[len-1] = 0; // add \0 to end of string
	DEBUG_PRINTF("\r\n****\r\nBuffer Len = `%d`, String = `%s`\r\n****\r\n",len,buffer);
}

char test_string[EEPROM_SIZE];
char read_string[EEPROM_SIZE];

// Template to write arbitrary data to arbitrary address and check the data is written correctly
template<uint32_t busSpeed, int size_of_data, int address>
void flash_WR()
{
    // Enable I2C signals to logic analyzer
    DigitalOut i2cEn(PIN_I2C_EN, 1);
    DigitalOut i2cEnFastMode(PIN_I2C_EN_FMP, busSpeed > 400000);

	I2CEEBlockDevice memory(PIN_I2C_SDA, PIN_I2C_SCL, EEPROM_I2C_ADDRESS, EEPROM_SIZE, EEPROM_BLOCK_SIZE, busSpeed,
	                        EEPROM_ADDRESS_8_BIT);

    // Reset buffers
    memset(test_string, 0, size_of_data);
    memset(read_string, 0, size_of_data);

	init_string((char *) test_string, size_of_data); // populate test_string with random characters

	DEBUG_PRINTF("\r\n****\r\n Test String = `%s` \r\n****\r\n", test_string);

	int programRet = memory.program((const void *) test_string, address, size_of_data);
	int readRet = memory.read((void *) read_string, address, size_of_data);

	if (programRet != BD_ERROR_OK || readRet != BD_ERROR_OK) {
		// No point in the other asserts
		TEST_ASSERT_EQUAL(programRet, BD_ERROR_OK);
		TEST_ASSERT_EQUAL(readRet, BD_ERROR_OK);
	}
	else
	{
		TEST_ASSERT_MESSAGE(memcmp((char *) test_string, (char *) read_string, size_of_data) == 0,
		                    "String Written != String Read");
		TEST_ASSERT_EQUAL_STRING_MESSAGE((char *) test_string, (char *) read_string,
		                                 "String read does not match the string written");
		TEST_ASSERT_EQUAL_STRING_MESSAGE((char *) read_string, (char *) test_string,
		                                 "String read does not match the string written");
		DEBUG_PRINTF(
				"\r\n****\r\n Address = `%d`\r\n Len = `%d`\r\n Written String = `%s` \r\n Read String = `%s` \r\n****\r\n",
				address, size_of_data, test_string, read_string);
	}
}

// Test single byte R/W
template<uint32_t busSpeed, int address>
void single_byte_WR()
{
    // Enable I2C signals to logic analyzer
    DigitalOut i2cEn(PIN_I2C_EN, 1);
    DigitalOut i2cEnFastMode(PIN_I2C_EN_FMP, busSpeed > 400000);

	I2CEEBlockDevice memory(PIN_I2C_SDA, PIN_I2C_SCL, EEPROM_I2C_ADDRESS, EEPROM_SIZE, EEPROM_BLOCK_SIZE, busSpeed, EEPROM_ADDRESS_8_BIT);
	char test = 'A' + rand()%26;
	char read;
	int r = 0;
	int w = 0;
	w = memory.program((const void *)&test, address, sizeof(test));
	r = memory.read((void *)&read, address, sizeof(test));
	DEBUG_PRINTF("\r\n****\r\n Num Bytes Read = %d \r\n Num Bytes Written = %d \r\n Read byte = `%c` \r\n Written Byte = `%c` \r\n****\r\n",r,w,read,test);

	TEST_ASSERT(r == BD_ERROR_OK);
	TEST_ASSERT(w == BD_ERROR_OK);
	TEST_ASSERT_EQUAL_MESSAGE(test,read,"Character Read does not equal character written!");
	TEST_ASSERT_MESSAGE(test == read, "character written does not match character read")
}

// Test initializing an I2C Object
void test_object()
{
	I2C i2c(PIN_I2C_SDA, PIN_I2C_SCL);
	TEST_ASSERT_MESSAGE(true,"If you hang here your I2C Object has problems");
}

utest::v1::status_t test_setup(const size_t number_of_cases)
{
	// Setup Greentea using a reasonable timeout in seconds
	GREENTEA_SETUP(20, "default_auto");
	return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
			// TODO need tests that test using a correct and incorrect address and seeing if we get the right result.
			// Should use single byte and multi byte API.  Also should have one that does and does not transfer one byte after sending the address.
		Case("I2C -  Instantiation of I2C Object", test_object),
		Case("I2C - 100kHz - EEPROM WR Single Byte",single_byte_WR<100000, 1>),
		Case("I2C - 100kHz - EEPROM 2nd WR Single Byte",single_byte_WR<100000, 1025>),
		Case("I2C - 100kHz - EEPROM WR 2 Bytes", flash_WR<100000, 2, 5>),
		Case("I2C - 100kHz - EEPROM 2nd WR 2 Bytes",flash_WR<100000, 2, 1029>),
		Case("I2C - 100kHz - EEPROM WR 10  Bytes",flash_WR<100000, 10, 100>),
		Case("I2C - 100kHz - EEPROM 2nd WR 10  Bytes",flash_WR<100000, 10, 1124>),
		Case("I2C - 100kHz - EEPROM WR Full Size",flash_WR<100000, EEPROM_SIZE, 0>),
		Case("I2C - 400kHz - EEPROM WR Single Byte",single_byte_WR<400000, 1>),
		Case("I2C - 400kHz - EEPROM 2nd WR Single Byte",single_byte_WR<400000, 1025>),
		Case("I2C - 400kHz - EEPROM WR 2 Bytes", flash_WR<400000, 2, 5>),
		Case("I2C - 400kHz - EEPROM 2nd WR 2 Bytes",flash_WR<400000, 2, 1029>),
		Case("I2C - 400kHz - EEPROM WR 10  Bytes",flash_WR<400000, 10, 100>),
		Case("I2C - 400kHz - EEPROM 2nd WR 10  Bytes",flash_WR<400000, 10, 1124>),
		Case("I2C - 400kHz - EEPROM WR Full Size",flash_WR<400000, EEPROM_SIZE, 0>),
};

Specification specification(test_setup, cases, greentea_continue_handlers);

// Entry point into the tests
int main()
{
	return !Harness::run(specification);
}
