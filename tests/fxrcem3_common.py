"""Shared plumbing for the FX-RCEM3 regression runners (crash parity 2026-09-23).

Every runner EXTRACTS the production text from the real source file (or from a git revision with
--pre-fix <rev>, which is how the RED side is shown), pastes it into a small harness compiled with
the canonical flags (tools/build/msvc_flags.txt + msvc_includes.txt, tools/build/msvc_env.bat) and
runs numeric/structural checks derived from the ARTIST asm.

Run the runners from the workflow checkout with
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_<name>.py
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
RCEM = "src/GameSource/World/EntityModules/RaceCarEntityModule/"


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
    """The full text of the function whose definition starts with `signature` (brace-balanced,
    skipping comments and string literals). Raises ValueError when the signature is absent."""
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
    raise ValueError(f"Unclosed function: {signature}")


def optional_definition(source, signature):
    """definition(), or '' when the signature is absent (the pre-fix side of a missing body)."""
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def code_mask(text):
    """`text` with comments and literals blanked (same length), for structural regex checks."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text)


def switch_arm(body, label):
    """The text of `case <label>:` up to the next case/default at the same depth, or None."""
    masked = code_mask(body)
    found = re.search(r"case\s+" + label + r"\s*:", masked)
    if not found:
        return None
    depth, position = 0, found.end()
    token = re.compile(r"[{}]|\bcase\b|\bdefault\s*:")
    while True:
        match = token.search(masked, position)
        if match is None:
            raise ValueError("unterminated arm " + label)
        text = match.group(0)
        if text == "{":
            depth += 1
        elif text == "}":
            depth -= 1
            if depth < 0:
                return body[found.start():match.start()]
        elif depth == 0:
            return body[found.start():match.start()]
        position = match.end()


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def build_and_run(harness, pieces, label, extra_flags="", extra_sources=()):
    """Write `pieces` ({file name: text}) beside a compile of `harness`, run it, return its rc."""
    with tempfile.TemporaryDirectory(prefix=f"brn_{label}_") as directory:
        out = Path(directory)
        for name, text in pieces.items():
            (out / name).write_text(text + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        sources = " ".join(f'"{path}"' for path in (harness,) + tuple(extra_sources))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + extra_flags + " " + includes
                   + f' /I"{out}" ' + sources + ' /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + command + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        return subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
