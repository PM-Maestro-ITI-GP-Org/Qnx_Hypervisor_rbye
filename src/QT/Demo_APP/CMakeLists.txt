cmake_minimum_required(VERSION 3.21)

project(QtQuick2DDemo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Qt6 Library Path Configuration
# ============================================

if(NOT DEFINED Qt6_DIR)
    message(FATAL_ERROR "Qt6_DIR not set. Please provide it via cmake:\n"
            "  cmake -S . -B <build_dir> -DQt6_DIR=<path_to_libs>/lib/cmake/Qt6")
endif()

message(STATUS "Using Qt6_DIR: ${Qt6_DIR}")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
)

message(STATUS "Qt6 found!")

# ============================================
# Deploy paths (derive from Qt6_DIR)
# ============================================
get_filename_component(QT6_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
set(QT6_LIB_DIR "${QT6_PREFIX}/lib" CACHE PATH "Qt6 shared libraries")
set(QT6_QML_DIR "${QT6_PREFIX}/qml" CACHE PATH "Qt6 QML imports")

set(QT_DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy" CACHE PATH "Deploy output dir")
set(DEPLOY_LIB_DIR "${QT_DEPLOY_DIR}/lib")
set(DEPLOY_QML_DIR "${QT_DEPLOY_DIR}/qml")
set(DEPLOY_PLUGIN_DIR "${QT_DEPLOY_DIR}/plugins")
set(DEPLOY_FONTS_DIR "${QT_DEPLOY_DIR}/lib/fonts")

# QNX target sysroot path
set(QNX_TARGET "$ENV{QNX_TARGET}" CACHE PATH "QNX target sysroot")
set(QNX_ARCH "aarch64le" CACHE STRING "QNX target architecture")
set(QNX_LIB_DIR "${QNX_TARGET}/${QNX_ARCH}" CACHE PATH "QNX arch root directory")
set(QT6_PLUGIN_DIR "${QT6_PREFIX}/plugins" CACHE PATH "Qt6 plugins directory")
set(FONT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../qt6-qnx-libs/fonts" CACHE PATH "Directory containing font files")

# ============================================
# Application Source Files
# ============================================

add_executable(QtQuick2DDemo
    main.cpp
    resources.qrc
)

target_link_libraries(QtQuick2DDemo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

# ============================================
# Runtime Library Path Configuration
# ============================================

set_target_properties(QtQuick2DDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${QT_DEPLOY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH "${QT6_LIB_DIR}"
)

# ============================================
# Deploy: copy only needed Qt + system deps
# ============================================
add_custom_command(TARGET QtQuick2DDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DBIN=$<TARGET_FILE:QtQuick2DDemo>
        -DPROJ_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DQT_LIB_DIR=${QT6_LIB_DIR}
        -DDEPLOY_LIB_DIR=${DEPLOY_LIB_DIR}
        -DQT_QML_DIR=${QT6_QML_DIR}
        -DDEPLOY_QML_DIR=${DEPLOY_QML_DIR}
        -DQT_PLUGIN_DIR=${QT6_PLUGIN_DIR}
        -DDEPLOY_PLUGIN_DIR=${DEPLOY_PLUGIN_DIR}
        -DDEPLOY_FONTS_DIR=${DEPLOY_FONTS_DIR}
        -DFONT_SOURCE_DIR=${FONT_SOURCE_DIR}
        -DQNX_LIB_DIR=${QNX_LIB_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/deploy_qt.cmake
    COMMENT "Deploying minimal Qt dependencies to ${QT_DEPLOY_DIR}"
)

# Generate a launcher
file(GENERATE
    OUTPUT ${QT_DEPLOY_DIR}/run.sh
    CONTENT
"#!/bin/sh
DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"
export LD_LIBRARY_PATH=\"$DIR/lib:$LD_LIBRARY_PATH\"
export QML2_IMPORT_PATH=\"$DIR/qml\"
export QT_QPA_PLATFORM_PLUGIN_PATH=\"$DIR/plugins\"
export QT_QUICK_BACKEND=\"software\"
export QQNX_PHYSICAL_SCREEN_SIZE=\"\${QQNX_PHYSICAL_SCREEN_SIZE:-150,90}\"
export APP_WIDTH=\"\${APP_WIDTH:-1280}\"
export APP_HEIGHT=\"\${APP_HEIGHT:-720}\"
export FONTCONFIG_FILE=\"$DIR/lib/fonts/fonts.conf\"
exec \"$DIR/QtQuick2DDemo\" \"\$@\"
"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

message(STATUS "Build complete!")
message(STATUS "Binary: ${QT_DEPLOY_DIR}/QtQuick2DDemo")
message(STATUS "Run with: ${QT_DEPLOY_DIR}/run.sh")
