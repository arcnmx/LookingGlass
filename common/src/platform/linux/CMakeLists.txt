cmake_minimum_required(VERSION 3.0)
project(lg_common_platform_code LANGUAGES C)

add_library(lg_common_platform_code STATIC
    crash.c
    sysinfo.c
    thread.c
    event.c
    ivshmem.c
    time.c
)
target_compile_options(lg_common_platform_code PRIVATE -Wall -Werror)

if(ENABLE_BACKTRACE)
	find_package(BFD REQUIRED)
	target_link_libraries(lg_common_platform_code PRIVATE ${BFD_LIBRARIES})
	target_compile_definitions(lg_common_platform_code PRIVATE ENABLE_BACKTRACE)
endif()

find_package(Threads REQUIRED)
target_link_libraries(lg_common_platform_code
	lg_common
	Threads::Threads
)
