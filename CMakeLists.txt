cmake_minimum_required(VERSION 3.25)
project(ts_decoder LANGUAGES CXX)

# --- LTO for optimised builds ---
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_output)
if(_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    message(WARNING "LTO not supported: ${_ipo_output}")
endif()

# --- Native arch for optimised builds ---
if(NOT CMAKE_CROSSCOMPILING)
    foreach(_config RELEASE RELWITHDEBINFO)
        string(APPEND CMAKE_C_FLAGS_${_config}   " -march=native")
        string(APPEND CMAKE_CXX_FLAGS_${_config} " -march=native")
    endforeach()
endif()

# --- ASan for debug builds ---
foreach(_lang C CXX)
    string(APPEND CMAKE_${_lang}_FLAGS_DEBUG " -fsanitize=address,undefined -fno-omit-frame-pointer")
endforeach()
string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG    " -fsanitize=address,undefined")
string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " -fsanitize=address,undefined")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_DEBUG " -fsanitize=address,undefined")


if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(FetchContent)
FetchContent_Declare(
    utilix
    GIT_REPOSITORY https://github.com/tomschammo/utilix
    GIT_TAG        main
)
FetchContent_MakeAvailable(utilix)

add_library(ts_decoder INTERFACE)
add_library(ts::decoder ALIAS ts_decoder)

target_include_directories(ts_decoder INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(ts_decoder INTERFACE cxx_std_23)

target_link_libraries(ts_decoder INTERFACE utilix::utilix)

if(PROFILE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    find_package(MetavisionSDK COMPONENTS core stream REQUIRED)
    add_executable(profile profile.cpp)
    target_compile_options(profile PRIVATE -Wno-psabi)
    target_link_libraries(profile PRIVATE ts::decoder MetavisionSDK::core MetavisionSDK::stream)

    # `cmake --build build --target perf` records a profile, then reports it.
    set(PERF_DATA "${CMAKE_SOURCE_DIR}/data/right.raw" CACHE STRING "raw file to profile")
    add_custom_target(perf
        COMMAND perf record --call-graph dwarf -F 999 -o perf.data $<TARGET_FILE:profile> ${PERF_DATA}
        COMMAND perf report -i perf.data
        DEPENDS profile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        VERBATIM)
endif()

# Dev-time flags: only applied when this project is built standalone, so
# downstream FetchContent consumers don't inherit our warnings/sanitizer.
if(PROJECT_IS_TOP_LEVEL)
    target_compile_options(ts_decoder INTERFACE
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g -O0>
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wcast-align -Wunused
        -Wconversion -Wsign-conversion
        -Wold-style-cast -Wzero-as-null-pointer-constant
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ts_decoder INTERFACE
            -Wduplicated-cond -Wduplicated-branches -Wuseless-cast)
    endif()

    # Sanitizer (Debug builds):
    #   cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=address
    #   cmake --build build-asan -j
    set(SANITIZE "" CACHE STRING "Sanitizer: address | undefined | thread")
    if(SANITIZE)
        target_compile_options(ts_decoder INTERFACE -fsanitize=${SANITIZE} -fno-omit-frame-pointer)
        target_link_options(ts_decoder INTERFACE -fsanitize=${SANITIZE})
    endif()
endif()
