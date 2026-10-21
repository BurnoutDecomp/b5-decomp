"""Exercise the real PC keyboard provider on a hosted Windows desktop.

This links the production provider and overlapped reset TUs. The fixture sends
only window/control messages; it never injects global keyboard or mouse input.
--output-dir retains the compiler/run transcripts and a hash-pinned receipt.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

from fxgs_common import REPO, WORKFLOW, definition, settings

TAG = "CgsKeyboardPCContract"
NUMERIC = re.compile(r"CgsKeyboardPCContract: (\d+) checks, (\d+) failures")
EXPECTED_CHECKS = 157
MAX_RUNTIME_SECONDS = 30
EXPECTED_SPRINTF_DEFINITION_UTF8_LF_SHA256 = "ca8ec6863b82da887387d5f211a63e3d36593f3bde74fd0ec3387362e2bf69c7"
EXPECTED_SPRINTF_PROVIDER_SHA256 = "2f666e89be717d0c8c1848ac6ebae0e4ac6c26c9cdffa39148e3e523182299e0"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def output_text(value):
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value


def run_fixture(output):
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    test_cpp = Path(__file__).with_name("CgsKeyboardPCContract.cpp")
    runner_source = Path(__file__).resolve()
    production_sources = [
        REPO / "src/GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp",
        REPO / "src/GameShared/GameClasses/System/X360/CgsXOverlappedX360.cpp",
    ]
    identity_source = REPO / "src/GameShared/GameClasses/System/PC/CgsPcNetIdentity.cpp"
    string_utils_source = REPO / "src/GameShared/GameClasses/Core/CgsStringUtils.cpp"
    sprintf_signature = "void CgsCore::SPrintf("
    # Select the exact canonical provider body. Compiling all of string-utils
    # would pull unrelated CGSSTRNLOWER/assert-system dependencies into a keyboard
    # fixture. No formatting implementation is recreated here.
    string_utils_lf = string_utils_source.read_bytes().replace(b"\r\n", b"\n").decode("utf-8-sig")
    sprintf_definition = definition(string_utils_lf, sprintf_signature)
    sprintf_definition_path = output / "CgsKeyboardSPrintfDefinition.inc"
    sprintf_definition_path.write_bytes(sprintf_definition.encode("utf-8"))
    sprintf_provider = output / "CgsKeyboardSPrintfProvider.cpp"
    sprintf_provider.write_text(
        '#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
        '#include <cstdarg>\n#include <stdio.h>\n'
        '#include "CgsKeyboardSPrintfDefinition.inc"\n',
        encoding="utf-8", newline="\n",
    )
    if sha256(sprintf_definition_path) != EXPECTED_SPRINTF_DEFINITION_UTF8_LF_SHA256:
        raise ValueError("Canonical SPrintf definition differs from the reviewed source pin")
    if sha256(sprintf_provider) != EXPECTED_SPRINTF_PROVIDER_SHA256:
        raise ValueError("Generated SPrintf provider differs from the reviewed source pin")
    linked_sources = [*production_sources, identity_source, sprintf_provider]
    toolchain_inputs = [
        WORKFLOW / "tools/build/msvc_flags.txt",
        WORKFLOW / "tools/build/msvc_includes.txt",
        WORKFLOW / "tools/build/msvc_env.bat",
        REPO / "tests/fxgs_common.py",
    ]
    source_inputs = [test_cpp, *production_sources]
    input_files = [runner_source, *source_inputs, *toolchain_inputs]
    input_receipt = [
        {
            "path": str(path.resolve()),
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
            "kind": "runner" if path == runner_source else
                    "source" if path in source_inputs else "toolchain",
        }
        for path in input_files
    ]
    # The parent CI validator has an exact nine-record input_hashes contract.
    # Keep it intact and retain these additional, genuinely consumed providers
    # as additive source-integrity observations in the same hashed result.json.
    link_dependency_hashes = [
        {"path": str(path.resolve()), "bytes": path.stat().st_size,
         "sha256": sha256(path), "kind": kind}
        for path, kind in (
            (identity_source, "source"),
            (string_utils_source, "source"),
            (sprintf_provider, "generated_canonical_provider"),
            (sprintf_definition_path, "extracted_canonical_definition"),
        )
    ]

    includes = " ".join(
        '/I"' + str(WORKFLOW / path) + '"'
        for path in settings("msvc_includes.txt")
    )
    executable = output / "regression.exe"
    command = (
        "cl " + " ".join(settings("msvc_flags.txt"))
        + " /D_ALLOW_KEYWORD_MACROS=1 " + includes
        + ' "' + str(test_cpp) + '"'
        + "".join(' "' + str(source) + '"' for source in linked_sources)
        + ' /Fe:"' + str(executable) + '" /link /OPT:REF user32.lib gdi32.lib'
    )
    build_script = output / "build.cmd"
    build_script.write_text(
        '@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
        + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n'
        + command + "\nexit /b %ERRORLEVEL%\n",
        encoding="utf-8",
        newline="\r\n",
    )
    input_receipt.append({
        "path": str(build_script.resolve()),
        "bytes": build_script.stat().st_size,
        "sha256": sha256(build_script),
        "kind": "generated_build_script",
    })

    build_stdout = ""
    build_stderr = ""
    build_exit_code = None
    try:
        built = subprocess.run(
            ["cmd", "/c", str(build_script)],
            cwd=output,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        build_stdout = built.stdout
        build_stderr = built.stderr
        build_exit_code = built.returncode
    except (OSError, subprocess.SubprocessError) as error:
        build_stderr = str(error)

    run_stdout = ""
    run_stderr = ""
    run_exit_code = None
    run_timed_out = False
    if build_exit_code == 0 and executable.is_file():
        try:
            # subprocess.run owns and reaps this test process; its direct runtime
            # is capped at 30 seconds. The test does not start child processes.
            result = subprocess.run(
                [str(executable)],
                cwd=output,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=MAX_RUNTIME_SECONDS,
            )
            run_stdout = result.stdout
            run_stderr = result.stderr
            run_exit_code = result.returncode
        except subprocess.TimeoutExpired as error:
            run_timed_out = True
            run_stdout = output_text(error.stdout)
            run_stderr = output_text(error.stderr)
        except (OSError, subprocess.SubprocessError) as error:
            run_stderr = str(error)

    combined_run_output = run_stdout + "\n" + run_stderr
    ui_unavailable = "UI_UNAVAILABLE" in combined_run_output
    numeric_match = NUMERIC.search(run_stdout)
    numeric = None
    if numeric_match is not None:
        numeric = {
            "checks": int(numeric_match.group(1)),
            "failures": int(numeric_match.group(2)),
        }

    failures = []
    if build_exit_code != 0:
        failures.append("compiler_or_linker_failed")
    if build_exit_code == 0 and not executable.is_file():
        failures.append("compiler_succeeded_without_executable")
    if run_timed_out:
        failures.append("runtime_timeout")
    if run_exit_code is not None and run_exit_code != 0:
        failures.append("fixture_nonzero_exit")
    if numeric is None:
        failures.append("numeric_result_missing")
    elif (numeric["checks"] <= 0 or numeric["checks"] != EXPECTED_CHECKS
          or numeric["failures"] != 0):
        failures.append("numeric_checks_failed_or_empty")
    if ui_unavailable:
        failures.append("hosted_ui_unavailable")

    executable_info = None
    if executable.is_file():
        executable_info = {
            "path": str(executable.resolve()),
            "bytes": executable.stat().st_size,
            "sha256": sha256(executable),
        }

    receipt = {
        "tag": TAG,
        "status": "pass" if not failures else "fail",
        "process_exit_code": run_exit_code,
        "build_exit_code": build_exit_code,
        "run_exit_code": run_exit_code,
        "run_timeout_seconds": MAX_RUNTIME_SECONDS,
        "run_timed_out": run_timed_out,
        "ui_unavailable": ui_unavailable,
        "numeric": numeric,
        "numeric_expected": EXPECTED_CHECKS,
        "failures": failures,
        "command": command,
        "input_hashes": input_receipt,
        "link_dependency_hashes": link_dependency_hashes,
        "provider_extraction": {
            "canonical_source": str(string_utils_source.resolve()),
            "canonical_source_raw_sha256": sha256(string_utils_source),
            "signature": sprintf_signature,
            "source_eol_selection": "CRLF_to_LF_only",
            "definition": str(sprintf_definition_path.resolve()),
            "definition_utf8_lf_bytes": sprintf_definition_path.stat().st_size,
            "definition_utf8_lf_sha256": sha256(sprintf_definition_path),
            "definition_expected_utf8_lf_sha256": EXPECTED_SPRINTF_DEFINITION_UTF8_LF_SHA256,
            "provider": str(sprintf_provider.resolve()),
            "provider_bytes": sprintf_provider.stat().st_size,
            "provider_sha256": sha256(sprintf_provider),
            "provider_expected_sha256": EXPECTED_SPRINTF_PROVIDER_SHA256,
        },
        "executable": executable_info,
        "retained_outputs": {
            "build_stdout": "build.stdout.txt",
            "build_stderr": "build.stderr.txt",
            "run_stdout": "run.stdout.txt",
            "run_stderr": "run.stderr.txt",
            "build_script": "build.cmd",
            "sprintf_provider": "CgsKeyboardSPrintfProvider.cpp",
            "sprintf_definition": "CgsKeyboardSPrintfDefinition.inc",
        },
    }
    (output / "build.stdout.txt").write_text(build_stdout, encoding="utf-8")
    (output / "build.stderr.txt").write_text(build_stderr, encoding="utf-8")
    (output / "run.stdout.txt").write_text(run_stdout, encoding="utf-8")
    (output / "run.stderr.txt").write_text(run_stderr, encoding="utf-8")
    (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n",
                                        encoding="utf-8")

    print(build_stdout, end="")
    if build_stderr:
        print(build_stderr, end="")
    print(run_stdout, end="")
    if run_stderr:
        print(run_stderr, end="")
    if failures:
        print("CgsKeyboardPCContract runner failed: " + ", ".join(failures))
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        help="Retain transcripts, executable and result.json in a new/empty folder")
    args = parser.parse_args()

    if args.output_dir is not None:
        output = args.output_dir.resolve()
        if output.exists() and (not output.is_dir() or any(output.iterdir())):
            parser.error("Refusing to overwrite fixture receipts: " + str(output))
        output.mkdir(parents=True, exist_ok=True)
        return run_fixture(output)

    with tempfile.TemporaryDirectory(prefix="brn_keyboard_contract_") as directory:
        return run_fixture(Path(directory))


if __name__ == "__main__":
    raise SystemExit(main())
