# QNX aarch64le toolchain for Qt 6 cross-compilation
# Based on the Qt 6.10.2 QNX RPi4 cross-compilation guide
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=...

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 800)
set(CMAKE_SYSTEM_PROCESSOR aarch64le)

set(CMAKE_C_COMPILER qcc)
set(CMAKE_C_COMPILER_TARGET gcc_ntoaarch64le)
set(CMAKE_CXX_COMPILER q++)
set(CMAKE_CXX_COMPILER_TARGET gcc_ntoaarch64le)

set(CMAKE_FIND_ROOT_PATH
    $ENV{QNX_TARGET}
    $ENV{QNX_TARGET}/aarch64le
)

set(CMAKE_SYSROOT $ENV{QNX_TARGET})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER CACHE STRING "Package find root mode")

set(CMAKE_STRIP   $ENV{QNX_HOST}/usr/bin/ntoaarch64-strip)
set(CMAKE_AR      $ENV{QNX_HOST}/usr/bin/ntoaarch64-ar)
set(CMAKE_RANLIB  $ENV{QNX_HOST}/usr/bin/ntoaarch64-ranlib)
set(CMAKE_NM      $ENV{QNX_HOST}/usr/bin/ntoaarch64-nm)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# QNX 8.0 ships eventfd()/eventfd_read()/eventfd_write() in a standalone
# libeventfd.so (on Linux these live in libc). Qt6Core references them, so
# link libeventfd into every target or the QNX build fails at link time with
# "undefined reference to `eventfd'".
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT    " -leventfd")
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " -leventfd")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " -leventfd")
