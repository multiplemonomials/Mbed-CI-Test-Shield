cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

# Initialize Mbed OS build system
set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR}) # Directory where mbed_config.cmake is found
include(mbed-os/tools/cmake/app.cmake)
add_subdirectory(mbed-os)

project(mbed-ce-ci-shield-tests)

add_executable(TestEEPROM TestEEPROM.cpp)
target_link_libraries(TestEEPROM mbed-storage-i2cee mbed-os)
mbed_set_post_build(TestEEPROM)

add_executable(TestSDCard TestSDCard.cpp)
target_link_libraries(TestSDCard mbed-storage-sd mbed-os)
mbed_set_post_build(TestSDCard)