cmake_minimum_required(VERSION 3.3)
project(cmpl)

set(ARCH "64")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m${ARCH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/out>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/out>)

set(CMPL_SRC_FILES
	src/cmpl.h
	src/cmplVm.h
	src/cmplCc.h
	src/parser.h
	src/printer.h
	src/internal.h
	src/cgen.c
	src/code.c
	src/core.c
	src/lexer.c
	src/lstd.c
	src/parser.c
	src/plugin.c
	src/printer.c
	src/tree.c
	src/type.c
	src/utils.c
)

add_executable(cmpl ${CMPL_SRC_FILES} src/main.c)
target_link_libraries(cmpl m dl)

add_library(File SHARED src/plugin/file.c)
target_include_directories(File PUBLIC ./src/)

add_library(OpenGL SHARED src/plugin/openGL.c)
target_include_directories(OpenGL PUBLIC ./src/)
target_link_libraries(OpenGL GL GLU glut)

# a simple "how to use" demo code.
add_executable(cmplVmEmit ${CMPL_SRC_FILES} src/test/vmMulf32.c)
target_include_directories(cmplVmEmit PUBLIC ./src/)
target_link_libraries(cmplVmEmit m dl)
