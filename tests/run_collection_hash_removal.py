"""Run from the workflow checkout: python b5-decomp/tests/run_collection_hash_removal.py.

Compile the collection hash-map methods verbatim from production sources. Isolating these
definitions avoids linking the full game's unrelated resource/scene dependencies.
The normal build remains the integration check for the complete translation units.
"""
from pathlib import Path
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent


def definition(path, signature):
    source = path.read_text(encoding="utf-8")
    start = source.index(signature)
    # These namespace-level methods close at column zero; nested blocks do not.
    return source[start:source.index("\n}", start) + 2]


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    with tempfile.TemporaryDirectory(prefix="brn_collection_hash_") as directory:
        output = Path(directory)
        source = REPO / "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.cpp"
        methods = [source.read_text(encoding="utf-8").split("// AttribSys runtime")[0], "namespace Attrib {"]
        for signature in ("u32 CollectionHashMap::GrowRequest(", "bool CollectionHashMap::Add(",
                          "u32 CollectionHashMap::FindIndex(", "bool CollectionHashMap::InternalAdd(",
                          "u32 CollectionHashMap::UpdateSearchLength(", "void CollectionHashMap::RebuildTable(",
                          "void CollectionHashMap::CopyFromOldTable(", "Collection* CollectionHashMap::RemoveIndex(",
                          "Collection* CollectionHashMap::Find("):
            methods.append(definition(source, signature))
        methods.append("}")
        extracted = output / "methods.cpp"
        extracted.write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("CollectionHashRemoval.cpp"), extracted]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " "
                   + " ".join(f'"{path}"' for path in sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
