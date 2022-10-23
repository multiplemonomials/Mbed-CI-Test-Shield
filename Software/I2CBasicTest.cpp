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
#include "ci_test_config.h"

using namespace utest::v1;

// Configuration for 24FC02-I/SN
#define EEPROM_I2C_ADDRESS 0xA0 // 8-bit address

// Single instance of I2C used in the test.
// Prefer to use a single instance so that, if it gets in a bad state and cannot execute further
// transactions, this will be visible in the test.
I2C * i2c;

// Test that we can address the EEPROM with its correct address
void test_correct_addr_single_byte()
{
	i2c->start();
	TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS));
    //i2c->write_byte(0x20);
	i2c->stop();
}
void test_correct_addr_transaction()
{
	TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write(EEPROM_I2C_ADDRESS, nullptr, 0, false));
}

// Test that we receive a NACK when trying to use an address that doesn't exist
void test_incorrect_addr_single_byte()
{
	i2c->start();
	TEST_ASSERT_EQUAL(I2C::Result::NACK, i2c->write_byte(0x20));
	i2c->stop();
}
void test_incorrect_addr_transaction()
{
	i2c->start();
	TEST_ASSERT_EQUAL(I2C::Result::NACK, i2c->write(0x20, nullptr, 0, false));
	i2c->stop();
}

utest::v1::status_t test_setup(const size_t number_of_cases)
{
	// Create I2C
	i2c = new I2C(PIN_I2C_SDA, PIN_I2C_SCL);
	i2c->frequency(100000); // Use a lower frequency so that a logic analyzer can more easily capture what's up

	// Setup Greentea using a reasonable timeout in seconds
	GREENTEA_SETUP(20, "default_auto");
	return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
			// TODO need tests that test using a correct and incorrect address and seeing if we get the right result.
			// Should use single byte and multi byte API.  Also should have one that does and does not transfer one byte after sending the address.
		Case("Correct Address - Single Byte", test_correct_addr_single_byte),
		Case("Correct Address - Transaction", test_correct_addr_transaction),
        Case("Incorrect Address - Single Byte", test_incorrect_addr_single_byte),
		Case("Incorrect Address - Transaction", test_incorrect_addr_transaction),
        Case("2nd Correct Address - Transaction", test_correct_addr_transaction)
};

Specification specification(test_setup, cases, greentea_continue_handlers);

// Entry point into the tests
int main()
{
	// Turn on I2C_EN pin to enable I2C data to the logic analyzer
	DigitalOut i2cEn(PIN_I2C_EN, 1);

	return !Harness::run(specification);
}
