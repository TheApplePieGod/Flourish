cmake_minimum_required(VERSION 3.23)

project(
    Flourish
    VERSION 0.1
    DESCRIPTION "A cross-platform 3D rendering library"
)

option(FLOURISH_BUILD_TESTS "Build the tests" OFF)
option(FLOURISH_ENABLE_LOGGING "Enable logging" ON)
option(FLOURISH_ENABLE_AFTERMATH "Enable building with the NSight Aftermath SDK" OFF)
option(FLOURISH_GLFW_INCLUDE_DIR "Include directory for GLFW" "OFF")
option(FLOURISH_IMGUI_INCLUDE_DIR "Include directory for ImGUI" "OFF")
option(FLOURISH_TRACY_INCLUDE_DIR "Include directory for Tracy" "OFF")

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}) 
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message("Detected GNU compiler")
    set(CMAKE_CXX_FLAGS "-fpermissive")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message("Detected MSVC compiler")
    add_compile_options(/MP /INCREMENTAL)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("Detected Clang compiler")
endif()

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    message("Building Flourish in Debug mode")
    add_compile_definitions(FL_DEBUG)
else()
    message("Building Flourish in Release mode")
endif()

if (FLOURISH_ENABLE_LOGGING)
    add_compile_definitions(FL_LOGGING)
endif()

add_subdirectory("Flourish")
if (FLOURISH_BUILD_TESTS)
    message("Building Flourish tests")
    add_subdirectory("FlourishTesting")
endif()
