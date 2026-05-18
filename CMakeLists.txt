cmake_minimum_required(VERSION 3.25)
project(ts_decoder LANGUAGES CXX)

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
