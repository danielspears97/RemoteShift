# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
# RemoteShift input engine: platform-neutral keyboard/mouse -> DualSense translation.
cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(rsinput CXX)
	enable_testing()
	set(RSINPUT_STANDALONE ON)
endif()

add_library(rsinput STATIC
	src/keycodes.cpp
	src/profile.cpp
	src/mousestick.cpp
	src/engine.cpp
	include/rsinput/keycodes.h
	include/rsinput/profile.h
	include/rsinput/mousestick.h
	include/rsinput/engine.h)
target_include_directories(rsinput PUBLIC include PRIVATE third_party)
target_compile_features(rsinput PUBLIC cxx_std_17)
set_target_properties(rsinput PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
	target_compile_options(rsinput PRIVATE /W4 /utf-8)
else()
	target_compile_options(rsinput PRIVATE -Wall -Wextra)
endif()

option(RSINPUT_ENABLE_TESTS "Build RemoteShift input engine tests" ON)
if(RSINPUT_ENABLE_TESTS)
	add_executable(rsinput_tests tests/test_main.cpp)
	target_link_libraries(rsinput_tests rsinput)
	target_compile_definitions(rsinput_tests PRIVATE RSINPUT_PRESET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/presets")
	add_test(NAME rsinput_tests COMMAND rsinput_tests "${CMAKE_CURRENT_SOURCE_DIR}/presets")
endif()
