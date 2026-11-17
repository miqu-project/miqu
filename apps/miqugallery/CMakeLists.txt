cmake_minimum_required(VERSION 3.20)
project(miqugallery LANGUAGES C CXX)

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

add_executable(miqugallery
    src/main.cpp
    src/gallery_app.cpp
    src/miller_view.cpp
    src/gallery_model.cpp
    src/preview_gallery.cpp
)

target_include_directories(miqugallery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miqugallery PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
    stdc++fs
)

# Local in-tree dev tree fallback
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqugallery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    set_target_properties(miqugallery PROPERTIES INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build;${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miqugallery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    set_target_properties(miqugallery PROPERTIES INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build;${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miqugallery BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miqugallery BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miqugallery PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miqugallery RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miqugallery.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miqugallery)
install(FILES assets/miqugallery.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miqugallery.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
