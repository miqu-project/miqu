cmake_minimum_required(VERSION 3.20)
project(miqu VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Global options for selective subsystem builds
option(BUILD_TOOLKIT "Build miqutoolkit UI framework" ON)
option(BUILD_COMPOSITOR "Build miquland Wayland compositor" ON)
option(BUILD_SHELL "Build Miqu desktop shell components & daemons" ON)
option(BUILD_APPS "Build Miqu standalone applications" ON)

# 1. miqutoolkit (Core UI library, defined first for in-tree linking)
if(BUILD_TOOLKIT)
    message(STATUS "==> Configuring miqutoolkit...")
    add_subdirectory(miqutoolkit)
endif()

# 2. miquland (Wayland compositor)
if(BUILD_COMPOSITOR)
    message(STATUS "==> Configuring miquland...")
    add_subdirectory(miquland)
endif()

# 3. shell (Desktop Shell UI & Daemons)
if(BUILD_SHELL)
    message(STATUS "==> Configuring desktop shell components...")
    add_subdirectory(shell)
endif()

# 4. apps (First-party Desktop Applications)
if(BUILD_APPS)
    message(STATUS "==> Configuring desktop applications...")
    add_subdirectory(apps)
endif()
