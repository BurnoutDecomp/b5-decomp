import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from fxgs_common import WORKFLOW, settings

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)

REPO = Path(__file__).resolve().parents[1]
TEST_SOURCE = Path(__file__).with_name("PCOcclusionQueries.cpp")
PROVIDER_SOURCE = REPO / "src/pc/gcm/renderengine/OcclusionQueryProviderPC.cpp"
TAG = "PCOcclusionQueries"
BUILD_TIMEOUT_SECONDS = 120
RUN_TIMEOUT_SECONDS = 30
# Runner exit codes: 0 pass, 1 behavior/build/runtime failure, 77 native setup unavailable.


def print_tail(label, output, limit=6000):
    if output:
        print(f"{label}:\n{output[-limit:]}", end="" if output.endswith("\n") else "\n")


def text_value(value):
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value or ""


def main():
    print(f"{TAG}: direct provider leaf only; WorldDraw observer wiring and renderer activation remain unproven")
    output = Path(tempfile.mkdtemp(prefix="brn_pc_occlusion_queries_"))
    def save_log(name, value):
        (output / name).write_text(text_value(value), encoding="utf-8")

    def retain(reason):
        (output / "status.txt").write_text(reason + "\n", encoding="utf-8")
        print(f"ARTIFACTS retained: {output}")

    includes = f'/I"{output}" ' + " ".join(
        f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
    map_path = output / (TAG + ".map")
    command = (
        "cl " + " ".join(settings("msvc_flags.txt"))
        + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
        + "d3d9.lib user32.lib " + includes
        + f' "{TEST_SOURCE}" "{PROVIDER_SOURCE}"'
        + f' /Fe:{TAG}.exe /link /OPT:REF /MAP:"{map_path}"'
    )
    build_script = output / "build.cmd"
    build_script.write_text(
        '@echo off\n'
        + f'call "{WORKFLOW / "tools/build/msvc_env.bat"}" >nul 2>&1\n'
        + 'if errorlevel 1 exit /b 1\n'
        + command + '\n'
        + 'if errorlevel 1 exit /b 90\n'
        + 'exit /b 0\n',
        encoding="utf-8", newline="\r\n")

    try:
        build = subprocess.run(["cmd", "/c", str(build_script)], cwd=output,
                               capture_output=True, text=True,
                               timeout=BUILD_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        save_log("compile.stdout.log", error.stdout)
        save_log("compile.stderr.log", error.stderr)
        print(f"{TAG}: FAIL (compile exceeded {BUILD_TIMEOUT_SECONDS}s)")
        retain("compile timeout")
        return 1
    save_log("compile.stdout.log", build.stdout)
    save_log("compile.stderr.log", build.stderr)
    print_tail("compile stdout", build.stdout)
    print_tail("compile stderr", build.stderr)
    if build.returncode != 0:
        print(f"{TAG}: FAIL (native fixture did not compile; exit {build.returncode})")
        retain(f"compile failure, exit {build.returncode}")
        return 1

    executable = output / (TAG + ".exe")
    try:
        run = subprocess.run([str(executable)], cwd=output,
                             capture_output=True, text=True,
                             timeout=RUN_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        save_log("runtime.stdout.log", error.stdout)
        save_log("runtime.stderr.log", error.stderr)
        print(f"{TAG}: FAIL (native query wait exceeded {RUN_TIMEOUT_SECONDS}s)")
        retain("native runtime timeout")
        return 1
    save_log("runtime.stdout.log", run.stdout)
    save_log("runtime.stderr.log", run.stderr)
    print_tail("fixture stdout", run.stdout)
    print_tail("fixture stderr", run.stderr)

    if run.returncode == 77 or "UNAVAILABLE:" in run.stdout:
        print(f"{TAG}: UNAVAILABLE (native D3D9Ex/query setup did not support a behavior verdict)")
        retain("native setup unavailable")
        return 77
    if run.returncode != 0:
        print(f"{TAG}: FAIL (native fixture exit {run.returncode})")
        retain(f"native fixture exit {run.returncode}")
        return 1

    match = re.search(re.escape(TAG) + r": (\d+) checks, (\d+) failures", run.stdout)
    if match is None:
        print(f"{TAG}: FAIL (fixture produced no numeric result)")
        retain("missing numeric result")
        return 1
    checks, failures = (int(value) for value in match.groups())
    if failures:
        print(f"{TAG}: FAIL ({checks - failures}/{checks} checks; {failures} failures)")
        retain(f"behavior failures: {failures} of {checks}")
        return 1

    skipped = []
    if "SKIP_RESET_RECOVERY:" in run.stdout:
        skipped.append("ResetEx recovery")
    if "SUBCASE_UNAVAILABLE:" in run.stdout:
        skipped.append("safe HRESULT failure witness")
    if skipped:
        print(f"{TAG}: PARTIAL ({checks}/{checks} checks); unavailable: {', '.join(skipped)}")
        retain("partial: " + ", ".join(skipped))
        return 0

    shutil.rmtree(output, ignore_errors=True)
    print(f"{TAG}: PASS ({checks}/{checks} checks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
