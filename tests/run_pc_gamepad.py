"""Build production SDL input with virtual controllers and a packaged-runtime probe."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from fxgs_common import WORKFLOW, REPO, STRSTREAM_CPP, settings, definition


def main():
    sdl = REPO / "vendor/sdl3"
    subprocess.run([os.sys.executable, str(WORKFLOW / "tools/build/build_sdl3.py")], check=True)
    with tempfile.TemporaryDirectory(prefix="brn_sdl_gamepad_") as directory:
        output = Path(directory)
        assertion = (REPO / "src/GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.cpp").read_text(encoding="utf-8")
        helper = definition(assertion, "    static bool IsAssertReleaseKeyDownPC()")
        modal = definition(assertion, "    void Manager::DoAssert()")
        if modal.count("IsAssertReleaseKeyDownPC()") != 2 or "while (IsAssertReleaseKeyDownPC() &&" not in modal:
            raise RuntimeError("The assert press and release loops must both poll mapped End")
        (output / "assert_end_key.inc").write_text(helper, encoding="utf-8")
        includes = '/I"' + str(output) + '" ' + " ".join('/I"' + str(WORKFLOW / path) + '"' for path in settings("msvc_includes.txt"))
        executable = output / "regression.exe"
        command = ('cl ' + ' '.join(settings("msvc_flags.txt")) +
                   ' /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public ' + includes +
                   ' "' + str(Path(__file__).with_name("PCGamepad.cpp")) + '" "' +
                   str(STRSTREAM_CPP) + '" /Fe:"' + str(executable) +
                   '" /link /OPT:REF "' + str(sdl / "lib/x64/SDL3.lib") + '"')
        script = output / "build.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") +
                          '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command +
                          '\nexit /b %ERRORLEVEL%\n', newline="\r\n")
        built = subprocess.run(["cmd", "/c", str(script)], cwd=output, capture_output=True, text=True)
        if built.returncode:
            print(built.stdout, built.stderr)
            return built.returncode
        shutil.copy2(sdl / "lib/x64/SDL3.dll", output)
        shutil.copy2(sdl / "LICENSE.txt", output / "SDL3-LICENSE.txt")
        environment = os.environ.copy()
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        environment["PATH"] = os.path.join(os.environ["SystemRoot"], "System32")
        for arguments in ([], ["--native"]):
            result = subprocess.run([str(executable), *arguments], cwd=output, env=environment,
                                    capture_output=True, text=True, timeout=30)
            print(result.stdout, result.stderr, end="")
            if result.returncode:
                return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
