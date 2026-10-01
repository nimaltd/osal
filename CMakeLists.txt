cmake_minimum_required(VERSION 3.16)

project(osal
    VERSION     1.0.0
    LANGUAGES   C
    DESCRIPTION "Mutex and delay for STM32, on bare metal, FreeRTOS or ThreadX"
    HOMEPAGE_URL "https://github.com/nimaltd/osal")

option(OSAL_BUILD_TESTS "Build the host unit tests" OFF)

# The whole library is src/osal.h, so the target only carries its include path.
# osal.h includes the platform main.h, and the RTOS headers when one is chosen,
# which it does not ship: whatever links this must be able to find them.
add_library(osal INTERFACE)
add_library(nimaltd::osal ALIAS osal)

target_include_directories(osal INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_compile_features(osal INTERFACE c_std_11)

if(OSAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
