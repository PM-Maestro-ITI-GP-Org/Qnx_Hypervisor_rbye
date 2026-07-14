# deploy_qt.cmake — deploy only the needed Qt + system dependencies
# Usage: cmake -DBIN=... -DQT_LIB_DIR=... -DDEPLOY_LIB_DIR=...
#              -DQT_QML_DIR=... -DDEPLOY_QML_DIR=...
#              -DQT_PLUGIN_DIR=... -DDEPLOY_PLUGIN_DIR=...
#              -DQNX_LIB_DIR=... -DPROJ_SOURCE_DIR=... -P deploy_qt.cmake

# ============================================
# 1. Qt shared libraries (needed for 2D app)
# ============================================
set(QT_LIBS
    libQt6Core.so
    libQt6Gui.so
    libQt6DBus.so
    libQt6Network.so
    libQt6Qml.so
    libQt6QmlMeta.so
    libQt6QmlModels.so
    libQt6QmlWorkerScript.so
    libQt6Quick.so
    libQt6QuickControls2.so
    libQt6QuickTemplates2.so
    libQt6QuickControls2Impl.so
    libQt6QuickControls2Basic.so
    libQt6QuickControls2BasicStyleImpl.so
)

file(MAKE_DIRECTORY ${DEPLOY_LIB_DIR})
foreach(_lib ${QT_LIBS})
    file(GLOB _matches "${QT_LIB_DIR}/${_lib}*")
    if(_matches)
        execute_process(COMMAND cp -a ${_matches} ${DEPLOY_LIB_DIR}/)
    endif()
endforeach()

# ============================================
# 2. System libraries from QNX target
# ============================================
set(SYS_LIBS
    libsocket.so
    libc++.so
    libc.so
    libm.so
    libgcc_s.so
    libfsnotify.so
    libicui18n.so
    libicuuc.so
    libicudata.so
    libzstd.so
    libz.so
    libslog2.so
    libscreen.so
)

if(DEFINED QNX_LIB_DIR)
    set(_qnx_lib_paths
        "${QNX_LIB_DIR}/lib"
        "${QNX_LIB_DIR}/usr/lib"
    )

    foreach(_lib ${SYS_LIBS})
        foreach(_dir ${_qnx_lib_paths})
            file(GLOB _matches "${_dir}/${_lib}*")
            if(_matches)
                execute_process(COMMAND cp -a ${_matches} ${DEPLOY_LIB_DIR}/)
                break()
            endif()
        endforeach()
    endforeach()

    # Remove broken symlinks that cp -a may have preserved
    execute_process(COMMAND find ${DEPLOY_LIB_DIR} -type l ! -exec test -e {} \; -delete)
endif()

# ============================================
# 3. QNX platform plugin
# ============================================
if(DEFINED QT_PLUGIN_DIR)
    file(MAKE_DIRECTORY ${DEPLOY_PLUGIN_DIR}/platforms)
    file(GLOB _plugin "${QT_PLUGIN_DIR}/platforms/libqqnx.so*")
    if(_plugin)
        execute_process(COMMAND cp -a ${_plugin} ${DEPLOY_PLUGIN_DIR}/platforms/)
        message(STATUS "Deployed QNX platform plugin")
    endif()
endif()

# ============================================
# 4. QML modules (QtQuick + QtQuick.Controls)
# ============================================
file(MAKE_DIRECTORY ${DEPLOY_QML_DIR})
foreach(_mod QtQuick QtQuick.Controls QtQml)
    if(EXISTS "${QT_QML_DIR}/${_mod}")
        file(COPY "${QT_QML_DIR}/${_mod}" DESTINATION ${DEPLOY_QML_DIR})
        message(STATUS "Deployed QML: ${_mod}")
    endif()
endforeach()

# ============================================
# 5. Fonts (at least one .ttf for text rendering)
# ============================================
if(DEFINED PROJ_SOURCE_DIR)
    file(MAKE_DIRECTORY ${DEPLOY_FONTS_DIR})
    file(GLOB _fonts "${PROJ_SOURCE_DIR}/fonts/*.ttf" "${PROJ_SOURCE_DIR}/fonts/*.ttc")
    if(_fonts)
        file(COPY ${_fonts} DESTINATION ${DEPLOY_FONTS_DIR})
        message(STATUS "Deployed fonts from ${PROJ_SOURCE_DIR}/fonts/")
    endif()
endif()

message(STATUS "Deploy complete — only required dependencies")
