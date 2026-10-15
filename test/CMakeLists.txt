# Unity is vendored in test/unity. It is third party code, so it is built as its
# own target and is deliberately left out of this project's -Werror.
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)

set(OSAL_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# The same suite once per setting. The first uses the osal_config.h users get,
# which has no RTOS. The others cannot come from another config file: osal.h
# includes osal_config.h from its own folder, and that always wins over an
# include path. So they define the config's include guard, which turns that
# include into nothing, and give OSAL_RTOS on the command line instead.
function(osal_suite name)
    set(target test_osal${name})

    add_executable(${target} test_osal.c)
    target_link_libraries(${target} PRIVATE osal unity)

    # The stubs stand in for the CubeMX main.h and the RTOS headers.
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stub)
    target_compile_definitions(${target} PRIVATE ${ARGN})

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE ${OSAL_WARNINGS})
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

osal_suite("")
osal_suite(_cmsis_v1           OSAL_CONFIG_H OSAL_RTOS=OSAL_RTOS_CMSIS_V1)
osal_suite(_cmsis_v2           OSAL_CONFIG_H OSAL_RTOS=OSAL_RTOS_CMSIS_V2)
osal_suite(_threadx            OSAL_CONFIG_H OSAL_RTOS=OSAL_RTOS_THREADX)

# CMSIS-RTOS v1 as CubeMX most often sets it up: no static allocation, which
# takes the static members out of ST's osThreadDef_t, and 7 priorities.
osal_suite(_cmsis_v1_dynamic   OSAL_CONFIG_H OSAL_RTOS=OSAL_RTOS_CMSIS_V1
                               configSUPPORT_STATIC_ALLOCATION=0 configMAX_PRIORITIES=7)
