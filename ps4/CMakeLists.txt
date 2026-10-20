# PS4LM's build: llama.cpp as a subproject, plus PS4LM's compat code.
#
# scripts/build-llama.sh configures this with ps4/toolchain.cmake. llama.cpp's
# own targets (llama-server, llama-cli, llama-bench) are built from here, so
# there is one build tree.

cmake_minimum_required(VERSION 3.20)
project(ps4lm C CXX)

set(LLAMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/llama.cpp")
# All programs in build/.../bin, as when llama.cpp is built on its own.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# The SDK's libc has posix_spawn but not posix_spawn_file_actions_addchdir_np;
# nothing on the console spawns processes with a working directory, so
# subprocess.h goes without (docs/PORTING.md).
add_compile_definitions(SUBPROCESS_HAVE_CWD=0)
# There is no libm: the math functions are in Sony's libc (libSceLibcInternal).
set(MATH_LIBRARY SceLibcInternal)
add_subdirectory(${LLAMA_DIR} llama.cpp)

set(PS4LM_LOG_HOST "" CACHE STRING "Mac log listener address baked into the tools")
set(PS4LM_LOG_PORT "9999" CACHE STRING "Mac log listener port")

# Every tool gets the functions the console lacks (compat/compat.c), sends its
# output to the Mac and to /data/PS4LM/logs (compat/log.c), and is named
# <tool>.elf like the probes.
foreach(tool llama-cli llama-server llama-bench)
    if(NOT TARGET ${tool})
        message(FATAL_ERROR "llama.cpp has no target ${tool}")
    endif()
    target_sources(${tool} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/compat/compat.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/compat/log.c")
    # libkernel_sys, not the SDK's default libkernel_web: only it exports
    # openat, unlinkat, fchmodat and pathconf, which libc++'s std::filesystem
    # calls (docs/PORTING.md).
    target_link_libraries(${tool} PRIVATE kernel_sys)
    target_compile_definitions(${tool} PRIVATE
        PS4LM_LOG_HOST="${PS4LM_LOG_HOST}" PS4LM_LOG_PORT=${PS4LM_LOG_PORT})
    set_target_properties(${tool} PROPERTIES SUFFIX ".elf")
endforeach()
