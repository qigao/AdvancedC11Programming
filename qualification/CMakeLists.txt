cmake_minimum_required(VERSION 3.25)

project(AdvancedC11BookQualification LANGUAGES C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Salts CONFIG REQUIRED)

function(book_qualification_gate target)
  add_executable(${target} ${ARGN})
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
  add_test(NAME ${target} COMMAND ${target})
endfunction()

book_qualification_gate(
  book_ch01_cmeta_pp
  ch01_cmeta_pp.c)
target_link_libraries(
  book_ch01_cmeta_pp
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_exact_invoke
  ch03_exact_invoke.c)
target_link_libraries(
  book_ch03_exact_invoke
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch05_stream_graph
  ch05_stream_graph.c)
target_link_libraries(
  book_ch05_stream_graph
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch13_parse_u64
  ch13_parse_u64.c)
