#!/usr/bin/env python3
"""Restore the highest-version published Salts SDKs, including release candidates.

This is an installed-consumer qualification gate: never pin, downgrade to a
previous release, repair a broken package, or substitute a source-tree SDK.
"""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile


SEMVER = re.compile(
    r"^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$"
)
SUM_LINE = re.compile(r"^([0-9a-fA-F]{64})\s+\*?(.+)$")
HEADERS = {
    "Accept": "application/vnd.github+json",
    "User-Agent": "advanced-c11-book-qualification",
}


def version_key(tag):
    """Return a comparable SemVer precedence key, or None for unknown tags."""
    match = SEMVER.fullmatch(tag or "")
    if match is None:
        return None
    core = tuple(int(part) for part in match.group(1, 2, 3))
    prerelease = match.group(4)
    if prerelease is None:
        return (*core, 1, ())
    parts = []
    for part in prerelease.split("."):
        if not part or (part.isdigit() and len(part) > 1 and part[0] == "0"):
            return None
        parts.append((0, int(part)) if part.isdigit() else (1, part))
    return (*core, 0, tuple(parts))


def select_release(releases, package_name):
    """Choose highest SemVer (stable or RC), then require its exact NuGet asset."""
    published = [r for r in releases if not r.get("draft") and r.get("published_at")]
    candidates = [(version_key(r.get("tag_name")), r) for r in published]
    valid = [(key, release) for key, release in candidates if key is not None]
    if not valid:
        raise RuntimeError(f"no published semantic-version release for {package_name}")
    _, release = max(valid, key=lambda pair: pair[0])

    # Never silently downgrade when a newly published, non-SemVer tag appears.
    if any(key is None and r["published_at"] > release["published_at"]
           for key, r in candidates):
        raise RuntimeError(f"newer unrecognized release tag for {package_name}")

    tag = release["tag_name"]
    exact_asset_name = f"{package_name}.{tag.removeprefix('v')}.nupkg"
    assets = [a for a in release.get("assets", [])
              if a.get("name") == exact_asset_name and
              not a.get("state") == "new"]
    if len(assets) != 1:
        raise RuntimeError(
            f"{tag}: expected one exact {exact_asset_name} asset, found {len(assets)}"
        )
    return release, assets[0]


def load_releases(repo):
    url = f"https://api.github.com/repos/{repo}/releases?per_page=100"
    headers = dict(HEADERS)
    if os.environ.get("GH_TOKEN"):
        headers["Authorization"] = f"Bearer {os.environ['GH_TOKEN']}"
    releases = []
    while url:
        request = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(request, timeout=90) as response:
            batch = json.load(response)
            if not isinstance(batch, list):
                raise RuntimeError(f"unexpected release listing for {repo}")
            releases.extend(batch)
            link = response.headers.get("Link", "")
        url = None
        for part in link.split(","):
            if 'rel="next"' in part:
                match = re.search(r"<([^>]+)>", part)
                if match:
                    url = match.group(1)
                else:
                    raise RuntimeError(f"invalid pagination for {repo}")
    return releases


def download(url, path):
    request = urllib.request.Request(
        url, headers={"User-Agent": HEADERS["User-Agent"]}
    )
    with urllib.request.urlopen(request, timeout=180) as response:
        with path.open("wb") as output:
            shutil.copyfileobj(response, output)


def required_checksum(contents, asset_name):
    matches = []
    for line in contents.splitlines():
        match = SUM_LINE.fullmatch(line.strip())
        if match and Path(match.group(2)).name == asset_name:
            matches.append(match.group(1).lower())
    if len(matches) != 1:
        raise RuntimeError(f"missing/ambiguous SHA256 entry for {asset_name}")
    return matches[0]


def validate_sdk_rid(rid, system, architecture):
    """Require an exact runner/SDK ABI match, never a cross-RID fallback."""
    supported = {
        ("linux", "x86_64"): "linux-x64",
        ("linux", "amd64"): "linux-x64",
        ("win32", "amd64"): "windows-x64",
        ("darwin", "arm64"): "macos-arm64",
        ("darwin", "aarch64"): "macos-arm64",
    }
    expected = supported.get((system.lower(), architecture.lower()))
    if expected is None:
        raise RuntimeError(f"unsupported SDK host: {system}/{architecture}")
    if rid != expected:
        raise RuntimeError(
            f"SDK RID {rid!r} does not match runner {system}/{architecture}; "
            f"expected {expected!r}"
        )
    return rid


def restore(repo, package_name, root, cmake_package, rid, require_idlc=False):
    release, asset = select_release(load_releases(repo), package_name)
    root = Path(root)
    archive = Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir())) / f"{package_name}.nupkg"
    download(asset["browser_download_url"], archive)

    sums = [a for a in release.get("assets", []) if a.get("name") == "SHA256SUMS"]
    if len(sums) > 1:
        raise RuntimeError(f"duplicate SHA256SUMS for {repo}")
    if sums:
        sums_file = archive.with_suffix(".SHA256SUMS")
        download(sums[0]["browser_download_url"], sums_file)
        expected = required_checksum(sums_file.read_text(), asset["name"])
        actual = hashlib.sha256(archive.read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(f"SHA256 mismatch for {asset['name']}")

    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    # Unix requires unzip to preserve the host compiler's packaged 0755 mode.
    # Windows does not carry Unix executable bits and ships salts-idlc.exe.
    if os.name == "nt":
        with zipfile.ZipFile(archive) as content:
            content.extractall(root)
    else:
        subprocess.run(["unzip", "-q", "-o", str(archive), "-d", str(root)], check=True)

    prefix = root / "sdk" / rid
    config = prefix / "lib" / "cmake" / cmake_package / f"{cmake_package}Config.cmake"
    if not config.is_file():
        raise RuntimeError(f"{asset['name']}: missing installed {config}")
    if require_idlc:
        compiler = prefix / "bin" / ("salts-idlc.exe" if os.name == "nt" else "salts-idlc")
        if not compiler.is_file() or not os.access(compiler, os.X_OK):
            raise RuntimeError(f"{asset['name']}: host compiler not executable: {compiler}")
    print(f"Selected {repo}@{release['tag_name']}: {asset['name']}", flush=True)
    return prefix


def main():
    environment = os.environ.get("GITHUB_ENV")
    if not environment:
        raise RuntimeError("GITHUB_ENV is required to publish restored SDK roots")
    rid = validate_sdk_rid(os.environ.get("BOOK_SDK_RID"), sys.platform, platform.machine())
    temp = Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir()))
    salts = restore("qigao/salts", "Salts.Native", temp / "salts-native", "Salts", rid)
    utils = restore(
        "qigao/salts-utils", "SaltsUtils.Native", temp / "saltsutils-native",
        "SaltsUtils", rid, require_idlc=True
    )
    with open(environment, "a", encoding="utf-8") as env:
        env.write(f"SALTS_SDK_PREFIX={salts.as_posix()}\n")
        env.write(f"SALTS_UTILS_SDK_PREFIX={utils.as_posix()}\n")
        if sys.platform == "darwin":
            env.write(f"DYLD_LIBRARY_PATH={(utils / 'lib').as_posix()}:{(salts / 'lib').as_posix()}\n")
        elif sys.platform == "linux":
            env.write(f"LD_LIBRARY_PATH={(utils / 'lib').as_posix()}:{(salts / 'lib').as_posix()}\n")
    if os.name == "nt":
        github_path = os.environ.get("GITHUB_PATH")
        if not github_path:
            raise RuntimeError("GITHUB_PATH required for Windows installed DLL closure")
        with open(github_path, "a", encoding="utf-8") as path:
            for root in (utils, salts):
                path.write(f"{(root / 'bin').as_posix()}\n")
                path.write(f"{(root / 'lib').as_posix()}\n")


if __name__ == "__main__":
    main()
