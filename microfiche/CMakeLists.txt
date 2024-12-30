cmake_minimum_required(VERSION 3.23)
cmake_policy(SET CMP0169 OLD)

include(FetchContent)
FetchContent_Declare(pico-sdk
  GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
  GIT_TAG 2.1.0
)
if(NOT depname_pico-sdk)
  FetchContent_Populate(pico-sdk)
endif()

include("${pico-sdk_SOURCE_DIR}/pico_sdk_init.cmake")
project(microfiche LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)

pico_sdk_init()

# FetchContent_Declare(picow_http
#   GIT_REPOSITORY https://gitlab.com/slimhazard/picow_http.git
#   GIT_TAG 1.1.0
# )
# FetchContent_MakeAvailable(picow_http)

# add_library(lwipopts INTERFACE)
# target_sources(lwipopts INTERFACE
#     "${picow_http_SOURCE_DIR}/etc/lwipopts.h")
# target_include_directories(lwipopts INTERFACE
#     "${picow_http_SOURCE_DIR}/etc")

add_executable(microfiche)
target_sources(microfiche PRIVATE
    "include/lwipopts.h"
    "include/encre_file.h"
    "src/encre_file.c"
    "include/GDEP073E01.h"
    "src/GDEP073E01.c"
    "src/main.c"
    # "src/webserver.c"
    )
target_include_directories(microfiche PRIVATE "include")
target_compile_definitions(microfiche PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=10000
    PICOW_HTTPS=0)
target_link_libraries(microfiche PRIVATE
    hardware_spi
    pico_unique_id
    pico_multicore
    pico_stdio
    pico_stdlib
    pico_lwip
    pico_lwip_arch
    pico_lwip_mdns
    pico_cyw43_arch_lwip_poll
    # lwipopts
    # picow_http
    )

pico_enable_stdio_usb(microfiche ON)
pico_enable_stdio_uart(microfiche OFF)

pico_add_extra_outputs(microfiche)

pico_set_program_description(microfiche "Affiche for the Raspberry Pi Pico 2 W")
pico_set_program_url(microfiche "https://github.com/DDoS/Cadre")

include("../cmake/PicoDebugProbe.cmake")
picoprobe_add_flash_target(microfiche)
picoprobe_add_reset_target(microfiche)
