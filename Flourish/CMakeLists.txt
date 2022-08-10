project(Core)

# Build dependencies
add_subdirectory("lib/glm")

# Build main
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/flpch.h")
add_library(Core STATIC "${SOURCES}" "${DEPENDENCY_SOURCES}")

target_compile_features(Core PUBLIC cxx_std_17)

# Includes
target_include_directories(
  Core
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/glm>"
  "$<BUILD_INTERFACE:$ENV{VULKAN_SDK}/include>"
)

# Locate vulkansdk libraries
if (WIN32)
  find_library(vulkan
    NAMES "vulkan-1"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  find_library(shaderc
    NAMES "shaderc_shared"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  find_library(spirvglsl
    NAMES "spirv-cross-glsl"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
else()
  find_library(vulkan
    NAMES "libvulkan.so"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  find_library(shaderc
    NAMES "libshaderc_shared.so"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  find_library(spirvglsl
    NAMES "libspirv-cross-glsl.a"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
endif()

# Locate & link accessory vulkan libraries
if (WIN32)
  if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    find_library(spirvcored
      NAMES "spirv-cross-cored"
      PATHS "$ENV{VULKAN_SDK}/lib"
    )
    target_link_libraries(Core ${spirvcored})
  else()
    find_library(spirvcore
      NAMES "spirv-cross-core"
      PATHS "$ENV{VULKAN_SDK}/lib"
    )
    target_link_libraries(Core ${spirvcore})
  endif()
else()
  find_library(spirvcore
    NAMES "libspirv-cross-core.a"
    PATHS "$ENV{VULKAN_SDK}/lib"
  )
  target_link_libraries(Core ${spirvcore})
endif()

# Link dependencies
target_link_libraries(
  Core
  glm ${vulkan} ${shaderc} ${spirvglsl}
)

# PCH
target_precompile_headers(
  Core
  PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/flpch.h>"
)