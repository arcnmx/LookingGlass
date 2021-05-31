cmake_minimum_required(VERSION 3.0)
project(lg_common LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

add_subdirectory(src/platform)

set(COMMON_SOURCES
  src/stringutils.c
  src/stringlist.c
  src/option.c
  src/framebuffer.c
  src/KVMFR.c
  src/countedbuffer.c
)

add_library(lg_common STATIC ${COMMON_SOURCES})
target_link_libraries(lg_common lg_common_platform)
target_compile_options(lg_common PRIVATE -Wall -Werror -ffast-math)
target_compile_definitions(lg_common PRIVATE _GNU_SOURCE)
if(ENABLE_BACKTRACE)
  target_compile_definitions(lg_common PRIVATE ENABLE_BACKTRACE)
endif()

target_include_directories(lg_common
	INTERFACE
		include
	PRIVATE
		src
)
