"""Offline regression tests for the executed CTest inventory admission."""
import io
import unittest
import xml.etree.ElementTree as ET

from verify_ctest_results import BASELINE_TESTS, VerificationError, verify_report, verify_testsuite


class BookCTestAdmissionTests(unittest.TestCase):
    def setUp(self):
        self.names = sorted(BASELINE_TESTS)
        self.assertEqual(len(self.names), 36)

    def suite(self, names=None):
        names = self.names if names is None else names
        suite = ET.Element("testsuite", {
            "name": "Linux-cc",
            "tests": str(len(names)),
            "failures": "0",
            "disabled": "0",
            "skipped": "0",
        })
        for name in names:
            ET.SubElement(suite, "testcase", {
                "name": name, "classname": name, "time": "0.0", "status": "run"
            })
        return suite

    def test_all_36_executed_passed(self):
        self.assertEqual(verify_testsuite(self.suite()), 36)

    def test_more_executed_tests_allowed(self):
        self.assertEqual(verify_testsuite(self.suite(self.names + ["book_future_gate"])), 37)

    def test_one_removed_gate_rejected(self):
        with self.assertRaisesRegex(VerificationError, "minimum 36"):
            verify_testsuite(self.suite(self.names[:-1]))

    def test_missing_baseline_even_with_extra_test_rejected(self):
        with self.assertRaisesRegex(VerificationError, "missing baseline CTests"):
            verify_testsuite(self.suite(self.names[:-1] + ["book_new_gate"]))

    def test_duplicate_gate_rejected(self):
        with self.assertRaisesRegex(VerificationError, "duplicate CTest names"):
            verify_testsuite(self.suite(self.names + [self.names[0]]))

    def test_wrong_reported_count_rejected(self):
        suite = self.suite()
        suite.set("tests", "35")
        with self.assertRaisesRegex(VerificationError, "reports 35 tests"):
            verify_testsuite(suite)

    def test_skipped_summary_rejected(self):
        suite = self.suite()
        suite.set("skipped", "1")
        with self.assertRaisesRegex(VerificationError, "reports 1 skipped"):
            verify_testsuite(suite)

    def test_disabled_summary_rejected(self):
        suite = self.suite()
        suite.set("disabled", "1")
        with self.assertRaisesRegex(VerificationError, "reports 1 disabled"):
            verify_testsuite(suite)

    def test_failure_summary_rejected(self):
        suite = self.suite()
        suite.set("failures", "1")
        with self.assertRaisesRegex(VerificationError, "reports 1 failures"):
            verify_testsuite(suite)

    def test_skipped_case_rejected_even_when_summary_lies(self):
        suite = self.suite()
        ET.SubElement(suite.find("testcase"), "skipped")
        with self.assertRaisesRegex(VerificationError, "contains <skipped>"):
            verify_testsuite(suite)

    def test_failed_case_rejected_even_when_summary_lies(self):
        suite = self.suite()
        ET.SubElement(suite.find("testcase"), "failure").text = "failure details"
        with self.assertRaisesRegex(VerificationError, "contains <failure>"):
            verify_testsuite(suite)

    def test_unexecuted_case_rejected(self):
        suite = self.suite()
        suite.find("testcase").set("status", "notrun")
        with self.assertRaisesRegex(VerificationError, "did not run"):
            verify_testsuite(suite)

    def test_unnamed_case_rejected(self):
        suite = self.suite()
        del suite.find("testcase").attrib["name"]
        with self.assertRaisesRegex(VerificationError, "unnamed testcase"):
            verify_testsuite(suite)

    def test_missing_report_count_rejected(self):
        suite = self.suite()
        del suite.attrib["disabled"]
        with self.assertRaisesRegex(VerificationError, "missing 'disabled'"):
            verify_testsuite(suite)

    def test_wrong_root_rejected(self):
        suite = self.suite()
        suite.tag = "testsuites"
        with self.assertRaisesRegex(VerificationError, "expected CTest <testsuite>"):
            verify_testsuite(suite)

    def test_valid_xml_roundtrip(self):
        data = io.BytesIO(ET.tostring(self.suite(), encoding="utf-8"))
        self.assertEqual(verify_report(data), 36)

    def test_malformed_xml_rejected(self):
        with self.assertRaises(ET.ParseError):
            verify_report(io.BytesIO(b"<testsuite"))

    def test_errors_summary_rejected(self):
        suite = self.suite()
        suite.set("errors", "1")
        with self.assertRaisesRegex(VerificationError, "reports errors"):
            verify_testsuite(suite)


if __name__ == "__main__":
    unittest.main()
