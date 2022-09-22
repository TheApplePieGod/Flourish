project(Core C CXX OBJC OBJCXX)

# Build dependencies
add_subdirectory("lib/vma")

# Build main
if(APPLE)
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/*.mm" "src/flpch.h")
else()
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/flpch.h")
endif()
add_library(Core STATIC "${SOURCES}" "${DEPENDENCY_SOURCES}")

target_compile_features(Core PUBLIC cxx_std_17)

# Includes
target_include_directories(
  Core
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/vma/include>"
  "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/include>"
)

find_library(mvk
  NAMES "libMoltenVK.dylib"
  PATHS "$ENV{VULKAN_SDK}/MoltenVK/dylib/macOS"
)
target_link_libraries(Core ${mvk})

find_library(shaderc
  NAMES "libshaderc_shared.dylib"
  PATHS "$ENV{VULKAN_SDK}/lib"
)
target_link_libraries(Core ${shaderc})

find_library(spirvcore
  NAMES "libspirv-cross-c.dylib"
  PATHS "$ENV{VULKAN_SDK}/lib"
)
target_link_libraries(Core ${spirvcore})

# PCH
target_precompile_headers(
  Core
  PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/flpch.h>"
)