if(APPLE)
  project(Core C CXX OBJC OBJCXX)
else()
  project(Core C CXX)
endif()

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
)
if (APPLE)
  target_include_directories(
    Core
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/macOS/include>"
  )
else()
  target_include_directories(
    Core
    PUBLIC
    "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/include>"
  )
endif()

# Link vulkan related libraries
if (WIN32)
  if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    set(DEBUG_LIB_EXTRAS "d")
  endif()
  find_library(shaderc
    NAMES "shaderc_shared"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(Core ${shaderc})
  find_library(spirvcore${DEBUG_LIB_EXTRAS}
    NAMES "spirv-cross-core${DEBUG_LIB_EXTRAS}"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(Core ${spirvcore${DEBUG_LIB_EXTRAS}})
  find_library(spirvglsl
    NAMES "spirv-cross-glsl"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(Core ${spirvglsl})
elseif(APPLE)
  find_library(shaderc
    NAMES "libshaderc_shared.dylib"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )
  target_link_libraries(Core ${shaderc})
  find_library(spirvcore
    NAMES "libspirv-cross-core.a"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )
  target_link_libraries(Core ${spirvcore})
  find_library(spirvglsl
    NAMES "libspirv-cross-glsl.a"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )
  target_link_libraries(Core ${sprivglsl})
  message($ENV{VULKAN_SDK})
else()
  find_library(shaderc
    NAMES "libshaderc_shared.so"
    PATHS "$ENV{VULKAN_SDK}/macOS/lib"
  )
endif()

# PCH
target_precompile_headers(
  Core
  PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/flpch.h>"
)