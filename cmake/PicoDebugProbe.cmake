# If you are using a picoprobe, this code defines targets that
# encapsulate the openocd invocations to load binaries to the board,
# and to restart the running application.
#
# This has *nothing* to do with picow-http, and is not required.
#
# For information about picoprobe, see Appendix A of Getting Started:
# https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

# Without any configuration, CMake attempts to find openocd on your
# system. That usually works if it is installed at a standard
# location.
#
# Otherwise, you can pass in the path for openocd, either with
# -DOPENOCD_PATH=/path/to/openocd
# in the cmake invocation, or with the path in the OPENOCD_PATH
# environment variable.
if(NOT OPENOCD_PATH)
  if(DEFINED ENV{OPENOCD_PATH})
    set(OPENOCD_PATH $ENV{OPENOCD_PATH})
    message("Using OPENOCD_PATH from environment ('${OPENOCD_PATH}')")
  else()
    find_program(OPENOCD_PATH
      NAMES openocd
      DOC "path to openocd binary"
    )
  endif()
endif()

# If the openocd path can be found, you may also specify its search path
# (the value for openocd's -s/--search option) by setting
# -DOPENOCD_SEARCH_PATH=/path/to/openocd/config/dir
# in the cmake invocation, or in the OPENOCD_SEARCH_PATH environment
# variable. This may not be necessary for a standard installation,
# since openocd may find the path on its own.
if(OPENOCD_PATH)
  message(STATUS "openocd: ${OPENOCD_PATH}")

  if(DEFINED ENV{OPENOCD_SEARCH_PATH} AND(NOT OPENOCD_SEARCH_PATH))
    set(OPENOCD_SEARCH_PATH $ENV{OPENOCD_SEARCH_PATH})
    message("Using OPENOCD_SEARCH_PATH from environment ('${OPENOCD_SEARCH_PATH}')")
  endif()

  if(OPENOCD_SEARCH_PATH)
    message(STATUS "openocd search path: ${OPENOCD_SEARCH_PATH}")
    set(OPENOCD_SEARCH_ARG "-s ${OPENOCD_SEARCH_PATH}")
  else()
    set(OPENOCD_SEARCH_ARG "")
  endif()
endif()

# Creates a target to flash binary code to the board.
#
# CMakeLists.txt should include:
# picoprobe_add_flash_target(my_executable)
# for a target defined in add_executable().
# Then this target encapsulates the openocd invocation that loads the code:
# make flash-my_executable
function(picoprobe_add_flash_target EXECUTABLE_NAME)
  if(NOT OPENOCD_PATH)
    message(WARNING "not generating flash target for ${EXECUTABLE_NAME}: openocd not found or set")
    return()
  endif()

  message(STATUS "generating target flash-${EXECUTABLE_NAME}")
  add_custom_target(flash-${EXECUTABLE_NAME}
    COMMAND ${OPENOCD_PATH} ${OPENOCD_SEARCH_ARG} -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2350.cfg -c \"program ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}${CMAKE_EXECUTABLE_SUFFIX} verify reset exit\"
    DEPENDS ${EXECUTABLE_NAME}
    COMMENT "Flash ${EXECUTABLE_NAME}${CMAKE_EXECUTABLE_SUFFIX} with openocd"
  )
endfunction()

# Creates the target 'make reset' to restart a running application.
function(picoprobe_add_reset_target)
  if(NOT OPENOCD_PATH)
    message(WARNING "not generating reset target: openocd not found or set")
    return()
  endif()

  message(STATUS "generating target reset")
  add_custom_target(reset
    COMMAND ${OPENOCD_PATH} ${OPENOCD_SEARCH_ARG} -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2350.cfg -c \"init$<SEMICOLON> reset$<SEMICOLON> exit\"
    COMMENT "Reset with openocd"
  )
endfunction()
