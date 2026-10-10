cmake_minimum_required(VERSION 3.25)

project(AdvancedC11BookQualification LANGUAGES C)

include(CTest)
include(CheckCSourceCompiles)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Salts CONFIG REQUIRED)
find_package(SaltsUtils CONFIG REQUIRED)

function(book_require_restored_package_dir package_name root_env config_subdir)
  if(NOT DEFINED ENV{${root_env}} OR NOT IS_DIRECTORY "$ENV{${root_env}}")
    message(FATAL_ERROR "${root_env} must name the restored SDK")
  endif()

  file(REAL_PATH "$ENV{${root_env}}/${config_subdir}" expected_dir)
  file(REAL_PATH "${${package_name}_DIR}" actual_dir)

  if(NOT actual_dir STREQUAL expected_dir)
    message(FATAL_ERROR
      "${package_name} resolved outside restored SDK: "
      "expected=${expected_dir} actual=${actual_dir}")
  endif()
endfunction()

book_require_restored_package_dir(
  Salts
  SALTS_ROOT
  "lib/cmake/Salts")
book_require_restored_package_dir(
  SaltsUtils
  SALTS_UTILS_ROOT
  "lib/cmake/SaltsUtils")

set(CMAKE_REQUIRED_LIBRARIES Salts::CMeta)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(CMAKE_REQUIRED_FLAGS "-Wall -Wextra -Werror -pedantic")
endif()

function(book_expect_c_compile variable expected source)
  unset(${variable} CACHE)
  check_c_source_compiles("${source}" ${variable})
  if(${expected})
    if(NOT ${variable})
      message(FATAL_ERROR "expected C source to compile: ${variable}")
    endif()
  else()
    if(${variable})
      message(FATAL_ERROR "expected C source to fail translation: ${variable}")
    endif()
  endif()
endfunction()

book_expect_c_compile(
  BOOK_CONST_REQUIRE_FALSE
  FALSE
  [=[
#include <cmeta/compiler.h>
int main(void) {
    return 1 + CMETA_CONST_REQUIRE(0);
}
]=])

book_expect_c_compile(
  BOOK_CONTAINER_OF_WRONG_MEMBER_TYPE
  FALSE
  [=[
#include <cmeta/container_of.h>
typedef struct book_owner { long member; } book_owner;
int main(void) {
    int value = 0;
    book_owner *owner = cmeta_container_of_as(&value, book_owner, long, member);
    return owner != 0;
}
]=])

book_expect_c_compile(
  BOOK_TYPEFUNCTION_DECLARED_KEY
  TRUE
  [=[
#include <cmeta/compute.h>
TypeFunction(BookDeclared,
    (int, double, double));
typedef TypeEval(BookDeclared, int, double) book_result;
_Static_assert(sizeof(book_result) == sizeof(double), "declared token key");
int main(void) { return 0; }
]=])

book_expect_c_compile(
  BOOK_TYPEFUNCTION_IDENTICAL_DUPLICATE
  TRUE
  [=[
#include <cmeta/compute.h>
TypeFunction(BookSame,
    (int, double, double),
    (int, double, double));
typedef TypeEval(BookSame, int, double) book_result;
int main(void) { return sizeof(book_result) == sizeof(double) ? 0 : 1; }
]=])

book_expect_c_compile(
  BOOK_TYPEFUNCTION_MISSING_KEY
  FALSE
  [=[
#include <cmeta/compute.h>
TypeFunction(BookMissing,
    (int, double, double));
typedef TypeEval(BookMissing, float, double) book_result;
int main(void) { return (int)sizeof(book_result); }
]=])

book_expect_c_compile(
  BOOK_TYPEFUNCTION_ALIAS_KEY
  FALSE
  [=[
#include <cmeta/compute.h>
typedef int book_int;
TypeFunction(BookAlias,
    (int, double, double));
typedef TypeEval(BookAlias, book_int, double) book_result;
int main(void) { return (int)sizeof(book_result); }
]=])

book_expect_c_compile(
  BOOK_TYPEFUNCTION_CONFLICTING_RESULT
  FALSE
  [=[
#include <cmeta/compute.h>
TypeFunction(BookConflict,
    (int, double, double),
    (int, double, long));
int main(void) { return 0; }
]=])

book_expect_c_compile(
  BOOK_REFLECT_WRONG_FIELD_TYPE
  FALSE
  [=[
#include <cmeta/data_reflect.h>
typedef struct book_wrong_reflection { int id; } book_wrong_reflection;
cmeta_reflect_data(book_wrong_reflection, "book.Wrong",
    cmeta_field(long, id)
);
int main(void) { return 0; }
]=])

book_expect_c_compile(
  BOOK_TRAITS_DUPLICATE_ROW
  FALSE
  [=[
#include <cmeta/type_traits.h>
static bool book_equal(const void *a, const void *b) {
    return a == b;
}
cmeta_traits(book_duplicate,
    (equal, book_equal),
    (equal, book_equal)
);
int main(void) { return 0; }
]=])

book_expect_c_compile(
  BOOK_TRAITS_WRONG_CALLBACK
  FALSE
  [=[
#include <cmeta/type_traits.h>
static int book_bad_equal(const void *a, const void *b) {
    return a == b;
}
cmeta_traits(book_bad,
    (equal, book_bad_equal)
);
int main(void) { return 0; }
]=])

book_expect_c_compile(
  BOOK_BIND_POINTER_AS_VALUE_CAPTURE
  FALSE
  [=[
#include <cmeta/bind.h>
int book_bad_source(void *pointer, int value);
FunctionDeclAsAbiResult(
    value,
    int,
    &cmeta_type_int,
    CMETA_ABI_SCALAR,
    CMETA_RESULT_VALUE,
    book_bad_source,
    (void *, pointer, CMETA_PARAM_IN | CMETA_PARAM_BORROWED,
     &cmeta_type_void_ptr, CMETA_ABI_OBJECT_POINTER),
    (int, value, CMETA_PARAM_IN,
     &cmeta_type_int, CMETA_ABI_SCALAR));
FunctionBindDeclAsAbiResult(
    value,
    int,
    &cmeta_type_int,
    CMETA_ABI_SCALAR,
    CMETA_RESULT_VALUE,
    book_bad_bound,
    book_bad_source,
    CMETA_SIG_U_I_I,
    (value,
        (void *, pointer, CMETA_PARAM_IN | CMETA_PARAM_BORROWED,
         &cmeta_type_void_ptr, CMETA_ABI_OBJECT_POINTER)),
    (arg,
        (int, value, CMETA_PARAM_IN,
         &cmeta_type_int, CMETA_ABI_SCALAR)));
int main(void) { return 0; }
]=])

function(book_qualification_gate target)
  add_executable(${target} ${ARGN})
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
  add_test(NAME ${target} COMMAND ${target})
endfunction()

book_qualification_gate(
  book_ch01_compiler_skills
  ch01_compiler_skills.c)
target_link_libraries(
  book_ch01_compiler_skills
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch01_cmeta_pp
  ch01_cmeta_pp.c)
target_link_libraries(
  book_ch01_cmeta_pp
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch02_declaration_metadata
  ch02_declaration_metadata.c)
target_link_libraries(
  book_ch02_declaration_metadata
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch02_type_identity
  ch02_type_identity_main.c
  ch02_type_identity_a.c
  ch02_type_identity_b.c)
target_link_libraries(
  book_ch02_type_identity
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_bind_capture
  ch03_bind_capture.c)
target_link_libraries(
  book_ch03_bind_capture
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_scope_cleanup
  ch03_scope_cleanup.c)
target_link_libraries(
  book_ch03_scope_cleanup
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_receiver_resolution
  ch03_receiver_resolution.c)
target_link_libraries(
  book_ch03_receiver_resolution
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_lifecycle_binding
  ch03_lifecycle_binding.c)
target_link_libraries(
  book_ch03_lifecycle_binding
  PRIVATE
    Salts::CMeta)

book_qualification_gate(
  book_ch03_exact_invoke
  ch03_exact_invoke.c)
target_link_libraries(
  book_ch03_exact_invoke
  PRIVATE
    Salts::CMeta)

# ACE design is an installed public CMeta contract: borrowed typed
# Strategy/Interceptor, Platform-owned synchronization and NativeIO token.
book_qualification_gate(
  book_ch03_cmeta_ace_patterns
  ch03_cmeta_ace_patterns.c)
target_link_libraries(
  book_ch03_cmeta_ace_patterns
  PRIVATE
    Salts::CMeta
    Salts::Platform
    Salts::NativeIO)
set_tests_properties(book_ch03_cmeta_ace_patterns PROPERTIES
  LABELS "cmeta;ace;strategy;interceptor;scoped-lock;installed"
  TIMEOUT 25)

book_qualification_gate(
  book_ch04_graph_admission
  ch04_graph_admission.c)
target_link_libraries(
  book_ch04_graph_admission
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch05_stream_graph
  ch05_stream_graph.c)
target_link_libraries(
  book_ch05_stream_graph
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch06_normalize_snapshot
  ch06_normalize_snapshot.c)
target_link_libraries(
  book_ch06_normalize_snapshot
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch06_optimizer_trace
  ch06_optimizer_trace.c)
target_link_libraries(
  book_ch06_optimizer_trace
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch07_plan_certificate
  ch07_plan_certificate.c)
target_link_libraries(
  book_ch07_plan_certificate
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch07_direct_no_fallback
  ch07_direct_no_fallback.c)
target_link_libraries(
  book_ch07_direct_no_fallback
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch06_normalize_idempotence
  ch06_normalize_idempotence.c)
target_link_libraries(
  book_ch06_normalize_idempotence
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch06_authorized_rewrite
  ch06_authorized_rewrite.c)
target_link_libraries(
  book_ch06_authorized_rewrite
  PRIVATE
    Salts::CFlow)

# SaltsUtils 4.3 Contract-only Native Service: plain CMeta VALUE records,
# exact FunctionDesc/NativeExecution and caller-owned binding metadata.
# No BINARY_CODEC, legacy *_native.c or implicit format admission.
salts_idl_target(
  TARGET book_native_value_service
  IDL "${CMAKE_CURRENT_SOURCE_DIR}/ch08_native_value_service.schema"
  ARTIFACT_NAME "book_value"
  ARTIFACTS NATIVE)

target_sources(
  ${book_native_value_service_NATIVE_TARGET}
  PRIVATE
    ch08_native_value_service_business.c)

book_qualification_gate(
  book_ch08_native_value_service
  ch08_native_value_service.c)
target_link_libraries(
  book_ch08_native_value_service
  PRIVATE
    Salts::DataBind
    ${book_native_value_service_NATIVE_TARGET})
target_include_directories(
  book_ch08_native_value_service
  PRIVATE
    "${book_native_value_service_GENERATED_DIR}")
add_dependencies(
  book_ch08_native_value_service
  book_native_value_service_idl_codegen)

# Optional/default overlay is not a CMeta VALUE execution record in
# SaltsUtils 4.3 Contract-only Native. This case deliberately tests the
# explicitly selected Binary schema path; it must never be an implicit fallback.
salts_idl_target(
  TARGET book_databind_service
  IDL "${CMAKE_CURRENT_SOURCE_DIR}/ch08_databind_service.schema"
  ARTIFACT_NAME "book_service"
  BINARY_CODEC
  ARTIFACTS NATIVE)

target_sources(
  ${book_databind_service_NATIVE_TARGET}
  PRIVATE
    ch08_databind_service_business.c)

book_qualification_gate(
  book_ch08_databind_binding_plan
  ch08_databind_binding_plan.c)
target_link_libraries(
  book_ch08_databind_binding_plan
  PRIVATE
    Salts::DataBind
    ${book_databind_service_NATIVE_TARGET})
target_include_directories(
  book_ch08_databind_binding_plan
  PRIVATE
    "${book_databind_service_GENERATED_DIR}")
add_dependencies(
  book_ch08_databind_binding_plan
  book_databind_service_idl_codegen)

book_qualification_gate(
  book_ch08_databind_public_sdk
  ch08_databind_public_sdk.c)
target_link_libraries(
  book_ch08_databind_public_sdk
  PRIVATE
    Salts::DataBind)

book_qualification_gate(
  book_ch09_reactive_demand
  ch09_reactive_demand.c)
target_link_libraries(
  book_ch09_reactive_demand
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch09_wait_wake
  ch09_wait_wake.c)
target_link_libraries(
  book_ch09_wait_wake
  PRIVATE
    Salts::CFlow)

# ACE Monitor Object: real Platform mutex/condition and Thread-Specific
# Storage, exposed through a fully reflected borrowed CMeta Interface.
# No Actor mailbox, second scheduler or undisclosed worker runtime.
# Optional CPU-event ACE Leader/Followers. One application owns bounded
# event admission and baton election; CMeta reflects the exact callback;
# Platform owns real mutex/condition and worker thread resources.
book_qualification_gate(
  book_ch10_cmeta_ace_leader_followers
  ch10_cmeta_ace_leader_followers.c)
target_link_libraries(
  book_ch10_cmeta_ace_leader_followers
  PRIVATE
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch10_cmeta_ace_leader_followers PROPERTIES
  TIMEOUT 25
  LABELS "cmeta;ace;leader-followers;cpu-event;installed")

book_qualification_gate(
  book_ch10_cmeta_ace_monitor
  ch10_cmeta_ace_monitor.c)
target_link_libraries(
  book_ch10_cmeta_ace_monitor
  PRIVATE
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch10_cmeta_ace_monitor PROPERTIES
  TIMEOUT 25
  LABELS "cmeta;ace;monitor;thread-local;installed")

book_qualification_gate(
  book_ch10_executor_settlement
  ch10_executor_settlement.c)
target_link_libraries(
  book_ch10_executor_settlement
  PRIVATE
    Salts::CFlow)

book_qualification_gate(
  book_ch11_machine_staged_commit
  ch11_machine_staged_commit.c)
target_link_libraries(
  book_ch11_machine_staged_commit
  PRIVATE
    Salts::CFlow)

# Reactor-style caller-driven Acceptor-Connector: real listener readiness
# and CNet poll callbacks adapted to an exact reflected CMeta Interface.
# NativeIO/CNet remain the only I/O authority; no standalone backend is added.
book_qualification_gate(
  book_ch12_ace_reactor_dispatch
  ch12_ace_reactor_dispatch.c)
target_link_libraries(
  book_ch12_ace_reactor_dispatch
  PRIVATE
    Salts::CNet
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch12_ace_reactor_dispatch PROPERTIES
  TIMEOUT 25
  LABELS "ace;reactor;acceptor;connector;installed;network")

# CNet 2.3 Strategy decisions are pure admission policy; CMeta supplies an
# exact typed ACE-style handler, never a second Reactor or Actor runtime.
book_qualification_gate(
  book_ch12_cnet_strategy
  ch12_cnet_strategy.c)
target_link_libraries(
  book_ch12_cnet_strategy
  PRIVATE
    Salts::CNet
    Salts::CMeta)

# Real installed-SDK dual-SG-Owner TCP transport progression.
# One NativeIO host observe per shard; final Owner performs Manager adoption.
book_qualification_gate(
  book_ch12_cnet_sg_handoff
  ch12_cnet_sg_handoff.c)
target_link_libraries(
  book_ch12_cnet_sg_handoff
  PRIVATE
    Salts::NativeIO
    Salts::CNet
    Salts::CMeta)
set_tests_properties(
  book_ch12_cnet_sg_handoff PROPERTIES
    TIMEOUT 25
    LABELS "cnet;sg;handoff;installed;network")

# Optional application Active Object: CMeta ports remain borrowed,
# CFlow alone owns Machine/Actor mailbox, task scheduling and stale producer refs.
book_qualification_gate(
  book_ch12_cmeta_ace_active_object
  ch12_cmeta_ace_active_object.c)
target_link_libraries(
  book_ch12_cmeta_ace_active_object
  PRIVATE
    Salts::CFlow
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch12_cmeta_ace_active_object PROPERTIES
  LABELS "cmeta;ace;active-object;cflow;installed"
  TIMEOUT 15)

# An application Service Configurator chooses a CMeta typed Strategy from
# a real strictly validated file, and publishes only with no live call leases.
# No invented general hot-reload framework or hidden runtime replacement.
foreach(fixture IN ITEMS
    ace_configurator_add.cfg
    ace_configurator_scale.cfg
    ace_configurator_invalid.cfg
    ace_configurator_missing.cfg
    ace_configurator_unknown.cfg
    ace_configurator_range.cfg)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/${fixture}"
    "${CMAKE_CURRENT_BINARY_DIR}/${fixture}"
    COPYONLY)
endforeach()
book_qualification_gate(
  book_ch15_cmeta_ace_configurator
  ch15_cmeta_ace_configurator.c)
target_link_libraries(
  book_ch15_cmeta_ace_configurator
  PRIVATE
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch15_cmeta_ace_configurator PROPERTIES
  LABELS "cmeta;ace;configurator;control-plane;installed"
  TIMEOUT 15)

# Application-level ACE Configurator selects two *independent* installed
# CNet policy APIs from strict config: local Owner versus remote endpoint.
# Only advisory decision is tested; no implicit handoff, pool, dial or retry.
foreach(fixture IN ITEMS
    ace_cnet_policy_strict.cfg
    ace_cnet_policy_mixed.cfg
    ace_cnet_policy_rr.cfg
    ace_cnet_policy_invalid.cfg
    ace_cnet_policy_missing.cfg
    ace_cnet_policy_unknown.cfg)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/${fixture}"
    "${CMAKE_CURRENT_BINARY_DIR}/${fixture}"
    COPYONLY)
endforeach()
book_qualification_gate(
  book_ch15_cmeta_ace_cnet_policy_config
  ch15_cmeta_ace_cnet_policy_config.c)
target_link_libraries(
  book_ch15_cmeta_ace_cnet_policy_config
  PRIVATE
    Salts::CNet
    Salts::CMeta
    Salts::Platform)
set_tests_properties(book_ch15_cmeta_ace_cnet_policy_config PROPERTIES
  LABELS "ace;configurator;cnet;server;client;installed"
  TIMEOUT 15)

book_qualification_gate(
  book_ch13_parse_u64
  ch13_parse_u64.c)

book_qualification_gate(
  book_ch13_plugin_exact_abi
  ch13_plugin_exact_abi.c)
target_link_libraries(
  book_ch13_plugin_exact_abi
  PRIVATE
    Salts::Plugin)
