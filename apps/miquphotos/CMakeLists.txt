cmake_minimum_required(VERSION 3.20)
project(miquphotos LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build")
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

if(NOT TARGET PkgConfig::MIQUTOOLKIT)
    pkg_check_modules(MIQUTOOLKIT REQUIRED IMPORTED_TARGET miqutoolkit)
endif()

add_executable(miquphotos
    src/main.cpp
    src/photos_app.cpp
    src/miller_view.cpp
    src/photos_model.cpp
    src/preview_photos.cpp
)

target_include_directories(miquphotos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(miquphotos PRIVATE
    PkgConfig::MIQUTOOLKIT
    Threads::Threads
    stdc++fs
)

# Local in-tree dev tree fallback
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miquphotos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build/libmiqutoolkit.so")
    set_target_properties(miquphotos PROPERTIES INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/build;${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    target_link_libraries(miquphotos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build/libmiqutoolkit.so")
    set_target_properties(miquphotos PROPERTIES INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/build;${CMAKE_INSTALL_PREFIX}/lib;$ORIGIN/../lib")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
    target_include_directories(miquphotos BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../miqutoolkit/include")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
    target_include_directories(miquphotos BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../miqutoolkit/include")
endif()

target_compile_options(miquphotos PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

include(GNUInstallDirs)
install(TARGETS miquphotos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/miquphotos.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/miquphotos)
install(FILES assets/miquphotos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/miquphotos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
