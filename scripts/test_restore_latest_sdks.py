"""Offline selection tests: no network, package pins, or fallback behavior."""
import unittest

from restore_latest_sdks import (
    required_checksum, select_release, validate_sdk_rid, version_key
)


def release(tag, package="Salts.Native", *, published="2026-01-01T00:00:00Z",
            draft=False, asset=True):
    name = f"{package}.{tag.removeprefix('v')}.nupkg"
    return {
        "tag_name": tag,
        "published_at": published,
        "draft": draft,
        "assets": [{"name": name, "browser_download_url": "https://example.invalid/sdk"}]
        if asset else [],
    }


class VersionTests(unittest.TestCase):
    def test_rc_is_newer_than_previous_stable(self):
        older = release("v7.4.0")
        newer = release("v7.5.0-rc.8")
        self.assertEqual(select_release([older, newer], "Salts.Native")[0], newer)

    def test_formal_release_supersedes_same_version_rc(self):
        stable = release("v7.5.0")
        rc = release("v7.5.0-rc.20")
        self.assertEqual(select_release([rc, stable], "Salts.Native")[0], stable)

    def test_numeric_rc_order_not_string_order(self):
        rc9 = release("v7.5.0-rc.9")
        rc10 = release("v7.5.0-rc.10")
        self.assertEqual(select_release([rc9, rc10], "Salts.Native")[0], rc10)

    def test_draft_is_not_published(self):
        stable = release("v7.4.0")
        draft = release("v8.0.0", draft=True)
        self.assertEqual(select_release([draft, stable], "Salts.Native")[0], stable)

    def test_missing_latest_asset_is_failure_not_downgrade(self):
        stable = release("v7.4.0")
        newest = release("v7.5.0-rc.1", asset=False)
        with self.assertRaisesRegex(RuntimeError, "expected one exact"):
            select_release([stable, newest], "Salts.Native")

    def test_wrong_or_duplicate_asset_is_failure(self):
        newest = release("v7.5.0-rc.1")
        newest["assets"] *= 2
        with self.assertRaisesRegex(RuntimeError, "expected one exact"):
            select_release([newest], "Salts.Native")

    def test_newer_unknown_tag_is_failure_not_downgrade(self):
        known = release("v7.5.0", published="2026-01-01T00:00:00Z")
        unknown = release("future-main", published="2026-01-02T00:00:00Z")
        with self.assertRaisesRegex(RuntimeError, "unrecognized release tag"):
            select_release([known, unknown], "Salts.Native")

    def test_unsupported_semver_is_rejected(self):
        self.assertIsNone(version_key("v7.5.0-rc.01"))

    def test_exact_checksum(self):
        digest = "a" * 64
        self.assertEqual(
            required_checksum(f"{digest}  Salts.Native.7.5.0.nupkg\n",
                              "Salts.Native.7.5.0.nupkg"),
            digest,
        )

    def test_missing_checksum_is_failure(self):
        with self.assertRaisesRegex(RuntimeError, "SHA256 entry"):
            required_checksum("a" * 64 + "  some-other-file\n",
                              "Salts.Native.7.5.0.nupkg")



class RidTests(unittest.TestCase):
    def test_only_native_host_sdk_matches(self):
        for rid, host, arch in (
            ("linux-x64", "linux", "x86_64"),
            ("windows-x64", "win32", "AMD64"),
            ("macos-arm64", "darwin", "arm64"),
        ):
            with self.subTest(rid=rid):
                self.assertEqual(validate_sdk_rid(rid, host, arch), rid)

    def test_runner_and_sdk_cannot_cross_architectures(self):
        for rid, host, arch in (
            ("macos-arm64", "darwin", "x86_64"),
            ("windows-x64", "linux", "x86_64"),
            ("linux-x64", "win32", "AMD64"),
        ):
            with self.subTest(rid=rid):
                with self.assertRaisesRegex(RuntimeError, "SDK|unsupported"):
                    validate_sdk_rid(rid, host, arch)

    def test_unknown_or_unselected_rid_fails_closed(self):
        with self.assertRaisesRegex(RuntimeError, "does not match runner"):
            validate_sdk_rid(None, "linux", "x86_64")
        with self.assertRaisesRegex(RuntimeError, "unsupported SDK host"):
            validate_sdk_rid("linux-arm64", "linux", "aarch64")


if __name__ == "__main__":
    unittest.main()
