# - Finds Wayland
# Find the Wayland libraries that are needed for UWAC
#
#  This module defines the following variables:
#     WAYLAND_FOUND     - true if UWAC has been found
#     WAYLAND_LIBS      - Set to the full path to wayland client libraries
#     WAYLAND_INCLUDE_DIR - Set to the include directories for wayland
#     XKBCOMMON_LIBS      - Set to the full path to xkbcommon libraries
#     XKBCOMMON_INCLUDE_DIR - Set to the include directories for xkbcommon
#

#=============================================================================
# Copyright 2015 David Fort <contact@hardening-consulting.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#=============================================================================

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(WAYLAND_SCANNER_PC wayland-scanner)
  pkg_check_modules(WAYLAND_CLIENT_PC wayland-client)
  pkg_check_modules(WAYLAND_CURSOR_PC wayland-cursor)
  pkg_check_modules(XKBCOMMON_PC xkbcommon)
endif()

find_program(WAYLAND_SCANNER wayland-scanner HINTS "${WAYLAND_SCANNER_PC_PREFIX}/bin")

find_path(WAYLAND_INCLUDE_DIR wayland-client.h HINTS ${WAYLAND_CLIENT_PC_INCLUDE_DIRS})

find_library(WAYLAND_CLIENT_LIB NAMES "wayland-client" HINTS "${WAYLAND_CLIENT_PC_LIBRARY_DIRS}")

find_library(WAYLAND_CURSOR_LIB NAMES "wayland-cursor" HINTS "${WAYLAND_CURSOR_PC_LIBRARY_DIRS}")

if(WAYLAND_CLIENT_LIB AND WAYLAND_CURSOR_LIB)
  list(APPEND WAYLAND_LIBS ${WAYLAND_CLIENT_LIB} ${WAYLAND_CURSOR_LIB})
endif(WAYLAND_CLIENT_LIB AND WAYLAND_CURSOR_LIB)

find_path(XKBCOMMON_INCLUDE_DIR xkbcommon/xkbcommon.h HINTS ${XKBCOMMON_PC_INCLUDE_DIRS})

find_library(XKBCOMMON_LIBS NAMES xkbcommon HINTS "${XKBCOMMON_PC_LIBRARY_DIRS}")

set(Wayland_SCANNER ${WAYLAND_SCANNER})
set(Wayland_INCLUDE_DIR ${WAYLAND_INCLUDE_DIR})
set(Wayland_LIBS ${WAYLAND_LIBS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Wayland
  DEFAULT_MSG
  Wayland_SCANNER
  Wayland_INCLUDE_DIR
  Wayland_LIBS
  XKBCOMMON_INCLUDE_DIR
  XKBCOMMON_LIBS
)

set(WAYLAND_FOUND ${Wayland_FOUND})
