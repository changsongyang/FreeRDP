# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2024 Armin Novak <anovak@thincast.com>
# Copyright 2024 Thincast Technologies GmbH
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
set(SRCS
    sdl_button.hpp
    sdl_button.cpp
    sdl_buttons.hpp
    sdl_buttons.cpp
    sdl_dialogs.cpp
    sdl_dialogs.hpp
    sdl_widget.hpp
    sdl_widget.cpp
    sdl_widget_list.hpp
    sdl_widget_list.cpp
    sdl_input_widget.hpp
    sdl_input_widget.cpp
    sdl_input_widget_pair.hpp
    sdl_input_widget_pair.cpp
    sdl_input_widget_pair_list.hpp
    sdl_input_widget_pair_list.cpp
    sdl_select.hpp
    sdl_select.cpp
    sdl_select_list.hpp
    sdl_select_list.cpp
    sdl_selectable_widget.cpp
    sdl_selectable_widget.hpp
    sdl_connection_dialog.cpp
    sdl_connection_dialog.hpp
    sdl_connection_dialog_wrapper.cpp
    sdl_connection_dialog_wrapper.hpp
    sdl_blend_mode_guard.cpp
    sdl_blend_mode_guard.hpp
)

list(APPEND LIBS sdl3_client_res winpr)

if(NOT WITH_SDL_LINK_SHARED)
  list(APPEND LIBS ${SDL3_STATIC_LIBRARIES})
else()
  list(APPEND LIBS ${SDL3_LIBRARIES})
endif()

macro(find_sdl_component name)
  find_package(${name} REQUIRED)
  if(WITH_SDL_LINK_SHARED)
    list(APPEND LIBS ${name}::${name})
    set_target_properties(${name}::${name}-shared PROPERTIES SYSTEM TRUE)
  else()
    list(APPEND LIBS ${name}::${name}-static)
    set_target_properties(${name}::${name}-static PROPERTIES SYSTEM TRUE)
  endif()

endmacro()

find_sdl_component(SDL3_ttf)

option(WITH_SDL_IMAGE_DIALOGS "Build with SDL_image support (recommended)" OFF)
if(WITH_SDL_IMAGE_DIALOGS)
  find_sdl_component(SDL3_image)
  add_compile_definitions(WITH_SDL_IMAGE_DIALOGS)
endif()

add_subdirectory(res)

add_library(sdl3-dialogs STATIC ${SRCS} sdl_connection_dialog_hider.hpp sdl_connection_dialog_hider.cpp)

set_property(TARGET sdl3-dialogs PROPERTY FOLDER "Client/SDL")
target_link_libraries(sdl3-dialogs PRIVATE ${LIBS})

option(SDL_DIALOG_TEST "Build dialog test binaries" OFF)
if(SDL_DIALOG_TEST)
  add_subdirectory(test)
endif()
