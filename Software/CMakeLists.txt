cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

# Initialize Mbed OS build system
set(MBED_APP_JSON_PATH mbed_app.json)
include(mbed-os/tools/cmake/app.cmake)
add_subdirectory(mbed-os)

project(mbed-ce-ci-shield-tests)

enable_testing()
include(mbed_greentea)

add_executable(TestEEPROM TestEEPROM.cpp)
target_link_libraries(TestEEPROM mbed-storage-i2cee mbed-os)
mbed_set_post_build(TestEEPROM)

add_executable(TestSDCard TestSDCard.cpp)
target_link_libraries(TestSDCard mbed-storage-sd mbed-os)
mbed_set_post_build(TestSDCard)

# Add tests -------------------------------------------------------
mbed_greentea_add_test(
    TEST_NAME testshield-businout
    TEST_SOURCES BusInOutTest.cpp
)

mbed_greentea_add_test(
    TEST_NAME testshield-interruptin
    TEST_SOURCES InterruptInTest.cpp
)

mbed_greentea_add_test(
    TEST_NAME testshield-digitalio
    TEST_SOURCES DigitalIOTest.cpp
)

mbed_greentea_add_test(
    TEST_NAME testshield-i2c
    TEST_SOURCES I2CTest.cpp
    TEST_REQUIRED_LIBS mbed-storage-i2cee
)

mbed_greentea_add_test(
    TEST_NAME testshield-spi
    TEST_SOURCES SPITest.cpp
    TEST_REQUIRED_LIBS mbed-storage-sd mbed-storage-fat
)

mbed_finalize_build()