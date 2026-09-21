"""Compile and exercise the production mode-parameter initializer with poisoned storage."""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    source = (REPO / "src/GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.cpp").read_text(encoding="utf-8")
    start = source.index("void GameModeParams::Construct(")
    # Namespace methods close at column zero; nested scopes are indented.
    body = source[start:source.index("\n}", start) + 2]
    with tempfile.TemporaryDirectory(prefix="brn_mode_params_") as directory:
        output = Path(directory)
        extracted = output / "methods.cpp"
        extracted.write_text('#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"\n'
                             'namespace BrnGameState { const u32 KU_MAX_ACTIVE_RACE_CARS = 8u;\n'
                             + body + '\n}\n', encoding="utf-8")
        sources = [Path(__file__).with_name("GameModeParamsDefaults.cpp"), extracted]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public " + includes + " "
                   + " ".join(f'"{path}"' for path in sources) + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
