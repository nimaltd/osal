cmake_minimum_required(VERSION 3.16)

project(osal
    VERSION     1.0.0
    LANGUAGES   C
    DESCRIPTION "OS abstraction layer for STM32: the same mutex and delay on bare metal, FreeRTOS and ThreadX"
    HOMEPAGE_URL "https://github.com/nimaltd/osal")

option(OSAL_BUILD_TESTS "Build the host unit tests" OFF)

# The library is the headers in src/, so the target only carries their include
# path. osal.h includes the platform main.h, and its port the RTOS header when
# one is chosen, which it does not ship: whatever links this must find them.
add_library(osal INTERFACE)
add_library(nimaltd::osal ALIAS osal)

target_include_directories(osal INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_compile_features(osal INTERFACE c_std_11)

if(OSAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
