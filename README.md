<img src="https://raw.githubusercontent.com/TheApplePieGod/Flourish/dev/images/logo.png" width=25% height=25%>

# Flourish

Flourish is a cross-platform 3D rendering library. It is designed to allow both high and low level interaction with the underlying graphics APIs. It is also designed to be used with a multithreaded rendering approach. Flourish supports Vulkan for all platforms with (eventually) the option of switching to native Metal when running on Apple devices.

# Table of Contents

- [Introduction](#Introduction)
- [Getting Started](#Getting-Started)
    - [Requirements](#Requirements)
    - [Setup](#Setup)
    - [Usage](#Usage)
- [Documentation](#Documentation)
- [License](#License)

# Introduction

This library was written to replace the internal one used in the [Heart](https://github.com/TheApplePieGod/Heart) game engine. It is a work in progress, but it is currently has numerous features.

## Motivation

Flourish is intended to be used as a medium-level abstraction for the underlying graphics APIs on semi-modern devices. As it is an abstraction, some extremely fine-grained and targeted features or optimizations may not be available to the user. Additionally, in order to create a clean and simplified API, some assumptions here and there are made, which may mean older or esoteric hardware will not work correctly.

## Features

- Render graph based GPU execution model
    - Build graphs once or dynamically
    - Define dependencies between work and resources
- Sync or async GPU work execution 
- Multithreaded GPU command recording and submission
- Automatic optimizations for frame-based rendering
- Double and triple buffering support
- Queryable features depending on user hardware
    - ALPHA support for hardware-accelerated ray tracing
    - (Partial) bindless descriptor model
- Built-in GLSL shader compilation and reflection
- Automatic lifetime management for most resources
- Sync or async data uploads to and from the GPU
- Texture compression support

## Platforms

Flourish has been tested on the following platforms: `windows`, `android arm64`, `macos arm64`.

Flourish currently supports Vulkan 1.1 with a minimal amount of required extensions, which should be supported on most platforms.


# Getting Started

Setting up Flourish is relatively simple, and it utilizes CMake and git submodules. 

## Requirements

- Compiler using C++17
- [CMake](https://cmake.org/download/) >= 3.23
- [VulkanSDK](https://vulkan.lunarg.com/) >= 1.3.290

## Setup

1. Clone the repo using the `--recursive` flag to ensure all submodules are downloaded
2. Make sure the VulkanSDK is accessible via the `${VULKAN_SDK}` environment variable (this should happen automatically with the installer on Windows)
 - On MacOS, this should be <sdk_version>/macOS

## Usage

To use in a project, [add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html) should be sufficient if using git submodules or another form of package management. CMake install support will come at some point in the future.

```cmake
add_subdirectory("path-to-flourish")
target_link_libraries(MyTarget PUBLIC FlourishCore)
target_include_directories(MyTarget PUBLIC "path-to-flourish/Flourish/src")
```

There are also some CMake compile options that can be set using

```cmake
set(FLOURISH_FLAG OFF)
```

or

```cmake
set(FLOURISH_STR_FLAG "string")
```

1. `FLOURISH_BUILD_TESTS`: Build the test program. Default off.
2. `FLOURISH_ENABLE_LOGGING`: Enable logging output from the library. Default on.
3. `FLOURISH_ENABLE_AFTERMATH`: Enable integration with [NVIDIA NSight Aftermath](https://developer.nvidia.com/nsight-aftermath). The SDK must be accessible via the `${NSIGHT_AFTERMATH_SDK}` environment variable. Default off.
4. `FLOURISH_GLFW_INCLUDE_DIR`: Path to the include directory of [GLFW](https://www.glfw.org/). Default empty. Setting a value here will enable builtin GLFW support for Flourish.
5. `FLOURISH_IMGUI_INCLUDE_DIR`: Path to the include directory of [ImGui](https://github.com/ocornut/imgui). Default empty. Setting a value here will enable builtin ImGUI support for Flourish.
6. `FLOURISH_TRACY_INCLUDE_DIR`: Path to the include directory of [Tracy](https://github.com/wolfpld/tracy). Default empty. Setting a value here will enable builtin profiling for Flourish calls.

# Documentation

Coming soon

# License

Copyright (C) 2024 [Evan Thompson](https://evanthompson.site/)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>
