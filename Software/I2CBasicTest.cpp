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

#if DEVICE_I2C_ASYNCH
void test_incorrect_addr_async()
{
    uint8_t const data[2] = {0x01, 0x04}; // Writes 0x4 to address 1
    TEST_ASSERT_EQUAL(I2C::Result::NACK, i2c->transfer_and_wait(0x20,
                                                               reinterpret_cast<const char *>(data), 2,
                                                               nullptr, 0,
                                                               1s));
}
#endif

// The following tests write one byte in EEPROM, then read it back.  Each pair of tests does the same thing,
// but using a different API.
void test_simple_write_single_byte()
{
    // Write 0x2 to address 1
    i2c->start();
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS));
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(0x1)); // address
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(0x2)); // data
    i2c->stop();

    // Maximum program time before the EEPROM responds again
    ThisThread::sleep_for(5ms);
}

void test_simple_read_single_byte()
{
    // Set read address to 1
    i2c->start();
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS));
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(0x1)); // address
    // Do NOT call stop() so that we do a repeated start

    // Read the byte
    i2c->start();
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS | 1));
    int readByte = i2c->read_byte(false);
    i2c->stop();
    TEST_ASSERT_EQUAL(0x2, readByte);
}

void test_simple_write_transaction()
{
    uint8_t const data[2] = {0x01, 0x03}; // Writes 0x3 to address 1
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write(EEPROM_I2C_ADDRESS, reinterpret_cast<const char *>(data), 2));

    // Maximum program time before the EEPROM responds again
    ThisThread::sleep_for(5ms);
}

void test_simple_read_transaction()
{
    // Set read address to 1
    uint8_t const readAddr = 0x01;
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write(EEPROM_I2C_ADDRESS, reinterpret_cast<const char *>(&readAddr), 1, true));

    // Read the byte back
    uint8_t readByte = 0;
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->read(EEPROM_I2C_ADDRESS | 1, reinterpret_cast<char *>(&readByte), 1));
    TEST_ASSERT_EQUAL_UINT8(0x3, readByte);
}

// Test that we can do a single byte, then a repeated start, then a transaction
void test_repeated_single_byte_to_transaction()
{
    // Set read address to 1
    i2c->start();
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS));
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(0x1)); // address
    // Do NOT call stop() so that we do a repeated start

    ThisThread::sleep_for(1ms);

    // Read the byte back
    uint8_t readByte = 0;
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->read(EEPROM_I2C_ADDRESS | 1, reinterpret_cast<char *>(&readByte), 1));
    TEST_ASSERT_EQUAL_UINT8(0x3, readByte);
}

// Test that we can do a transaction, then a repeated start, then a single byte
void test_repeated_transaction_to_single_byte()
{
    // Set read address to 1
    uint8_t const readAddr = 0x01;
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write(EEPROM_I2C_ADDRESS, reinterpret_cast<const char *>(&readAddr), 1, true));

    // Read the byte
    i2c->start();
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->write_byte(EEPROM_I2C_ADDRESS | 1));
    int readByte = i2c->read_byte(false);
    i2c->stop();
    TEST_ASSERT_EQUAL(0x3, readByte);
}

#if DEVICE_I2C_ASYNCH
void test_simple_write_async()
{
    uint8_t const data[2] = {0x01, 0x04}; // Writes 0x4 to address 1
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->transfer_and_wait(EEPROM_I2C_ADDRESS,
                                                               reinterpret_cast<const char *>(data), 2,
                                                               nullptr, 0,
                                                               1s));

    // Maximum program time before the EEPROM responds again
    ThisThread::sleep_for(5ms);
}

void test_simple_read_async()
{
    // Set read address to 1, then read the data back in one fell swoop.
    uint8_t const readAddr = 0x01;
    uint8_t readByte = 0;
    TEST_ASSERT_EQUAL(I2C::Result::ACK, i2c->transfer_and_wait(EEPROM_I2C_ADDRESS, reinterpret_cast<const char *>(&readAddr), 1,
                                                               reinterpret_cast<char *>(&readByte), 1,
                                                               1s));

    TEST_ASSERT_EQUAL_UINT8(0x4, readByte);
}
#endif

utest::v1::status_t test_setup(const size_t number_of_cases)
{
	// Create I2C
	i2c = new I2C(PIN_I2C_SDA, PIN_I2C_SCL);
	i2c->frequency(100000); // Use a lower frequency so that a logic analyzer can more easily capture what's up

	// Setup Greentea using a reasonable timeout in seconds
	GREENTEA_SETUP(20, "default_auto");
	return verbose_test_setup_handler(number_of_cases);
}

// Macro to help with async tests (can only run them if the device has the I2C_ASYNCH feature)
#if DEVICE_I2C_ASYNCH
#define ADD_ASYNC_TEST(x) x,
#else
#define ADD_ASYNC_TEST(x)
#endif

// Test cases
Case cases[] = {
		Case("Correct Address - Single Byte", test_correct_addr_single_byte),
		Case("Correct Address - Transaction", test_correct_addr_transaction),
        Case("Incorrect Address - Single Byte", test_incorrect_addr_single_byte),
		Case("Incorrect Address - Transaction", test_incorrect_addr_transaction),
        ADD_ASYNC_TEST(Case("Incorrect Address - Async", test_incorrect_addr_async))
        Case("Simple Write - Single Byte", test_simple_write_single_byte),
        Case("Simple Read - Single Byte", test_simple_read_single_byte),
        Case("Simple Write - Transaction", test_simple_write_transaction),
        Case("Simple Read - Transaction", test_simple_read_transaction),
        Case("Mixed Usage - Single Byte -> repeated -> Transaction", test_repeated_single_byte_to_transaction),
        Case("Mixed Usage - Transaction -> repeated -> Single Byte", test_repeated_transaction_to_single_byte),
        ADD_ASYNC_TEST(Case("Simple Write - Async", test_simple_write_async))
        ADD_ASYNC_TEST(Case("Simple Read - Async", test_simple_read_async))
};

// TODO: Test repeated start from each mode into each other mode
// TODO: Test that another thread can execute during the async operation

Specification specification(test_setup, cases, greentea_continue_handlers);

// Entry point into the tests
int main()
{
	// Turn on I2C_EN pin to enable I2C data to the logic analyzer
	DigitalOut i2cEn(PIN_I2C_EN, 1);

	return !Harness::run(specification);
}
