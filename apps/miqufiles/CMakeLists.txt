cmake_minimum_required(VERSION 3.20)
project(miqufiles LANGUAGES C CXX)

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

add_executable(miqufiles
    src/main.cpp
    src/files_app.cpp
    src/miller_view.cpp
    src/files_model.cpp
    src/preview_files.cpp
    src/file_opener.cpp
)

target_include_directories(miqufiles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqufiles PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
    stdc++fs
)

# Local in-tree dev tree fallback
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqufiles PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqufiles PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqufiles BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqufiles BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqufiles PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqufiles RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqufiles.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqufiles)
install(FILES assets/miqufiles.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miqufiles.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
