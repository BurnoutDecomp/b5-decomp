"""Run the production AIAggression bodies against cases derived from the ARTIST assembly.

The bodies are extracted verbatim from src/GameSource/World/AI/BrnAIAggression.cpp (and StepTo
from BrnAIUtils.cpp), so the test exercises the shipped text, not a copy. An optional first
argument names a BrnAIAggression.cpp snapshot to test instead, e.g. the pre-fix source:

    git -C b5-decomp show HEAD~1:src/GameSource/World/AI/BrnAIAggression.cpp > pre.cpp
    python b5-decomp/tests/run_ai_aggression.py pre.cpp
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import REPO, WORKFLOW, settings

AGGRESSION = REPO / "src/GameSource/World/AI/BrnAIAggression.cpp"
UTILS = REPO / "src/GameSource/World/AI/BrnAIUtils.cpp"

# Every body a case reaches, directly or through a call. Order is irrelevant: the header
# declares every member, so the extracted definitions compile in any order.
MEMBERS = [
    "CalcSeparationAcrossToTarget", "CanSlam", "DetermineAttackSide", "GetLeadingSeparation",
    "GetMaxOvertakeSpeed", "GetMinFallBackSpeed", "GetPositionNextToTarget", "GetSeparation",
    "GetSpeedMatchSpeed", "OutOfSpeedMatchRange", "SetSlowOvertakingSpeed",
    "UpdateAggressionPassive", "UpdateAggressionStateBeFodder",
    "UpdateAggressionStateClipOffBehind", "UpdateAggressionStateFallPast",
    "UpdateAggressionStateOvertakeFast", "UpdateAggressionStateSpurtForward",
    "UpdateAggressionStateVeerExtreme",
]


def definition(source, name, qualifier):
    """Return the full text of the definition of `name`, found by its definition line."""
    pattern = re.compile(r"^[ \t]*(?:f32|bool|void|Vector3)[ \t]+" + qualifier + name + r"[ \t]*\(",
                         re.M)
    match = pattern.search(source)
    if match is None:
        raise ValueError(f"no definition of {qualifier}{name}")
    start = match.start()
    depth = 0
    # Balance braces while skipping comments and string literals.
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError(f"unclosed definition of {name}")


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else AGGRESSION
    source = path.read_text(encoding="utf-8-sig")
    utils = UTILS.read_text(encoding="utf-8-sig")
    # The file-scope rodata constants the bodies name (column-0 `const f32 KF_...`).
    constants = re.findall(r"^const f32\s+KF_\w+\s*=[^;]+;", source, re.M)
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;"] + constants
    chunks.append(definition(source, "CurveToKeepLarge", ""))
    chunks += [definition(source, name, r"(?:BrnAI::)?AIAggression::") for name in MEMBERS]
    chunks.append(definition(utils, "StepTo", ""))
    chunks.append("}")
    with tempfile.TemporaryDirectory(prefix="brn_ai_aggression_") as directory:
        output = Path(directory)
        (output / "aggression_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("AIAggression.cpp")}"'
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
