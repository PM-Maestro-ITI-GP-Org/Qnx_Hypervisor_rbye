cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0009 NEW)
# deploy_qt.cmake — deploy only the needed Qt + system dependencies
# Usage: cmake -DBIN=... -DQT_LIB_DIR=... -DDEPLOY_LIB_DIR=...
#              -DQT_QML_DIR=... -DDEPLOY_QML_DIR=...
#              -DQT_PLUGIN_DIR=... -DDEPLOY_PLUGIN_DIR=...
#              -DQNX_LIB_DIR=... -DPROJ_SOURCE_DIR=... -P deploy_qt.cmake

# ============================================
# 1. Qt shared libraries
#    No fixed list — required libQt6* libs are discovered by the
#    recursive dependency resolution in step 6, starting from the app
#    binary (+ QML plugin .so files) and following NEEDED entries.
# ============================================
file(MAKE_DIRECTORY ${DEPLOY_LIB_DIR})

# ============================================
# 2. System libraries from QNX target
#    No fixed list — all required system libs are pulled in by the
#    recursive dependency resolution in step 6, so only what the app
#    actually needs (transitively) is deployed.
# ============================================

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
# 3.1 Qt image format plugins (SVG, JPEG, etc.)
# ============================================
if(DEFINED QT_PLUGIN_DIR)
    file(MAKE_DIRECTORY ${DEPLOY_PLUGIN_DIR})
    file(GLOB _imgplugins "${QT_PLUGIN_DIR}/imageformats/libqsvg.so*")
    if(_imgplugins)
        file(COPY ${QT_PLUGIN_DIR}/imageformats DESTINATION ${DEPLOY_PLUGIN_DIR})
        message(STATUS "Deployed image format plugins (SVG, etc.)")
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
# 5. Fonts (read fonts.txt, copy from FONT_SOURCE_DIR)
# ============================================
if(DEFINED PROJ_SOURCE_DIR AND DEFINED FONT_SOURCE_DIR)
    file(MAKE_DIRECTORY ${DEPLOY_FONTS_DIR})
    set(_fonts_txt "${PROJ_SOURCE_DIR}/fonts.txt")
    if(EXISTS "${_fonts_txt}")
        file(STRINGS "${_fonts_txt}" _font_list)
        foreach(_font ${_font_list})
            file(GLOB_RECURSE _matches "${FONT_SOURCE_DIR}/${_font}")
            if(_matches)
                file(COPY ${_matches} DESTINATION ${DEPLOY_FONTS_DIR})
                message(STATUS "Deployed font: ${_font}")
            else()
                message(WARNING "Font not found: ${_font} in ${FONT_SOURCE_DIR}")
            endif()
        endforeach()
    endif()
    # Generate minimal fonts.conf so fontconfig doesn't complain
    file(WRITE "${DEPLOY_FONTS_DIR}/fonts.conf"
"<?xml version=\"1.0\"?>
<fontconfig>
  <dir prefix=\"relative\">.</dir>
  <cachedir>/tmp/fontconfig-cache</cachedir>
</fontconfig>
")
endif()

# ============================================
# 6. Recursive system-library dependency resolution
#    Walk every deployed lib + the app binary, read its NEEDED entries,
#    and pull any missing libs from the QNX SDK / Qt lib dir.
#    Repeat until the dependency set is closed.
# ============================================
if(DEFINED QNX_LIB_DIR)
    set(_search_dirs
        "${QT_LIB_DIR}"
        "${QNX_LIB_DIR}/lib"
        "${QNX_LIB_DIR}/usr/lib"
        "${DEPLOY_LIB_DIR}"
    )

    # Seed worklist with the app binary + all deployed .so files
    # (lib/ + qml/ + plugins/) so NEEDED chains are fully resolved.
    set(_worklist "")
    if(DEFINED BIN AND EXISTS "${BIN}")
        list(APPEND _worklist "${BIN}")
    endif()
    file(GLOB_RECURSE _seed "${DEPLOY_LIB_DIR}/*.so*")
    list(APPEND _worklist ${_seed})
    file(GLOB_RECURSE _qmlseed "${DEPLOY_QML_DIR}/*.so*")
    list(APPEND _worklist ${_qmlseed})
    file(GLOB_RECURSE _plugseed "${DEPLOY_PLUGIN_DIR}/*.so*")
    list(APPEND _worklist ${_plugseed})

    set(_processed "")

    while(_worklist)
        list(GET _worklist 0 _cur)
        list(REMOVE_AT _worklist 0)
        list(FIND _processed "${_cur}" _already)
        if(_already GREATER -1)
            continue()
        endif()
        list(APPEND _processed "${_cur}")

        # Read NEEDED sonames (prefer objdump, fall back to readelf)
        execute_process(COMMAND objdump -p "${_cur}"
            OUTPUT_VARIABLE _od ERROR_QUIET)
        if(_od STREQUAL "")
            execute_process(COMMAND readelf -d "${_cur}"
                OUTPUT_VARIABLE _od ERROR_QUIET)
        endif()
        if(_od STREQUAL "")
            continue()
        endif()

        # objdump: "  NEEDED               libfontconfig.so.17"
        # readelf: " (NEEDED) Shared library: [libfontconfig.so.17]"
        set(_needed "")
        if(_od MATCHES "NEEDED[ \t]+[A-Za-z0-9_.-]+")
            string(REGEX MATCHALL "NEEDED[ \t]+([A-Za-z0-9_.-]+)" _needed "${_od}")
            string(REGEX REPLACE "NEEDED[ \t]+" "" _needed "${_needed}")
        else()
            string(REGEX MATCHALL "\\(NEEDED\\)[ \t]+Shared library:[ \t]+\\[([^]]+)\\]" _needed "${_od}")
            string(REGEX REPLACE "\\(NEEDED\\)[ \t]+Shared library:[ \t]+\\[" "" _needed "${_needed}")
            string(REGEX REPLACE "\\]" "" _needed "${_needed}")
        endif()

        foreach(_soname ${_needed})
            if(EXISTS "${DEPLOY_LIB_DIR}/${_soname}")
                continue()
            endif()
            set(_found "")
            foreach(_sd ${_search_dirs})
                file(GLOB _m "${_sd}/${_soname}*")
                set(_copied "")
                foreach(_f ${_m})
                    # Only deploy shared libs (.so, .so.N, .so.N.M);
                    # skip static .a archives and QNX .sym symbol files.
                    if(_f MATCHES "\\.so[0-9.]*$")
                        execute_process(COMMAND cp -a "${_f}" "${DEPLOY_LIB_DIR}/")
                        set(_copied 1)
                    endif()
                endforeach()
                if(_copied)
                    list(APPEND _worklist "${DEPLOY_LIB_DIR}/${_soname}")
                    set(_found 1)
                    break()
                endif()
            endforeach()
            if(NOT _found)
                message(WARNING "Recursive deploy: '${_soname}' (needed by ${_cur}) not found in search paths")
            endif()
        endforeach()
    endwhile()

    # Remove broken symlinks that cp -a may have preserved
    execute_process(COMMAND find ${DEPLOY_LIB_DIR} -type l ! -exec test -e {} \; -delete)
endif()

message(STATUS "Deploy complete — only required dependencies")
