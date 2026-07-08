# ---------------------------------------------------------------------------
# CMakeLists.txt for HelloWorldService
#
# This CMake file does 4 things:
#   1. Locates the pre-built libraries + toolchain in commonapi-qnx/build-rpi/
#   2. Runs the CommonAPI code generators on HelloWorld.fidl / HelloWorld.fdepl
#   3. Compiles the generated stub code + the server source
#   4. Links against CommonAPI, CommonAPI-SomeIP, and vsomeip3
#
# Prerequisites:
#   - Run commonapi-qnx/scripts/build.sh first (produces build-rpi/)
#   - Source commonapi-qnx/scripts/env.sh before running cmake
#
# Build:
#   source ../commonapi-qnx/scripts/env.sh
#   cmake -B build -S .
#   cmake --build build
# ---------------------------------------------------------------------------

# ===========================================================================
# 1. LOCATE TOOLCHAIN — MUST be before project() so CMake uses qcc/q++
# ===========================================================================
# Resolve LIBS_DIR — where commonapi-qnx lives
if(DEFINED ENV{LIBS_DIR})
    set(_LIBS_DIR "$ENV{LIBS_DIR}")
else()
    get_filename_component(_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../commonapi-qnx" ABSOLUTE)
endif()

# Resolve OUTPUT_DIR — where the libraries + toolchain + generators live
if(DEFINED ENV{OUTPUT_DIR})
    set(_OUTPUT_DIR "$ENV{OUTPUT_DIR}")
else()
    set(_OUTPUT_DIR "${_LIBS_DIR}/build-rpi")
endif()

# Load the QNX toolchain BEFORE project() so CMake detects qcc/q++
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${_OUTPUT_DIR}/toolchain.cmake")
endif()

# Now safe to call project() — CMake will use the QNX toolchain
cmake_minimum_required(VERSION 3.13)
project(HelloWorldService CXX)
set(CMAKE_CXX_STANDARD 14)

# Path to the interface directory (fidl/fdepl files)
set(_INTERFACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../interface")

# Tell find_package() to search OUTPUT_DIR for the installed libraries
list(APPEND CMAKE_PREFIX_PATH "${_OUTPUT_DIR}")

# Tell the linker where the .so files are (boost, vsomeip3, CommonAPI)
link_directories("${_OUTPUT_DIR}/lib")

# -rpath-link lets the linker resolve transitive .so dependencies at link time
# (libvsomeip3.so depends on libboost_thread.so which is in build-rpi/lib/)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath-link,${_OUTPUT_DIR}/lib")

# ===========================================================================
# 2. FIND PRE-BUILT LIBRARIES
# ===========================================================================
find_package(CommonAPI         CONFIG REQUIRED)
find_package(CommonAPI-SomeIP  CONFIG REQUIRED)
find_package(vsomeip3          CONFIG REQUIRED)

# ===========================================================================
# 3. CODE GENERATION — run the generators at configure time
# ===========================================================================
# The generators were unzipped by build.sh into:
#   OUTPUT_DIR/generators/core/commonapi-core-generator-linux-x86_64
#   OUTPUT_DIR/generators/someip/commonapi-someip-generator-linux-x86_64
# ---------------------------------------------------------------------------

set(_GEN_CORE_DIR    "${CMAKE_BINARY_DIR}/generated/core")
set(_GEN_SOMEIP_DIR  "${CMAKE_BINARY_DIR}/generated/someip")

find_program(CORE_GEN_EXE
    commonapi-core-generator-linux-x86_64
    PATHS "${_OUTPUT_DIR}/generators/core"
    REQUIRED)

find_program(SOMEIP_GEN_EXE
    commonapi-someip-generator-linux-x86_64
    PATHS "${_OUTPUT_DIR}/generators/someip"
    REQUIRED)

message(STATUS "Core generator:   ${CORE_GEN_EXE}")
message(STATUS "SOME/IP generator: ${SOMEIP_GEN_EXE}")

# --- Run the core generator on HelloWorld.fidl -----------------------------
# -sk  : generate skeleton (StubDefault) code — needed for the server
# Input:  interface/HelloWorld.fidl
# Output: server/build/generated/core/v0/commonapi/
#         - HelloWorldProxy.hpp          (proxy base — not used by server)
#         - HelloWorldStubDefault.hpp    (stub base — server inherits from this)
#         - HelloWorldStub.hpp
#         - HelloWorld.hpp               (interface header)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_GEN_CORE_DIR}"
    COMMAND "${CORE_GEN_EXE}" -sk -d "${_GEN_CORE_DIR}" "${_INTERFACE_DIR}/HelloWorld.fidl"
    RESULT_VARIABLE _core_result
)
if(NOT _core_result EQUAL 0)
    message(FATAL_ERROR "Core code generation failed (exit ${_core_result})")
endif()

# --- Run the SOME/IP generator on HelloWorld.fdepl -------------------------
# Input:  interface/HelloWorld.fdepl  (imports HelloWorld.fidl)
# Output: server/build/generated/someip/v0/commonapi/
#         - HelloWorldSomeIPStubAdapter.cpp   (stub adapter — links vsomeip)
#         - HelloWorldSomeIPDeployment.cpp     (serialization helpers)
#
# WORKING_DIRECTORY is set to interface/ because the .fdepl contains:
#     import "HelloWorld.fidl"
# which is a relative path — the generator must run from that directory.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_GEN_SOMEIP_DIR}"
    COMMAND "${SOMEIP_GEN_EXE}" -d "${_GEN_SOMEIP_DIR}" "HelloWorld.fdepl"
    WORKING_DIRECTORY "${_INTERFACE_DIR}"
    RESULT_VARIABLE _someip_result
)
if(NOT _someip_result EQUAL 0)
    message(FATAL_ERROR "SOME/IP code generation failed (exit ${_someip_result})")
endif()

message(STATUS "Generated code in ${CMAKE_BINARY_DIR}/generated/")

# ===========================================================================
# 4. COMPILE THE SERVER
# ===========================================================================

include_directories(
    "${_GEN_CORE_DIR}"
    "${_GEN_SOMEIP_DIR}"
)

file(GLOB_RECURSE CORE_STUB_SRCS
    "${_GEN_CORE_DIR}/v0/commonapi/*Stub*.cpp")

file(GLOB_RECURSE SOMEIP_SRCS
    "${_GEN_SOMEIP_DIR}/v0/commonapi/*.cpp")

add_executable(HelloWorldService
    src/HelloWorldService.cpp
    src/HelloWorldStubImpl.cpp
    ${CORE_STUB_SRCS}
    ${SOMEIP_SRCS}
)

target_link_libraries(HelloWorldService
    CommonAPI
    CommonAPI-SomeIP
    vsomeip3
)

# ===========================================================================
# 5. INSTALL
# ===========================================================================
install(TARGETS HelloWorldService DESTINATION bin)
install(FILES vsomeip.json DESTINATION bin RENAME vsomeip-service.json)
