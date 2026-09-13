"""Exercise production vertex-declaration cache retirement with counted COM references."""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import subprocess
import tempfile
from run_rival_impacts import WORKFLOW, REPO, settings


def definition(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    source = (REPO / "src/pc/gcm/renderengine/XenonD3D9Shims.cpp").read_text(encoding="utf-8-sig")
    methods = ["void WorldVd32_OnResourceMemoryFreed", "void WorldVd32_ReleaseAll"]
    chunks = ["namespace renderengine {\n" +
              "\n".join(definition(source, method) for method in methods) + "\n}"]
    with tempfile.TemporaryDirectory(prefix="brn_vd_lifetime_") as directory:
        output = Path(directory)
        (output / "restored_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("WorldVertexDeclarationLifetime.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
