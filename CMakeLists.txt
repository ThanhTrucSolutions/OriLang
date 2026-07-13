cmake_minimum_required(VERSION 3.16)
project(orilang C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(orivm core/orivm.c)
# Optional thin CLI bootstrap when present
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/ori.c")
  add_executable(ori_bootstrap tools/ori.c)
endif()

enable_testing()
add_test(NAME orivm_help COMMAND orivm --help)
