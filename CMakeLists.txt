cmake_minimum_required(VERSION 3.25)
project(photo_cd_player VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

add_library(photocd_core
    src/PhotoCdAlbum.cpp
    src/PhotoCdExporter.cpp
    src/PcdDecoder.cpp
    third_party/pcdDecode/pcdDecode.cpp
)
target_include_directories(photocd_core PUBLIC src third_party/pcdDecode)
target_link_libraries(photocd_core PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(photocd_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(photo-cd-player
    src/main.cpp
    src/MainWindow.cpp
)
target_link_libraries(photo-cd-player PRIVATE photocd_core Qt6::Widgets Qt6::Concurrent)
target_compile_options(photo-cd-player PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(catalog-tests tests/test_catalog.cpp)
    target_link_libraries(catalog-tests PRIVATE photocd_core Qt6::Core)
    add_test(NAME catalog-tests COMMAND catalog-tests)
endif()

install(TARGETS photo-cd-player RUNTIME DESTINATION bin)
install(FILES packaging/photo-cd-player.desktop DESTINATION share/applications)
install(FILES packaging/photo-cd-player.desktop packaging/x-kodak-photo-cd.xml
        DESTINATION share/photo-cd-player)
install(PROGRAMS packaging/install-user-integration.sh DESTINATION share/photo-cd-player)
