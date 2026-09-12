# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(ENV{PICO_SDK_PATH} "/Volumes/Development/eric/pico/pico-sdk")

set(PICO_BOARD pico CACHE STRING "rp2040")

cmake_minimum_required(VERSION 3.13)

# Point this at your pico-sdk checkout, or set PICO_SDK_PATH as an env var
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(loopback C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(_WIZCHIP_=5500)

pico_sdk_init()

# --- WIZnet ioLibrary_Driver ---
# git clone https://github.com/Wiznet/ioLibrary_Driver.git  (as a sibling dir, or a submodule)

set(WIZNET_DIR ${CMAKE_SOURCE_DIR}/ioLibrary_Driver)
set(WIZNET_BASE_DIR /Volumes/Development/eric/pico/WIZnet-PICO-C)
set(WIZNET_LIB_DIR /Volumes/Development/eric/pico/WIZnet-PICO-C/build/libraries)
set(WIZNET_PORT_DIR /Volumes/Development/eric/pico/WIZnet-PICO-C/build/port)
set(WIZNET_BUILD_DIR /Volumes/Development/eric/pico/WIZnet-PICO-C/build)

add_executable(pico_w5500_tcp_server wizchip_loopback.c dhcp.c ../gpsLib/src/gpsLib.cpp)

target_include_directories(pico_w5500_tcp_server PRIVATE
    ${WIZNET_DIR}/Ethernet
    ${WIZNET_DIR}/Ethernet/W5500
    ${WIZNET_DIR}/Internet
    ${WIZNET_BASE_DIR}/port
    ${WIZNET_BASE_DIR}/port/timer
    ${WIZNET_BASE_DIR}/port/ioLibrary_Driver/inc
    ${WIZNET_BASE_DIR}/libraries/ioLibrary_Driver/Ethernet
    ${WIZNET_BASE_DIR}/libraries/ioLibrary_Driver/Application/loopback
    ${WIZNET_BASE_DIR}/libraries/ioLibrary_Driver/Internet/DNS
    ${WIZNET_DIR}/Internet/DHCP
    ${WIZNET_BASE_DIR}/libraries/ioLibrary_Driver/Internet/DHCP
    ./
    ../gpsLib/src
)

# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/port \
# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/port/timer \
# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/port/ioLibrary_Driver/inc \
# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet \
# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W5500 \
# -I/Volumes/Development/eric/pico/WIZnet-PICO-C/libraries/ioLibrary_Driver/Application/loopback \

# Only the pieces we actually need: core W5500 driver + socket API + DHCP client
file(GLOB WIZNET_SOURCES
    ${WIZNET_DIR}/Ethernet/*.c
    ${WIZNET_DIR}/Ethernet/W5500/*.c
    ${WIZNET_DIR}/Internet/DHCP/*.c
)

#add_library(dhcp_override OBJECT dhcp.c)

target_sources(pico_w5500_tcp_server PRIVATE ${WIZNET_SOURCES})

# Tell the ioLibrary_Driver which chip we're using so it selects the SPI
# interface (this is what defines WIZCHIP_READ/WIZCHIP_WRITE correctly;
# without it you get "undefined reference to `WIZCHIP_READ'" at link time)
target_compile_definitions(pico_w5500_tcp_server PRIVATE
    _WIZCHIP_=5500
)

target_link_libraries(pico_w5500_tcp_server
    #dhcp_override
    pico_stdlib
    pico_rand
    hardware_spi
    hardware_uart
    hardware_dma
    hardware_pio
    pico_unique_id
    # ${WIZNET_LIB_DIR}/libETHERNET_FILES.a
    # ${WIZNET_PORT_DIR}/libIOLIBRARY_FILES.a
    # ${WIZNET_PORT_DIR}/libBOARD_FILES.a
    # ${WIZNET_PORT_DIR}/libPORT_COMMON_FILES.a
    # ${WIZNET_PORT_DIR}/libTIMER_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libETHERNET_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libDNS_FILES.a
  ${WIZNET_BUILD_DIR}/port/libIOLIBRARY_FILES.a
  ${WIZNET_BUILD_DIR}/port/libPORT_COMMON_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libLOOPBACK_FILES.a
  ${WIZNET_BUILD_DIR}/port/libTIMER_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libW5500_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libETHERNET_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libW5500_FILES.a
  ${WIZNET_BUILD_DIR}/libraries/libDHCP_FILES.a
)

string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--print-memory-usage")

# USB serial for printf(), no UART needed
pico_enable_stdio_usb(pico_w5500_tcp_server 0)
pico_enable_stdio_uart(pico_w5500_tcp_server 1)

pico_add_extra_outputs(pico_w5500_tcp_server)
