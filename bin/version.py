#!/usr/bin/env python3
"""Keep the hardcoded version and dependency references in sync.

library.json is the single source of truth. Everything else is derived:

  library.properties     the library version
  examples/*/platformio.ini
                         the lib_deps block, which must match the
                         dependencies declared in library.json

Usage:
  bin/version.py --check          verify everything agrees, exit 1 if not
  bin/version.py --set 1.2.0      write a new version, then re-sync
  bin/version.py --sync           re-sync derived files from library.json
"""

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LIBRARY_JSON = ROOT / "library.json"
LIBRARY_PROPERTIES = ROOT / "library.properties"
EXAMPLES = sorted((ROOT / "examples").glob("*/platformio.ini"))

VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")

# Dependencies that examples do not restate: they come with the toolchain,
# not with this library.
LIB_DEPS_HEADER = (
    "; these mirror the dependencies in library.json; bin/version.py --sync\n"
    "lib_deps =\n"
)


def read_json():
    return json.loads(LIBRARY_JSON.read_text(encoding="utf-8"))


def render_lib_deps(deps):
    """Render library.json dependencies as a platformio.ini lib_deps block."""
    lines = [LIB_DEPS_HEADER.rstrip("\n")]
    for name, spec in deps.items():
        if spec.startswith("http"):
            lines.append(f"    ; {name}")
            lines.append(f"    {spec}")
        else:
            lines.append(f"    {name}@{spec}")
    return "\n".join(lines) + "\n"


def replace_lib_deps(text, block):
    """Swap the lib_deps block in a platformio.ini for a freshly rendered one."""
    pattern = re.compile(
        r"(?:^; these mirror.*\n)?^lib_deps *=\n(?:^(?:    |;).*\n|^\n(?=    ))*",
        re.M,
    )
    if not pattern.search(text):
        raise SystemExit("no lib_deps block found; cannot sync")
    return pattern.sub(block, text, count=1)


def sync(write):
    """Return a list of problems; apply fixes when write is true."""
    data = read_json()
    version = data["version"]
    problems = []

    props = LIBRARY_PROPERTIES.read_text(encoding="utf-8")
    wanted = f"version={version}"
    if not re.search(rf"^{re.escape(wanted)}$", props, re.M):
        problems.append(f"library.properties: version is not {version}")
        if write:
            props = re.sub(r"^version=.*$", wanted, props, count=1, flags=re.M)
            LIBRARY_PROPERTIES.write_text(props, encoding="utf-8", newline="\n")

    block = render_lib_deps(data["dependencies"])
    for ini in EXAMPLES:
        text = ini.read_text(encoding="utf-8")
        updated = replace_lib_deps(text, block)
        if updated != text:
            rel = ini.relative_to(ROOT).as_posix()
            problems.append(f"{rel}: lib_deps does not match library.json")
            if write:
                ini.write_text(updated, encoding="utf-8", newline="\n")

    return problems


def set_version(version):
    if not VERSION_RE.match(version):
        raise SystemExit(f"not a MAJOR.MINOR.PATCH version: {version}")
    text = LIBRARY_JSON.read_text(encoding="utf-8")
    updated = re.sub(r'("version"\s*:\s*)"[^"]*"', rf'\1"{version}"', text, count=1)
    if updated == text:
        raise SystemExit("library.json: no version field to update")
    LIBRARY_JSON.write_text(updated, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--sync", action="store_true")
    group.add_argument("--set", metavar="VERSION")
    args = parser.parse_args()

    if args.set:
        set_version(args.set)

    problems = sync(write=not args.check)

    if args.check and problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        print("Run bin/version.py --sync to fix.", file=sys.stderr)
        return 1

    if args.check:
        print(f"version refs are consistent ({read_json()['version']})")
    else:
        for problem in problems:
            print(f"updated {problem.split(':')[0]}")
        print(f"version is now {read_json()['version']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
