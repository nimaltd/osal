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
foreach(rtos NONE CMSIS_V1 CMSIS_V2 THREADX)
    if(rtos STREQUAL "NONE")
        set(target test_osal)
        set(defs)
    else()
        string(TOLOWER ${rtos} name)
        set(target test_osal_${name})
        set(defs OSAL_CONFIG_H OSAL_RTOS=OSAL_RTOS_${rtos})
    endif()

    add_executable(${target} test_osal.c)
    target_link_libraries(${target} PRIVATE osal unity)

    # The stubs stand in for the CubeMX main.h and the RTOS headers.
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stub)
    target_compile_definitions(${target} PRIVATE ${defs})

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE ${OSAL_WARNINGS})
    endif()

    add_test(NAME ${target} COMMAND ${target})
endforeach()
