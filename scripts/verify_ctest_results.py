#!/usr/bin/env python3
"""Require every established executable book CTest to *run and pass*.

CTest success alone does not ensure that a test was not deleted, disabled, or
skipped.  This checks the actual CTest JUnit results on each native SDK host.
Additional future book gates are allowed; the established 35 are mandatory.
"""
from collections import Counter
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


MIN_EXECUTED_TESTS = 35
BASELINE_TESTS = frozenset({
    "book_ch01_compiler_skills",
    "book_ch01_cmeta_pp",
    "book_ch02_declaration_metadata",
    "book_ch02_type_identity",
    "book_ch03_bind_capture",
    "book_ch03_scope_cleanup",
    "book_ch03_receiver_resolution",
    "book_ch03_lifecycle_binding",
    "book_ch03_exact_invoke",
    "book_ch03_cmeta_ace_patterns",
    "book_ch04_graph_admission",
    "book_ch05_stream_graph",
    "book_ch06_normalize_snapshot",
    "book_ch06_optimizer_trace",
    "book_ch07_plan_certificate",
    "book_ch07_direct_no_fallback",
    "book_ch06_normalize_idempotence",
    "book_ch06_authorized_rewrite",
    "book_ch08_native_value_service",
    "book_ch08_databind_binding_plan",
    "book_ch08_databind_public_sdk",
    "book_ch09_reactive_demand",
    "book_ch09_wait_wake",
    "book_ch10_cmeta_ace_leader_followers",
    "book_ch10_cmeta_ace_monitor",
    "book_ch10_executor_settlement",
    "book_ch11_machine_staged_commit",
    "book_ch12_ace_reactor_dispatch",
    "book_ch12_cnet_strategy",
    "book_ch12_cnet_sg_handoff",
    "book_ch12_cmeta_ace_active_object",
    "book_ch15_cmeta_ace_configurator",
    "book_ch15_cmeta_ace_cnet_policy_config",
    "book_ch13_parse_u64",
    "book_ch13_plugin_exact_abi",
})


class VerificationError(ValueError):
    """The native book test execution did not meet its published baseline."""


def _integer_attribute(element, name):
    raw = element.get(name)
    if raw is None:
        raise VerificationError(f"CTest JUnit missing {name!r} count")
    try:
        value = int(raw)
    except ValueError as exc:
        raise VerificationError(f"CTest JUnit invalid {name!r} count: {raw!r}") from exc
    if value < 0:
        raise VerificationError(f"CTest JUnit negative {name!r} count")
    return value


def verify_testsuite(root):
    """Return the executed count, or raise on any missing or nonpassing gate."""
    if len(BASELINE_TESTS) != MIN_EXECUTED_TESTS:
        raise VerificationError("book's 35-test baseline roster was modified")

    if root.tag != "testsuite":
        raise VerificationError(f"expected CTest <testsuite>, got <{root.tag}>")

    counts = {
        key: _integer_attribute(root, key)
        for key in ("tests", "failures", "skipped", "disabled")
    }
    if root.get("errors") is not None and _integer_attribute(root, "errors"):
        raise VerificationError("CTest JUnit reports errors")

    cases = root.findall("testcase")
    if counts["tests"] != len(cases):
        raise VerificationError(
            f"CTest JUnit reports {counts['tests']} tests but contains {len(cases)} cases"
        )
    if len(cases) < MIN_EXECUTED_TESTS:
        raise VerificationError(
            f"only {len(cases)} book tests; minimum {MIN_EXECUTED_TESTS} required"
        )
    for key in ("failures", "skipped", "disabled"):
        if counts[key]:
            raise VerificationError(f"CTest JUnit reports {counts[key]} {key}")

    names = [case.get("name") for case in cases]
    if any(not name for name in names):
        raise VerificationError("CTest JUnit contains an unnamed testcase")
    duplicates = sorted(name for name, count in Counter(names).items() if count != 1)
    if duplicates:
        raise VerificationError(f"duplicate CTest names: {', '.join(duplicates)}")
    missing = sorted(BASELINE_TESTS.difference(names))
    if missing:
        raise VerificationError(f"missing baseline CTests: {', '.join(missing)}")

    for case in cases:
        name = case.get("name")
        if case.get("status") != "run":
            raise VerificationError(f"CTest {name} did not run (status={case.get('status')!r})")
        for kind in ("failure", "skipped", "error"):
            if case.find(kind) is not None:
                raise VerificationError(f"CTest {name} contains <{kind}>")
    return len(cases)


def verify_report(report):
    return verify_testsuite(ET.parse(report).getroot())


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print("usage: verify_ctest_results.py <CTest JUnit XML>", file=sys.stderr)
        return 2
    try:
        count = verify_report(Path(argv[0]))
    except (OSError, ET.ParseError, VerificationError) as exc:
        print(f"book installed-SDK CTest admission FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"book installed-SDK CTest admission PASS: {count} executed/passed; all 35 baseline tests present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
