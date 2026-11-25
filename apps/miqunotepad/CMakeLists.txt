cmake_minimum_required(VERSION 3.20)
project(miqunotepad LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

if(NOT TARGET PkgConfig::MIQUTOOLKIT)
    pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
endif()
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_executable(miqunotepad
    src/main.cpp
    src/notepad_app.cpp
    src/portal_file_chooser.cpp
)

target_include_directories(miqunotepad PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqunotepad PRIVATE
    PkgConfig::MIQUTOOLKIT
    PkgConfig::SYSTEMD
    Threads::Threads
    stdc++fs
)

# Local in-tree dev tree fallback
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqunotepad PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqunotepad PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqunotepad BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqunotepad BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqunotepad PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqunotepad RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqunotepad.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqunotepad)
install(FILES assets/miqunotepad.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miqunotepad.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
