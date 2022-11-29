project(Tests C CXX)

if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

# Build dependencies
add_subdirectory("lib/spdlog")

# Include other dependency sources that need to be built
set(DEPENDENCY_SOURCES
  "lib/stb_image/stb_image.cpp"
)

# Copy resources
add_custom_target(
  CopyResources ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/resources ${OUTPUT_DIRECTORY}/resources
  DEPENDS Tests
)

# Build main
if(APPLE)
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h" "src/*.mm")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/FlourishTesting/WindowsWindow.cpp")
else()
  file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
endif()
add_executable(Tests "${SOURCES}" "${DEPENDENCY_SOURCES}")

if(APPLE)
  target_link_options(Tests PUBLIC "SHELL:-framework QuartzCore" "SHELL:-framework Cocoa")
endif()
target_compile_features(Tests PUBLIC cxx_std_17)

# Include directories
target_include_directories(
  Tests
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog/include>"
)

target_link_libraries(Tests FlourishCore)

# PCH
target_precompile_headers(Tests REUSE_FROM FlourishCore)