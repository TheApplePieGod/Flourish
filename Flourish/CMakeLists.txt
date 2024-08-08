project(FlourishCore C CXX)

if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

# Vulkan compile definitions
if (APPLE)
  add_compile_definitions(VK_USE_PLATFORM_METAL_EXT)
elseif (WIN32)
  add_compile_definitions(VK_USE_PLATFORM_WIN32_KHR)
elseif (ANDROID)
  add_compile_definitions(VK_USE_PLATFORM_ANDROID_KHR)
elseif (UNIX)
  add_compile_definitions(VK_USE_PLATFORM_XCB_KHR)
endif()

# For overriding default option values
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Build dependencies
add_subdirectory("lib/vma")
set(VULKAN_HEADERS_INSTALL_DIR $ENV{VULKAN_SDK})
add_subdirectory("lib/volk")

# Need to compile all sdk dependencies manually
if (ANDROID)
  add_subdirectory("lib/spirv-headers")
  set(SKIP_SPIRV_TOOLS_INSTALL ON)
  set(SPIRV_WERROR OFF)
  set(SPIRV_SKIP_EXECUTABLES ON)
  set(SPIRV_SKIP_TESTS ON)
  set(SPIRV_CHECK_CONTEXT OFF)
  add_subdirectory("lib/spirv-tools")
  set(ENABLE_CTEST OFF)
  set(SKIP_GLSLANG_INSTALL ON)
  set(ENABLE_SPVREMAPPER OFF)
  set(ENABLE_GLSLANG_BINARIES OFF)
  add_subdirectory("lib/glslang")
  set(SHADERC_SKIP_INSTALL ON)
  set(SHADERC_SKIP_TESTS ON)
  set(SHADERC_SKIP_EXAMPLES ON)
  set(SHADERC_SKIP_COPYRIGHT_CHECK ON)
  add_subdirectory("lib/shaderc")
  set(SPIRV_CROSS_CLI OFF)
  set(SPIRV_CROSS_SKIP_INSTALL ON)
  set(SPIRV_CROSS_ENABLE_TESTS OFF)
  add_subdirectory("lib/spirv-cross")
endif()

# Build main
if(APPLE)
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/*.mm" "src/flpch.h")
else()
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/flpch.h")
endif()
add_library(FlourishCore STATIC "${SOURCES}" "${DEPENDENCY_SOURCES}")

target_compile_features(FlourishCore PUBLIC cxx_std_17)

# Includes
target_include_directories(
  FlourishCore
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/vma/include>"
  "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/include>"
)
if (APPLE)
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/../MoltenVK/include>"
  )
endif()

if (NOT FLOURISH_GLFW_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(FlourishCore PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_GLFW_INCLUDE_DIR}>"
  )
  target_compile_definitions(FlourishCore PUBLIC
    FL_USE_GLFW
  )
  message("Flourish compiling with GLFW support")
endif()

if (NOT FLOURISH_IMGUI_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(FlourishCore PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_IMGUI_INCLUDE_DIR}>"
  )
  target_compile_definitions(FlourishCore PUBLIC
    FL_USE_IMGUI IMGUI_IMPL_VULKAN_NO_PROTOTYPES
  )
  message("Flourish compiling with ImGui support")
endif()

if (NOT FLOURISH_TRACY_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(FlourishCore PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_TRACY_INCLUDE_DIR}>"
  )
  target_compile_definitions(FlourishCore PUBLIC
    FL_USE_TRACY
  )
  message("Flourish compiling with Tracy support")
endif()

# Link
target_link_libraries(FlourishCore
  VulkanMemoryAllocator volk
)

# Link Aftermath
if (FLOURISH_ENABLE_AFTERMATH)
  message("Flourish compiling with Aftermath support")
  find_library(aftermath
    NAMES "GFSDK_Aftermath_Lib.x64"
    PATHS "$ENV{NSIGHT_AFTERMATH_SDK}/lib/x64"
  )
  target_link_libraries(FlourishCore ${aftermath})
  target_include_directories(FlourishCore PUBLIC
    "$<BUILD_INTERFACE:$ENV{NSIGHT_AFTERMATH_SDK}/include>"
  )
  target_compile_definitions(FlourishCore PUBLIC
    FL_HAS_AFTERMATH
  )
  add_custom_target(CopyAftermathLibs ALL
      COMMAND ${CMAKE_COMMAND} -E copy $ENV{NSIGHT_AFTERMATH_SDK}/lib/x64/GFSDK_Aftermath_Lib.x64.dll ${OUTPUT_DIRECTORY}/GFSDK_Aftermath_Lib.x64.dll
  )
endif()

# Link vulkan related libraries & copy if necessary
if (WIN32)
  find_library(shaderc
    NAMES "shaderc_shared"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(FlourishCore ${shaderc})
  find_library(spirvcore
    NAMES "spirv-cross-c-shared"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(FlourishCore ${spirvcore})

  add_custom_target(CopyVulkanLibs ALL
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/Bin/shaderc_shared.dll ${OUTPUT_DIRECTORY}/shaderc_shared.dll
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/Bin/spirv-cross-c-shared.dll ${OUTPUT_DIRECTORY}/spirv-cross-c-shared.dll
  )
elseif(APPLE)
  find_library(shaderc
    NAMES "libshaderc_shared.dylib"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(FlourishCore ${shaderc})
  find_library(spirvcore
    NAMES "libspirv-cross-c-shared.dylib"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(FlourishCore ${spirvcore})

  add_custom_target(CopyVulkanLibs ALL
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/lib/libMoltenVK.dylib ${OUTPUT_DIRECTORY}/libMoltenVK.dylib
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${OUTPUT_DIRECTORY}/libMoltenVK.dylib ${OUTPUT_DIRECTORY}/libvulkan.1.dylib
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/lib/libshaderc_shared.dylib ${OUTPUT_DIRECTORY}/libshaderc_shared.1.dylib
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/lib/libspirv-cross-c-shared.dylib ${OUTPUT_DIRECTORY}/libspirv-cross-c-shared.0.dylib
  )
else()
    target_link_libraries(FlourishCore shaderc spirv-cross-c)
endif()

# PCH
target_precompile_headers(
  FlourishCore
  PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/flpch.h>"
)
