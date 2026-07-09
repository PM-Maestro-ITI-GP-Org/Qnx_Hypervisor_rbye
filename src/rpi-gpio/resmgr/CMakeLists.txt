cmake_minimum_required(VERSION 3.22.1)

include(../common.cmake)
project(rpi_gpio DESCRIPTION "GPIO resource manager for Raspberry Pi 3/4")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(RESMGR_INSTALL_MESSAGE "")
elseif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
  set(RESMGR_INSTALL_MESSAGE "Installing rpi_gpio resource manager. Please restart the device once this installation is finished...\n")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/public/)
set(SRCS event.c main.c pwm.c rp1_gpio.c rpi2711_gpio.c)
set(PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/public/sys/rpi_gpio.h)

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} PUBLIC login)
target_link_libraries(${PROJECT_NAME} PUBLIC secpol)

qnx_add_usemsg()

install(CODE "message(\"${RESMGR_INSTALL_MESSAGE}\")")
if(DEFINED ENV{QNX_TARGET})
  install(TARGETS ${PROJECT_NAME} DESTINATION
    ${CMAKE_SYSTEM_PROCESSOR}/sbin)
  install(FILES ${PUBLIC_HEADER} DESTINATION usr/include/sys)
else()
  install(TARGETS ${PROJECT_NAME} DESTINATION sbin)
  install(FILES ${PUBLIC_HEADER} DESTINATION include/sys)
endif()
