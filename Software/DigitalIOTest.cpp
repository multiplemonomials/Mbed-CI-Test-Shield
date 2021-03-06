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

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"


using namespace utest::v1;

// Template to test paired Digital IO pins, meant to be re-used multiple times
template <PinName dout_pin, PinName din_pin>
void DigitalIO_Test()
{
    DigitalOut dout(dout_pin);
    DigitalIn din(din_pin);
    // test 0
    dout = 0;
    wait_ns(100);
    TEST_ASSERT_MESSAGE(0 == din.read(),"Expected value to be 0, read value was not zero");
    // test 1
    dout = 1;
    wait_ns(100);
    TEST_ASSERT_MESSAGE(1 == din.read(),"Expected value to be 1, read value was not one");
    // test 2
    // Test = operator in addition to the .read() function
    dout = 0;
    wait_ns(100);
    TEST_ASSERT_MESSAGE(0 == din,"Expected value to be 0, read value was not zero");
}

utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea using a reasonable timeout in seconds
    GREENTEA_SETUP(30, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Digital I/O BUSIN_0 -> BUSOUT_0", DigitalIO_Test<PIN_BUSOUT_0, PIN_BUSIN_0>),
    Case("Digital I/O BUSOUT_0 -> BUSIN_0", DigitalIO_Test<PIN_BUSIN_0, PIN_BUSOUT_0>),
    Case("Digital I/O BUSIN_1 -> BUSOUT_1", DigitalIO_Test<PIN_BUSOUT_1, PIN_BUSIN_1>),
    Case("Digital I/O BUSOUT_1 -> BUSIN_1", DigitalIO_Test<PIN_BUSIN_1, PIN_BUSOUT_1>),
    Case("Digital I/O BUSIN_2 -> BUSOUT_2", DigitalIO_Test<PIN_BUSOUT_2, PIN_BUSIN_2>),
    Case("Digital I/O BUSOUT_2 -> BUSIN_2", DigitalIO_Test<PIN_BUSIN_2, PIN_BUSOUT_2>),
};

Specification specification(test_setup, cases, greentea_continue_handlers);

// Entry point into the tests
int main()
{
    return !Harness::run(specification);
}
