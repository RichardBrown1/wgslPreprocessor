cmake_minimum_required (VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)

project ("WGSLPreprocessor")

add_executable("${CMAKE_PROJECT_NAME}" wgslPreprocessor.cpp)

if (MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror -std=c++20 -fsanitize=undefined -fsanitize=address)
endif()

