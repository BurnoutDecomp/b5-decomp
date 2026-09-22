"""Check the production car-car shaping block against ARTIST-derived cases.
Optional first argument is a pre-fix BrnDeformableObject.cpp snapshot.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.cpp"
    source = path.read_text(encoding="utf-8-sig")
    start = re.search(r"        // -------- showtime / bounce-boost shaping", source, re.I).start()
    end = source.index("        // -------- apply the equal-and-opposite impulse", start)
    block = source[start:end]
    constants = "\n".join(re.findall(r"    static const [^;]+;", source[:source.index("    bool DeformableObject::ApplyCarCarImpulse")]))
    tail_start = source.index("        StoredImpulseContact lReverseContact;")
    tail_end = source.index("        static const bool lbWatchPair", tail_start) if "        static const bool lbWatchPair" in source[tail_start:] else source.index("        return true;", tail_start)
    tail = source[tail_start:tail_end]
    sensor_source = (REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.cpp").read_text(encoding="utf-8-sig")
    inverse = definition(sensor_source, "\tvoid StoredImpulseContact::GetInverse(")
    random = (REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp").read_text(encoding="utf-8-sig")
    constants += "\nnamespace CgsNumeric {\n" + re.search(r"(?:static )?const u64 KU_RANDOM_LCG_MULTIPLIER[^;]+;", random)[0]
    constants += "\n" + definition(random, "    u32 Random::RandomUInt()") + "\n}"
    with tempfile.TemporaryDirectory(prefix="brn_car_car_bounce_") as directory:
        output = Path(directory)
        (output / "car_car_shaping.inc").write_text(block, encoding="utf-8")
        (output / "car_car_constants.inc").write_text(constants, encoding="utf-8")
        (output / "car_car_reverse.inc").write_text(tail, encoding="utf-8")
        (output / "car_car_inverse.inc").write_text(inverse, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("CarCarBounce.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
