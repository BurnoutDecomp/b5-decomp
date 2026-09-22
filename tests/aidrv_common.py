"""Shared helpers for the FX-AIDRV regression runners (crash parity 2026-09-22).

Every runner extracts PRODUCTION bodies from the real source files (so the test exercises the
shipped text, not a copy) and compiles them against small fixtures with the canonical flags
(tools/build/msvc_flags.txt, msvc_includes.txt, msvc_env.bat).

`--rev <b5 git rev>` reads every source file from that revision instead of the working tree, so
the RED side of a fix is `--rev <fix commit>~1` (or `--rev HEAD` before the fix is committed).
A body that does not exist in the revision under test is replaced by the stub the runner names,
so the numeric checks report the missing behaviour instead of failing to compile.

Run from the workflow checkout (this shell exports NoDefaultCurrentDirectoryInExePath=1):
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_<name>.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW  # noqa: E402,F401


class Tree:
    """The b5-decomp sources, from the working tree or from one git revision."""

    def __init__(self, rev=None):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                              check=True, capture_output=True, text=True,
                              encoding="utf-8").stdout


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5-decomp revision to read the sources from")
    return parser.parse_args(argv)


def body_or_stub(source, signature, stub):
    """The production definition starting at `signature`, or `stub` when the source has none."""
    try:
        return definition(source, signature)
    except ValueError:
        return stub


def constant(source, name):
    """One file-scope `const <type> NAME = ...;` line, found by name."""
    match = re.search(r"^[ \t]*(?:static[ \t]+)?const[ \t]+\w+[ \t]+" + re.escape(name)
                      + r"(?:\[[^\]]*\])?[ \t]*=[^;]+;", source, re.M)
    if match is None:
        raise ValueError(f"no constant {name}")
    return match[0]


def compile_and_run(test_cpp, chunks, extra_sources=(), prefix="brn_aidrv_", defines=""):
    """Write `chunks` to restored_methods.inc beside the fixture, compile, run, return the code."""
    with tempfile.TemporaryDirectory(prefix=prefix) as directory:
        output = Path(directory)
        (output / "restored_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        sources = [Path(__file__).with_name(test_cpp)] + [Path(s) for s in extra_sources]
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
                   + defines + " " + includes + f' /I"{output}" '
                   + " ".join(f'"{s}"' for s in sources)
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        return subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode
