cmake_minimum_required(VERSION 3.20)
project(exif_batch_set LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer config packages (vcpkg provides exiv2Config.cmake)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

add_executable(exif_batch_set exif_batch_set.cpp)

# Exiv2 (from vcpkg)
find_package(exiv2 CONFIG REQUIRED)
target_link_libraries(exif_batch_set PRIVATE Exiv2::exiv2lib)

# Some extra warnings (safe defaults)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(exif_batch_set PRIVATE -Wall -Wextra -Wpedantic)
endif()

# On Windows, filesystem paths may contain UTF-8; Exiv2 takes std::string.
# (Nothing to do here, but keep for clarity.)
