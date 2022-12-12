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

# Getting Started

Setting up Flourish is relatively simple, and it utilizes CMake and git submodules. 

## Requirements

- Compiler using C++17
- [CMake](https://cmake.org/download/) >= 3.23
- [VulkanSDK](https://vulkan.lunarg.com/) >= 1.3.231
    - Make sure to include the 64-bit debuggable shader API libraries when installing on Windows

## Setup

1. Clone the repo using the `--recursive` flag to ensure all submodules are downloaded
2. Make sure the VulkanSDK is accessable via the `${VULKAN_SDK}` environment variable (this should happen automatically with the installer on Windows)

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
3. `FLOURISH_GLFW_INCLUDE_DIR`: Path to the include directory of GLFW. Default empty. Setting a value here will enable GLFW support for Flourish.
4. `FLOURISH_IMGUI_INCLUDE_DIR`: Path to the include directory of ImGUI. Default empty. Setting a value here will enable ImGUI support for Flourish.

# Documentation

Coming soon

# License

Copyright (C) 2022 [Evan Thompson](https://evanthompson.site/)

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