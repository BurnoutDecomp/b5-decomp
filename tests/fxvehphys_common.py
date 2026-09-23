"""Shared plumbing for the FX-VEHPHYS regression runners (crash parity 2026-09-23).

Every runner EXTRACTS the production text from the real source file (or from a git revision with
--pre-fix <rev>, which is how the RED side is shown), pastes it into a small harness compiled with
the canonical flags (tools/build/msvc_flags.txt + msvc_includes.txt, tools/build/msvc_env.bat) and
runs numeric checks derived from the ARTIST asm.

Run the runners from the workflow checkout with
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_<name>.py [--pre-fix <b5 rev>]
(this shell exports NoDefaultCurrentDirectoryInExePath=1, which stops cmd.exe finding
regression.exe in the current directory -- not a code failure).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent


def pre_fix_rev(argv):
    """The --pre-fix <rev> argument, or None for the working tree."""
    return argv[argv.index("--pre-fix") + 1] if "--pre-fix" in argv else None


def read(rel, rev=None):
    """A source file from the working tree, or from git revision `rev` ('' when absent there)."""
    if rev:
        result = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"],
                                capture_output=True, text=True, encoding="utf-8")
        return result.stdout.replace("\r\n", "\n") if result.returncode == 0 else ""
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def definition(source, signature):
    """The full text from `signature` to the brace that closes the first block it opens (brace-
    balanced, skipping comments and string/char literals). Raises ValueError when absent."""
    start = source.index(signature)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                             source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError(f"Unclosed block: {signature}")


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def build_and_run(harness, pieces, label, args=()):
    """Write `pieces` ({file name: text}) beside a compile of `harness`, run it, return its rc."""
    with tempfile.TemporaryDirectory(prefix=f"brn_{label}_") as directory:
        out = Path(directory)
        for name, text in pieces.items():
            (out / name).write_text(text + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
                   + str(harness) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + command + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe ' + " ".join(args) + '\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        return subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
