"""Shared plumbing for the FX-GS regression runners (crash-parity fix round, 2026-09-23).

Every FX-GS runner reads the PRODUCTION source -- the working tree, or any b5 revision with
--rev (the RED side of a fix is `--rev <fix commit>~1`) -- extracts the bodies under test by
their signatures, and compiles them with the canonical flags against a small fixture .cpp.
A revision whose source lacks a body cannot build the numeric test: every numeric check is then
counted as failed, with the missing bodies named.

Run the runners from the workflow checkout with NoDefaultCurrentDirectoryInExePath unset:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_<name>.py [--rev <rev>]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent


class Tree:
    """A b5 source tree: the working tree (rev None) or a git revision."""

    def __init__(self, rev=None):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        result = subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                                capture_output=True, text=True, encoding="utf-8")
        if result.returncode != 0:
            return ""
        text = result.stdout
        return text[1:] if text.startswith("\ufeff") else text


def definition(source, signature):
    """The full text of the definition starting at `signature` (brace-balanced, comments and
    string literals skipped when counting)."""
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
    raise ValueError("unterminated body: " + signature)


def code_only(text):
    """Strip comments so structural checks cannot be satisfied by a comment."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def body_or_empty(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def extract(tree, relative, signatures):
    """Return (texts, missing) for the given definition signatures of one file."""
    source = tree.read(relative)
    texts, missing = [], []
    for signature in signatures:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            missing.append(signature.strip())
    return texts, missing


STRSTREAM_CPP = REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"


def compile_and_run(test_cpp, inc_name, inc_text, tag, extra_flags="", shadow=None, extra_sources=(),
                    extra_files=None):
    """Write inc_text as <tmp>/<inc_name>, compile test_cpp (+ extra_sources, e.g. the real
    CgsStrStream.cpp the assert-message paths need) with the canonical flags (+ the access macros
    the fixtures need) and run it. `shadow` maps a src-relative header path to the revision's text
    so the extracted bodies compile against their own revision's header. `extra_files` maps more
    file names to texts written beside the .inc (e.g. a generated configuration include).
    Returns (checks, failures) parsed from the '<tag>: N checks, M failures' line, or None."""
    with tempfile.TemporaryDirectory(prefix="brn_fxgs_") as directory:
        output = Path(directory)
        (output / inc_name).write_text(inc_text, encoding="utf-8")
        for name, text in (extra_files or {}).items():
            (output / name).write_text(text, encoding="utf-8")
        shadow_dir = output / "shadow"
        for relative, text in (shadow or {}).items():
            target = shadow_dir / Path(relative).relative_to("src")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding="utf-8")
        includes = f'/I"{shadow_dir}" /I"{output}" ' + " ".join(
            f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
                   + extra_flags + " " + includes
                   + f' "{test_cpp}"' + "".join(f' "{source}"' for source in extra_sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + "\nif errorlevel 1 exit /b 90\nregression.exe\nexit /b %ERRORLEVEL%\n",
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output, capture_output=True, text=True)
        sys.stdout.write(result.stdout[-8000:])
        sys.stderr.write(result.stderr[-4000:])
        match = re.search(re.escape(tag) + r": (\d+) checks, (\d+) failures", result.stdout)
        if match is None:
            print(f"NUMERIC: the test did not run (exit {result.returncode})")
            return None
        return int(match.group(1)), int(match.group(2))


def report(name, wiring, numeric, numeric_total):
    """Print the verdict lines and return the process exit code."""
    for label, passed in wiring:
        print(("PASS  " if passed else "FAIL  ") + label)
    wiring_failures = sum(1 for _, passed in wiring if not passed)
    if numeric is None:
        numeric = (numeric_total, numeric_total)
    checks, failures = numeric
    total = len(wiring) + checks
    failed = wiring_failures + failures
    print(f"{name}: wiring {len(wiring) - wiring_failures}/{len(wiring)}, "
          f"numeric {checks - failures}/{checks}; total {total - failed}/{total} pass ({failed} fail)")
    return 1 if failed else 0
