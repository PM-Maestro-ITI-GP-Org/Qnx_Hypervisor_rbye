if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install"
           CACHE PATH "default install path" FORCE)
endif()

function(qnx_add_python)
  # Find Python includes
  find_path(PYTHON311_INCLUDE Python.h
      PATHS "$ENV{QNX_TARGET}/usr/include/python3.11")
  find_path(PYTHON313_INCLUDE Python.h
      PATHS "$ENV{QNX_TARGET}/usr/include/python3.13")
  # pyconfig.h is in a different location for python3.11
  if(PYTHON313_INCLUDE)
    find_path(PYTHON_CONFIG_INC pyconfig.h
        PATHS "$ENV{QNX_TARGET}/usr/include/python3.13")

    find_library(PYTHON_LIBRARIES NAMES python3.13)
    set(PYTHON_INCLUDE_DIRS ${PYTHON313_INCLUDE} PARENT_SCOPE)
    set(PYTHON_VERSION_MINOR 13 PARENT_SCOPE)
    message("-- Python version 3.13 found in SDP.")
  elseif(PYTHON311_INCLUDE)
    find_path(PYTHON_CONFIG_INC pyconfig.h PATHS
        "$ENV{QNX_TARGET}/usr/include/${CMAKE_SYSTEM_PROCESSOR}/python3.11")

    find_library(PYTHON_LIBRARY NAMES python3.11)
    set(PYTHON_INCLUDE_DIRS ${PYTHON311_INCLUDE} PARENT_SCOPE)
    set(PYTHON_VERSION_MINOR 11 PARENT_SCOPE)
    message("-- Python version 3.11 found in SDP.")
  else()
    message(FATAL_ERROR "No Python found in $ENV{QNX_TARGET}.")
  endif()
endfunction()

function(qnx_add_usemsg)
  find_program(USEMSG usemsg)
  if(USEMSG)
    string(TIMESTAMP BUILD_TIME "%Y/%m/%d-%H:%M:%S")
    cmake_host_system_information(RESULT HOST_NAME QUERY HOSTNAME)
    set(USER $ENV{USER})
    if(NOT STATE)
      set(STATE "Desktop")
    endif()
    configure_file(pinfo.in ${PROJECT_NAME}.pinfo @ONLY)
    add_custom_command(
      TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND usemsg -s __USAGENTO
      -s __USAGENTO
      -s __USAGE
      -iVERSION
      -iTAGID
      -iDESCRIPTION
      "${PROJECT_NAME}"
      "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.use"
    VERBATIM)
    unset(CURRENT_TIME CACHE)
  endif()

endfunction()
