cmake_minimum_required(VERSION 3.16...3.31)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RCLONE_BROWSER_VERSION LIMIT_COUNT 1)
string(STRIP "${RCLONE_BROWSER_VERSION}" RCLONE_BROWSER_VERSION)

project(rclone-browser VERSION ${RCLONE_BROWSER_VERSION} LANGUAGES CXX)
if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Qt 6 is preferred; Qt 5.15 is still accepted so distro packagers on older
# LTS releases are not left behind. Force one with -DRB_QT_MAJOR=5 or 6.
set(RB_QT_MAJOR "" CACHE STRING "Force Qt major version (5 or 6); empty = auto")
if(RB_QT_MAJOR)
  find_package(Qt${RB_QT_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network)
  set(QT_VERSION_MAJOR ${RB_QT_MAJOR})
else()
  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network)
endif()
if(WIN32 AND QT_VERSION_MAJOR LESS 6)
  message(FATAL_ERROR "Windows builds require Qt 6 (6.8 or newer recommended)")
endif()
if(QT_VERSION_MAJOR EQUAL 5 AND Qt5Core_VERSION VERSION_LESS 5.15)
  message(FATAL_ERROR "Qt 5.15 or newer is required (found ${Qt5Core_VERSION})")
endif()
message(STATUS "Building Rclone Browser ${RCLONE_BROWSER_VERSION} with Qt ${QT_VERSION_MAJOR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build")

enable_testing()

add_subdirectory(src)
