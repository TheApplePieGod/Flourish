project(FlourishCore C CXX)

if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

# Build dependencies
add_subdirectory("lib/vma")

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
)
if (APPLE)
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/macOS/include>"
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/MoltenVK/include>"
  )
else()
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/include>"
  )
endif()

if (NOT FLOURISH_GLFW_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_GLFW_INCLUDE_DIR}>"
  )
  target_compile_definitions(
    FlourishCore
    PUBLIC
    FL_USE_GLFW
  )
  message("Flourish compiling with GLFW support")
endif()

if (NOT FLOURISH_IMGUI_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_IMGUI_INCLUDE_DIR}>"
  )
  target_compile_definitions(
    FlourishCore
    PUBLIC
    FL_USE_IMGUI IMGUI_IMPL_VULKAN_NO_PROTOTYPES
  )
  message("Flourish compiling with ImGui support")
endif()

if (NOT FLOURISH_TRACY_INCLUDE_DIR STREQUAL "OFF")
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:${FLOURISH_TRACY_INCLUDE_DIR}>"
  )
  target_compile_definitions(
    FlourishCore
    PUBLIC
    FL_USE_TRACY
  )
  message("Flourish compiling with Tracy support")
endif()

# Link Aftermath
if (FLOURISH_ENABLE_AFTERMATH)
  message("Flourish compiling with Aftermath support")
  find_library(aftermath
    NAMES "GFSDK_Aftermath_Lib.x64"
    PATHS "$ENV{NSIGHT_AFTERMATH_SDK}/lib/x64"
  )
  target_link_libraries(FlourishCore ${aftermath})
  target_include_directories(
    FlourishCore
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{NSIGHT_AFTERMATH_SDK}/include>"
  )
  target_compile_definitions(
    FlourishCore
    PUBLIC
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
    NAMES "libshaderc_combined.a"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )
  target_link_libraries(FlourishCore ${shaderc})
  find_library(spirvcore
    NAMES "libspirv-cross-c.a"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )

  add_custom_target(CopyVulkanLibs ALL
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{VULKAN_SDK}/macOS/lib/libMoltenVK.dylib ${OUTPUT_DIRECTORY}/libMoltenVK.dylib
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${OUTPUT_DIRECTORY}/libMoltenVK.dylib ${OUTPUT_DIRECTORY}/libvulkan.1.dylib
  )
else()
endif()

# PCH
target_precompile_headers(
  FlourishCore
  PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/flpch.h>"
)
