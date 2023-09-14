# C++17 support was intorduced in CMake v3.12
cmake_minimum_required(VERSION 3.12)

# Make URL download use the new CMP0135 policy: https://cmake.org/cmake/help/latest/policy/CMP0135.html
cmake_policy(SET CMP0135 NEW)

# Only use C++ in this project
project(ies_rescale VERSION 1.1.2 LANGUAGES CXX)

# Include the path to the source folder
include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.h
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

# Include Google Test & Benchamrk libraries
include(FetchContent)

FetchContent_Declare(
	googletest
	# Specify the commit we depend on and update it regularly.
	URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(
	googletest
)

# The main unit test executable
add_executable(ies_rescale_test ${PROJECT_SOURCE_DIR}/test/ies_rescale_test.cpp ${SOURCES})

# Link against Google Test
target_link_libraries(ies_rescale_test gtest)

# Preprocesor definition to set the version
target_compile_definitions(ies_rescale_test PRIVATE IES_RESCALE_VERSION="${PROJECT_VERSION}")

# Restrict the C++ version to 17 and above
set_property(TARGET ies_rescale_test PROPERTY CXX_STANDARD 17)
set_property(TARGET ies_rescale_test PROPERTY CXX_STANDARD_REQUIRED On)
set_property(TARGET ies_rescale_test PROPERTY CXX_EXTENSIONS Off)

