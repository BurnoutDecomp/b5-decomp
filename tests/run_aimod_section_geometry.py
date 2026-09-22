"""FX-AIMOD G07-D5: replay the production AISection::PassesThrough edge cull.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_section_geometry.py [AISection.cpp]
Optional argument: a pre-fix snapshot of SharedClasses/AI/AISection.cpp (the RED side).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SECTION = REPO / "src/SharedClasses/AI/AISection.cpp"


def main():
    source = (Path(sys.argv[1]) if len(sys.argv) > 1 else SECTION).read_text(encoding="utf-8-sig")
    epsilon = re.search(r"static const f32 KF_INTERSECTION_EPSILON\s*=[^;]+;", source)[0]
    chunks = ["namespace BrnAI {", epsilon,
              definition(source, "    bool AISection::IsInside("),
              definition(source, "    bool AISection::PassesThrough("),
              "}"]
    with tempfile.TemporaryDirectory(prefix="brn_aimod_section_") as directory:
        output = Path(directory)
        (output / "restored_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("AIModSectionGeometry.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
