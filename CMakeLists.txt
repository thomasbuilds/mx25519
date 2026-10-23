# Copyright (c) 2022-2026 tevador <tevador@gmail.com>
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

cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

set(MX25519_VERSION 1)

project(mx25519 C)

set(mx25519_sources
src/portable/scalarmult.c
src/cpu.c
src/impl.c
src/mx25519.c)

option(MX25519_COPYLEFT "Include the MPL-2.0 licensed assembly backends (ARM64, AMD64, AMD64X)" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "Setting default build type: ${CMAKE_BUILD_TYPE}")
endif()

if(MX25519_COPYLEFT)
message(STATUS "Assembly backends (MPL-2.0): enabled")
else()
message(STATUS "Assembly backends (MPL-2.0): disabled")
endif()

# Assembly backends. src/backends.h decides from the compiler's predefined
# macros which backend the C code calls, and the GNU assembly sources include
# it and assemble to nothing where it says no. CMake compiles the same header
# with the same compiler and flags to choose the sources to build. A source
# left out is reported to the C code, which then uses the portable backend.
include(CheckCSourceCompiles)
include(CMakePushCheckState)

function(mx25519_check_impl impl result)
  # nothing but the compiler and its flags (a parent project's check settings
  # are not ours), and compile only, so that cross toolchains without
  # libraries work too
  cmake_reset_check_state()
  set(CMAKE_REQUIRED_QUIET ON)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  # check again on every configure, in case the compiler flags changed
  unset(${result} CACHE)
  check_c_source_compiles("#include \"${CMAKE_CURRENT_SOURCE_DIR}/src/backends.h\"
#ifndef ${impl}
#error ${impl} is not defined
#endif
int main(void) { return 0; }" ${result})
endfunction()

set(mx25519_definitions)
set(mx25519_asm_amd64 OFF)
set(mx25519_asm_arm64 OFF)
if(NOT MX25519_COPYLEFT)
  list(APPEND mx25519_definitions MX25519_NO_COPYLEFT)
else()
  list(LENGTH CMAKE_OSX_ARCHITECTURES mx25519_osx_arch_count)
  if(APPLE AND (CMAKE_GENERATOR STREQUAL "Xcode" OR mx25519_osx_arch_count GREATER 1))
    # Xcode picks the architectures at build time, or there are several:
    # each architecture compiles both sources and assembles only its own
    set(mx25519_asm_amd64 ON)
    set(mx25519_asm_arm64 ON)
  else()
    mx25519_check_impl(IMPL_AMD64 MX25519_HAVE_IMPL_AMD64)
    if(MX25519_HAVE_IMPL_AMD64)
      set(mx25519_asm_amd64 ON)
    endif()
    # cl cannot assemble the GNU source, and Visual Studio compiles it as C
    if(NOT MSVC)
      mx25519_check_impl(IMPL_ARM64 MX25519_HAVE_IMPL_ARM64)
      if(MX25519_HAVE_IMPL_ARM64)
        set(mx25519_asm_arm64 ON)
      endif()
    endif()
  endif()
  message(STATUS "Assembly sources: AMD64 ${mx25519_asm_amd64}, ARM64 ${mx25519_asm_arm64}")

  if(mx25519_asm_amd64)
    if(MSVC)
      enable_language(ASM_MASM)
      list(APPEND mx25519_sources src/amd64/scalarmult_masm.asm)
      set_property(SOURCE src/amd64/scalarmult_masm.asm PROPERTY LANGUAGE ASM_MASM)
    else()
      list(APPEND mx25519_sources src/amd64/scalarmult_gnu.S)
      set_property(SOURCE src/amd64/scalarmult_gnu.S PROPERTY LANGUAGE C)
      set_property(SOURCE src/amd64/scalarmult_gnu.S PROPERTY XCODE_EXPLICIT_FILE_TYPE sourcecode.asm)
    endif()
  else()
    list(APPEND mx25519_definitions MX25519_NO_ASM_AMD64)
  endif()

  if(mx25519_asm_arm64)
    list(APPEND mx25519_sources src/arm64/scalarmult.S)
    set_property(SOURCE src/arm64/scalarmult.S PROPERTY LANGUAGE C)
    set_property(SOURCE src/arm64/scalarmult.S PROPERTY XCODE_EXPLICIT_FILE_TYPE sourcecode.asm)
  else()
    list(APPEND mx25519_definitions MX25519_NO_ASM_ARM64)
  endif()
endif()

set(MX25519_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE STRING "mx25519 include path")

add_library(mx25519 SHARED ${mx25519_sources})
set_property(TARGET mx25519 PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET mx25519 PROPERTY PUBLIC_HEADER include/mx25519.h)
target_include_directories(mx25519
  PUBLIC include/)
target_compile_definitions(mx25519 PRIVATE MX25519_SHARED)
set_target_properties(mx25519 PROPERTIES SOVERSION ${MX25519_VERSION})

add_library(mx25519_static STATIC ${mx25519_sources})
set_property(TARGET mx25519_static PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET mx25519_static PROPERTY PUBLIC_HEADER include/mx25519.h)
target_include_directories(mx25519_static
  PUBLIC include/)
set_target_properties(mx25519_static PROPERTIES OUTPUT_NAME mx25519)
target_compile_definitions(mx25519_static PUBLIC MX25519_STATIC)

if(mx25519_definitions)
  target_compile_definitions(mx25519 PRIVATE ${mx25519_definitions})
  target_compile_definitions(mx25519_static PRIVATE ${mx25519_definitions})
endif()

add_executable(mx25519-tests
  tests/tests.c)
target_compile_definitions(mx25519-tests PRIVATE MX25519_STATIC)
target_link_libraries(mx25519-tests
  PRIVATE mx25519_static)
set_property(TARGET mx25519-tests PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(mx25519-bench
  tests/bench.c
  src/platform.c)
target_include_directories(mx25519-bench PRIVATE src)
target_compile_definitions(mx25519-bench PRIVATE MX25519_STATIC)
target_link_libraries(mx25519-bench
  PRIVATE mx25519_static)
set_property(TARGET mx25519-bench PROPERTY POSITION_INDEPENDENT_CODE ON)
