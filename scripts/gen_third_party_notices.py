#!/usr/bin/env python3
"""Regenerate THIRD-PARTY-NOTICES.txt.

Usage:
    python scripts/gen_third_party_notices.py
    python scripts/gen_third_party_notices.py --check    # non-zero if stale

Run it after bumping Qt, the MEGA SDK submodule, the vcpkg submodule, or the
vcpkg manifest features in CMakePresets.json. The output is committed on
purpose: the vcpkg input lives under build/ which is gitignored, so a
build-time generator would silently produce a distribution with no notices in
it on a clean clone.

Ported from MegaExplorer's script of the same name. What it lists is the
Windows build (the x64-windows-mega triplet); the other platforms link the
system's copies of some of these libraries instead.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_VCPKG_INSTALLED = REPO_ROOT / "build/msvc-debug/vcpkg_installed/x64-windows-mega"
OUTPUT = "THIRD-PARTY-NOTICES.txt"

# Both are pinned public submodules, which is what lets the notices point at a
# patch set instead of reproducing it.
VCPKG_REPO = "https://github.com/microsoft/vcpkg"
SDK_REPO = "https://github.com/meganz/sdk"

# Pinned in CLAUDE.md. Kept as constants rather than probed from the build tree
# so that a version bump shows up as a reviewable line in `git diff`.
QT_VERSION = "6.11.1"
SDK_VERSION = "v10.17.0"

# Every license text in here belongs to someone else, so without these the
# distribution never names this program's own author, nor says where its source
# is.
COPYRIGHT_LINE = "MegaDirStat  Copyright (c) 2026  Takumi Yamada (tackme31)"
SOURCE_URL = "https://github.com/tackme31/MegaDirStat"

# vcpkg's own metadata is unusable for these because the port is dual-licensed
# and *we* have to declare which side we took.
LICENSE_OVERRIDES = {
    # (BSD-3-Clause OR GPL-2.0-only) -- the permissive side.
    "zstd": "BSD-3-Clause",
}

# Qt ships no license file inside the repo (it is an external install), and its
# own bundled third-party set is far too large to reproduce. The LGPLv3 text
# (appended below by qt_text) plus the pointer to Qt's published list cover it.
QT_SOURCE_URL = f"https://download.qt.io/archive/qt/{'.'.join(QT_VERSION.split('.')[:2])}/{QT_VERSION}/single/"

QT_NOTICE = f"""\
The Qt Toolkit is Copyright (C) 2026 The Qt Company Ltd. and other
contributors.
Contact: https://www.qt.io/licensing/

MegaDirStat uses the Qt Community Edition under the terms of the GNU Lesser
General Public License, version 3 (LGPLv3). Only Qt modules available under
that license are used; no GPL-only Qt module is linked.

You may use, distribute and copy the Qt libraries used by this application
under the terms of the LGPLv3, whose full text is reproduced immediately
below, followed by the GPLv3 text it builds upon. The Qt libraries are
shipped as separate dynamic libraries and are not modified, so a recipient
may replace them with a modified, interface-compatible build. The
corresponding Qt {QT_VERSION} sources are published by The Qt Company at:
  {QT_SOURCE_URL}

Qt itself bundles certain third-party code that is licensed under separate
terms from their original authors, independent of the LGPL/GPL terms above.
The full list and texts of these components are published by The Qt Company
at:
  https://doc.qt.io/qt-6/licenses-used-in-qt.html

Qt is a trademark of The Qt Company Ltd. in Finland and/or other countries
worldwide.
"""

PREAMBLE = f"""\
MegaDirStat
Third-Party Software Notices and Information

{COPYRIGHT_LINE}

This application, MegaDirStat, is licensed under the MIT License. A copy of
that license is included in the file named "LICENSE" in the root of this
distribution, and the software is provided without warranty of any kind.

The complete source code of MegaDirStat, including the build instructions
needed to rebuild it, is available at
{SOURCE_URL}

MegaDirStat incorporates or links against third-party software components
that are subject to separate copyright and license terms, as detailed below.
Reproducing this file, unmodified, alongside the LICENSE file satisfies the
attribution requirements of those components.

How the third-party components were built
------------------------------------------
Qt is used exactly as published by The Qt Company: the official Qt {QT_VERSION}
binary release for MSVC, unmodified, shipped as separate dynamic libraries.
It is the only component used under the GNU Lesser General Public License;
see its entry below. Its corresponding sources are at
  {QT_SOURCE_URL}

Everything else is built from source and linked statically into the
executable. The MEGA C++ SDK and the components it vendors are built from the
SDK's source tree as tagged. The remaining components are built through
vcpkg, and each of those ports applies patches -- build fixes, MSVC
portability, and redirecting a component's bundled copies of other libraries
to external builds of them. Those patches are therefore part of what this
distribution contains, and each one is a file in the port directory of a
submodule of the source tree above:

  ports/<component>/                     {VCPKG_REPO}
  cmake/vcpkg_overlay_ports/<component>/ {SDK_REPO}

Both submodules are pinned to a specific commit of a public repository, so the
exact patched sources of any component here can be reconstructed from the
source tree of MegaDirStat.

This file is generated by scripts/gen_third_party_notices.py -- do not edit it
by hand.
"""

BANNER = "=" * 79
RULE = "-" * 79


def read_text(path: Path) -> str:
    """Normalize to LF and a single trailing newline so output is byte-stable
    regardless of how git checked the input out."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    return raw.replace("\r\n", "\n").replace("\r", "\n").rstrip("\n") + "\n"


def app_version() -> str:
    """Single-source the app version from project() rather than repeating it."""
    text = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(MegaDirStat\s+VERSION\s+([0-9.]+)", text)
    if not match:
        sys.exit("error: could not read VERSION from CMakeLists.txt's project()")
    return match.group(1)


def qt_text() -> str:
    """LGPLv3 is written as a set of additional permissions on top of GPLv3, so
    both texts have to travel with the distribution; this app's own LICENSE is
    MIT and covers neither."""
    upstream = REPO_ROOT / "licenses/upstream"
    return "\n".join([
        QT_NOTICE,
        RULE,
        read_text(upstream / "LGPL-3.0.txt"),
        RULE,
        read_text(upstream / "GPL-3.0.txt"),
    ])


def fixed_components() -> list[dict]:
    """Everything that does not come from vcpkg: this app first, then the
    frameworks, then what the SDK vendors."""
    sdk_root = REPO_ROOT / "third_party/sdk"
    vendored_root = sdk_root / "third_party"

    components = [
        {
            "name": "MegaDirStat",
            "version": app_version(),
            "license": "MIT",
            "homepage": SOURCE_URL,
            "text": read_text(REPO_ROOT / "LICENSE"),
        },
        {
            "name": "Qt",
            "version": QT_VERSION,
            "license": "LGPL-3.0-only",
            "homepage": "https://www.qt.io/",
            "text": qt_text(),
        },
        {
            "name": "MEGA C++ SDK",
            "version": SDK_VERSION,
            "license": "BSD-2-Clause",
            "homepage": SDK_REPO,
            "text": read_text(sdk_root / "LICENSE"),
        },
    ]

    # Vendored inside the SDK. third_party/sdk/third_party/CMakeLists.txt gates
    # two of the seven out of this build and they are deliberately absent here:
    # `evt-tls` needs USE_LIBUV (OFF in CMakePresets.json), and `glob` is
    # `if(NOT WIN32 ...)`. Neither is linked, so neither is distributed. `glob`
    # also ships no license file at all, which would need chasing upstream if it
    # ever became reachable.
    vendored = [
        ("ccronexpr", "LICENSE", "Apache-2.0", "https://github.com/staticlibs/ccronexpr"),
        ("csv", "LICENSE", "MIT", "https://github.com/ben-strasser/fast-cpp-csv-parser"),
        ("http_parser", "LICENSE-MIT", "MIT", "https://github.com/nodejs/http-parser"),
        ("utf8proc", "LICENSE", "MIT", "https://github.com/JuliaStrings/utf8proc"),
        ("zxcvbn-c", "LICENSE.txt", "MIT", "https://github.com/tsyrogit/zxcvbn-c"),
    ]
    for name, license_file, license_id, homepage in vendored:
        components.append({
            # Not direct dependencies of this app; they arrive through the SDK.
            "name": f"{name} (via MEGA SDK)",
            "version": SDK_VERSION,
            "license": license_id,
            "homepage": homepage,
            "text": read_text(vendored_root / name / license_file),
        })

    return components


def vcpkg_components(installed: Path) -> list[dict]:
    """One entry per real vcpkg port.

    Only directories carrying a vcpkg.spdx.json are ports; the rest
    (unofficial-*) are aliases that would otherwise be counted twice."""
    share = installed / "share"
    if not share.is_dir():
        sys.exit(
            f"error: {share} not found.\n"
            "Configure and build once so vcpkg materializes the license texts, "
            "or pass --vcpkg-installed."
        )

    components = []
    for spdx_path in sorted(share.glob("*/vcpkg.spdx.json")):
        package = json.loads(spdx_path.read_text(encoding="utf-8"))["packages"][0]
        name = package["name"]

        copyright_path = spdx_path.parent / "copyright"
        if not copyright_path.is_file():
            sys.exit(f"error: {name} has no copyright file next to {spdx_path}")

        license_id = LICENSE_OVERRIDES.get(name, package.get("licenseConcluded", ""))
        if "LicenseRef-" in license_id or not license_id:
            sys.exit(
                f"error: {name} has no usable license id ({license_id!r}); "
                "add it to LICENSE_OVERRIDES with a rationale."
            )
        # PREAMBLE says Qt is the only LGPL component and everything else is
        # linked statically; a (L)GPL port would make that false and bring
        # relinking or source obligations the preamble does not meet.
        if "GPL" in license_id:
            sys.exit(
                f"error: {name} is under {license_id!r}. If it is dual-licensed, "
                "declare the permissive side in LICENSE_OVERRIDES; otherwise the "
                "preamble has to be rewritten for it first."
            )

        components.append({
            "name": name,
            # versionInfo carries vcpkg's port revision ("1.1.0#1"); the upstream
            # version is what the notice is about, so drop the suffix.
            "version": package.get("versionInfo", "").split("#")[0],
            "license": license_id,
            "homepage": package.get("homepage", ""),
            "text": read_text(copyright_path),
        })

    return components


def render_notices(components: list[dict]) -> str:
    parts = [PREAMBLE]
    for index, component in enumerate(components, start=1):
        parts.append(
            f"\n{BANNER}\n{index}. {component['name']}\n{BANNER}\n\n"
            f"Project:    {component['name']}\n"
            f"Version:    {component['version']}\n"
            f"Homepage:   {component['homepage']}\n"
            f"License:    {component['license']}\n\n"
            f"{RULE}\n{component['text']}{RULE}\n"
        )
    parts.append(f"\n{BANNER}\nEnd of Third-Party Notices\n{BANNER}\n")
    return "".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpkg-installed", type=Path, default=DEFAULT_VCPKG_INSTALLED)
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify the committed file matches; write nothing",
    )
    args = parser.parse_args()

    components = fixed_components() + vcpkg_components(args.vcpkg_installed)
    content = render_notices(components)
    path = REPO_ROOT / OUTPUT

    if args.check:
        # newline="" so an LF file is not silently compared as CRLF-free;
        # Path.read_text() only grew the argument in 3.13.
        current = None
        if path.is_file():
            with path.open(encoding="utf-8", newline="") as handle:
                current = handle.read()
        if current != content:
            print(f"{OUTPUT} is out of date.", file=sys.stderr)
            print("Run: python scripts/gen_third_party_notices.py", file=sys.stderr)
            return 1
        print(f"Up to date ({len(components)} components).")
        return 0

    path.write_text(content, encoding="utf-8", newline="")
    print(f"Wrote {OUTPUT} ({len(components)} components).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
