cmake_minimum_required(VERSION 3.0)
project(lg_common_platform_code LANGUAGES C)

add_library(lg_common_platform_code STATIC
    crash.c
    dpi.c
    sysinfo.c
    thread.c
    event.c
    windebug.c
    ivshmem.c
    time.c
)

target_compile_options(lg_common_platform_code PRIVATE -Wall -Werror)
target_link_libraries(lg_common_platform_code
	lg_common
	setupapi
)

if (ENABLE_BACKTRACE)
    target_link_libraries(lg_common_platform_code dbghelp)
    target_compile_definitions(lg_common_platform_code PRIVATE ENABLE_BACKTRACE)
endif()
