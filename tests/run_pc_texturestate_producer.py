"""Build and run the real TextureState producer against its sampler reader.

The fixture keeps a unique command/output/input-hash packet by default. It uses
the canonical MSVC flags/includes, compiles the production PC TextureState TU,
and compiles the unchanged SDK SamplerState TU through a tiny generated adapter
so the positive control is produced by the real initializer.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = next(
    (candidate for candidate in (REPO, *REPO.parents)
     if (candidate / "tools" / "build" / "msvc_flags.txt").is_file()),
    None,
)
if WORKFLOW is None:
    raise SystemExit("Cannot locate the workflow tools/build/msvc_flags.txt for canonical compile settings")

SAMPLER_CPP = Path("src/SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.cpp")
PRODUCTION_CPP = Path("src/pc/gcm/renderengine/texturestate.cpp")
SOURCE_INPUTS = [
    Path("tests/PCTextureStateProducer.cpp"),
    Path("tests/run_pc_texturestate_producer.py"),
    PRODUCTION_CPP,
    Path("src/pc/gcm/renderengine/renderstates.h"),
    Path("src/pc/gcm/renderengine/WorldTextureFiltering.h"),
    Path("src/pc/gcm/renderengine/texture.h"),
    SAMPLER_CPP,
    Path("src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.cpp"),
    Path("src/GameShared/GameClasses/Fonts/CgsFont.cpp"),
]
ARTIST_INPUTS = [
    Path(".ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B62630.json"),
    Path(".ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B62720.json"),
    Path(".ida-exports/BURNOUT_X360_ARTIST.XEX/0x827ED538.json"),
    Path(".ida-exports/BURNOUT_X360_ARTIST.XEX/0x827E8950.json"),
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def snapshot_file(source: Path, snapshot_relative: Path, destination_root: Path) -> dict[str, str]:
    if not source.is_file():
        return {"status": "missing", "source": str(source)}
    destination = destination_root / snapshot_relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return {
        "status": "frozen",
        "source": str(source),
        "snapshot": str(destination.relative_to(destination_root.parent)),
        "sha256": sha256(source),
        "bytes": str(source.stat().st_size),
    }


def canonical_lines(name: str) -> list[str]:
    path = WORKFLOW / "tools" / "build" / name
    return [line for line in path.read_text(encoding="utf-8-sig").splitlines()
            if line and not line.startswith("#")]


def resolve_include(entry: str) -> Path:
    normalized = entry.replace("\\", "/")
    if normalized.startswith("b5-decomp/"):
        normalized = normalized[len("b5-decomp/"):]
    return REPO / normalized


def command_script(path: Path, body: str) -> None:
    path.write_text("@echo off\r\n" + body + "\r\n", encoding="utf-8", newline="")


def run_cmd(script: Path, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(script)],
                          cwd=cwd, capture_output=True, text=True, errors="replace")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def default_evidence_dir() -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    return WORKFLOW / "scratch" / "DEBUG_MENU_1011" / f"texturestate_producer_fixture_{stamp}"


def git_value(*args: str) -> str:
    result = subprocess.run(["git", "-C", str(REPO), *args], capture_output=True,
                            text=True, encoding="utf-8", errors="replace")
    return result.stdout.strip() if result.returncode == 0 else ""


def create_shader(evidence: Path, temp_dir: Path, manifest: dict) -> bool:
    shader_dir = WORKFLOW / "tools" / "assets" / "shaders"
    sys.path.insert(0, str(shader_dir))
    try:
        from convert_shaders_bundle import find_fxc
        fxc = Path(find_fxc())
    except Exception as exc:  # Keep tool/setup failure distinct from the native contract.
        manifest["shader_compile"] = {"status": "unsupported", "reason": f"find_fxc: {exc}"}
        return False

    if not fxc.is_file():
        manifest["shader_compile"] = {"status": "unsupported", "reason": f"fxc is missing: {fxc}"}
        return False

    source = temp_dir / "texturestate_producer.fx"
    output = temp_dir / "texturestate_producer.fxo"
    source_text = (
        "sampler3D Source : register(s0);\n"
        "float4 Main(float2 unused : TEXCOORD0) : COLOR0 {\n"
        "    return tex3D(Source, float3(0.5f, 1.0f, 0.5f));\n"
        "}\n"
    )
    write_text(source, source_text)
    command = [str(fxc), "/nologo", "/T", "ps_3_0", "/E", "Main", "/O2", "/Fo", str(output), str(source)]
    result = subprocess.run(command, cwd=temp_dir, capture_output=True, text=True, errors="replace")
    write_text(evidence / "commands" / "fxc_command.txt", subprocess.list2cmdline(command) + "\n")
    write_text(evidence / "outputs" / "fxc_stdout.txt", result.stdout)
    write_text(evidence / "outputs" / "fxc_stderr.txt", result.stderr)
    (evidence / "inputs" / "shader" / source.name).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, evidence / "inputs" / "shader" / source.name)
    if result.returncode != 0 or not output.is_file():
        manifest["shader_compile"] = {
            "status": "failed",
            "command": subprocess.list2cmdline(command),
            "exit_code": result.returncode,
            "fxc_path": str(fxc),
            "fxc_sha256": sha256(fxc),
        }
        return False

    bytecode = output.read_bytes()
    if not bytecode or len(bytecode) % 4:
        manifest["shader_compile"] = {
            "status": "failed",
            "reason": "fxc output is empty or not DWORD-aligned",
            "fxc_path": str(fxc),
        }
        return False
    words = [int.from_bytes(bytecode[index:index + 4], "little")
             for index in range(0, len(bytecode), 4)]
    include_text = "static const DWORD kauTextureStateProducerShader[] = {\n" + ",\n".join(
        "    0x%08Xu" % word for word in words
    ) + "\n};\n"
    include_path = temp_dir / "pc_texturestate_producer_shader.inc"
    write_text(include_path, include_text)
    shutil.copy2(output, evidence / "outputs" / output.name)
    shutil.copy2(include_path, evidence / "inputs" / include_path.name)
    manifest["shader_compile"] = {
        "status": "compiled",
        "command": subprocess.list2cmdline(command),
        "exit_code": result.returncode,
        "fxc_path": str(fxc),
        "fxc_sha256": sha256(fxc),
        "source_sha256": sha256(source),
        "bytecode_sha256": sha256(output),
        "bytecode_bytes": len(bytecode),
        "dword_count": len(words),
    }
    manifest["generated_shader_include"] = str(include_path)
    return True


def make_adapter(path: Path) -> None:
    # Compile the actual SDK implementation in this adapter TU, then expose a
    # type-erased call for a reference block. The adapter contains no sampler body.
    content = (
        '#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.cpp"\n'
        'extern "C" void PCTextureStateProducerBuildSdkSamplerState(\n'
        '    void* lpStateBytes, const void* lpParameterBytes)\n'
        '{\n'
        '    auto* lpState = reinterpret_cast<renderengine::SamplerStateData*>(lpStateBytes);\n'
        '    auto* lpParameters = reinterpret_cast<const renderengine::SamplerStateParameters*>(lpParameterBytes);\n'
        '    renderengine::SamplerState::Initialize(&lpState, lpParameters);\n'
        '}\n'
    )
    write_text(path, content)


def parse_fixture_output(output: str) -> dict:
    match = re.search(r"PCTextureStateProducer: (\d+) checks, (\d+) failures", output)
    if not match:
        return {"status": "no_numeric_result"}
    checks, failures = (int(match.group(1)), int(match.group(2)))
    return {"status": "pass" if failures == 0 else "red_contract_failure",
            "checks": checks, "failures": failures}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", type=Path,
                        help="new, unused directory for the immutable command/output/input-hash packet")
    args = parser.parse_args()
    evidence = (args.evidence_dir or default_evidence_dir()).resolve()
    if evidence.exists():
        print(f"Refusing to overwrite existing evidence directory: {evidence}")
        return 2
    evidence.mkdir(parents=True)
    (evidence / "inputs").mkdir()
    (evidence / "commands").mkdir()
    (evidence / "outputs").mkdir()
    (evidence / "native").mkdir()
    temp_dir = evidence / "native"

    manifest = {
        "schema_version": 1,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "repo": str(REPO),
        "workflow": str(WORKFLOW),
        "branch": git_value("branch", "--show-current"),
        "head": git_value("rev-parse", "HEAD"),
        "working_tree_status": git_value("status", "--short", "--branch"),
        "base_commit": "5ad64d67f184bacf7795c9328d0e3109bbe6d4d0",
        "fixture_scope": ["tests/PCTextureStateProducer.cpp", "tests/run_pc_texturestate_producer.py"],
        "claim_boundary": "The fixture tests generated state translation and a native D3D9 boundary draw; it does not claim game activation or runtime completion.",
        "inputs": {},
        "commands": {},
        "outputs": {},
    }

    for relative in SOURCE_INPUTS:
        manifest["inputs"][str(relative)] = snapshot_file(
            REPO / relative, Path("repo") / relative, evidence / "inputs")
    for relative in ARTIST_INPUTS:
        manifest["inputs"][str(relative)] = snapshot_file(
            WORKFLOW / relative, Path("original") / relative, evidence / "inputs")
    for machine_file in ("msvc_flags.txt", "msvc_includes.txt", "msvc_env.bat"):
        relative = Path("tools/build") / machine_file
        manifest["inputs"][str(relative)] = snapshot_file(
            WORKFLOW / relative, Path("workflow") / relative, evidence / "inputs")

    temp_dir.mkdir(parents=True, exist_ok=True)
    if not create_shader(evidence, temp_dir, manifest):
        manifest["status"] = "shader_setup_failure"
        freeze_manifest(evidence, manifest)
        print(f"{manifest['status']}: see {evidence}")
        return 77

    adapter = temp_dir / "sdk_samplerstate_adapter.cpp"
    make_adapter(adapter)
    shutil.copy2(adapter, evidence / "inputs" / adapter.name)

    flags = canonical_lines("msvc_flags.txt")[0]
    include_entries = canonical_lines("msvc_includes.txt")
    resolved_includes = [(entry, resolve_include(entry)) for entry in include_entries]
    include_dirs = [directory for _, directory in resolved_includes if directory.is_dir()]
    manifest["canonical_include_resolution"] = {
        "used_in_order": [str(directory) for directory in include_dirs],
        "missing_optional_entries": [
            {"entry": entry, "resolved_path": str(directory)}
            for entry, directory in resolved_includes if not directory.is_dir()
        ],
    }

    include_args = " ".join('/I"' + str(directory) + '"' for directory in include_dirs)
    include_args += ' /I"' + str(temp_dir) + '"'
    test_cpp = REPO / "tests" / "PCTextureStateProducer.cpp"
    production_cpp = REPO / PRODUCTION_CPP
    exe = temp_dir / "pc_texturestate_producer.exe"
    compile_line = (
        f"cl {flags} /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
        f"{include_args} \"{test_cpp}\" \"{production_cpp}\" \"{adapter}\" "
        f"/Fe:\"{exe}\" /link /OPT:REF d3d9.lib user32.lib"
    )
    compile_script = evidence / "commands" / "compile.cmd"
    command_script(compile_script,
        f'call "{WORKFLOW / "tools" / "build" / "msvc_env.bat"}" >nul 2>&1\r\n'
        "if errorlevel 1 exit /b 89\r\n"
        + compile_line + "\r\nexit /b %ERRORLEVEL%")
    write_text(evidence / "commands" / "compile_command.txt", compile_line + "\n")
    compile_result = run_cmd(compile_script, temp_dir)
    write_text(evidence / "outputs" / "compile_stdout.txt", compile_result.stdout)
    write_text(evidence / "outputs" / "compile_stderr.txt", compile_result.stderr)
    manifest["commands"]["compile"] = {
        "script": str(compile_script),
        "command": compile_line,
        "exit_code": compile_result.returncode,
    }
    manifest["outputs"]["compile_stdout"] = {
        "path": "outputs/compile_stdout.txt", "sha256": sha256(evidence / "outputs" / "compile_stdout.txt")
    }
    manifest["outputs"]["compile_stderr"] = {
        "path": "outputs/compile_stderr.txt", "sha256": sha256(evidence / "outputs" / "compile_stderr.txt")
    }
    for obj in temp_dir.glob("*.obj"):
        shutil.copy2(obj, evidence / "outputs" / obj.name)
    if compile_result.returncode != 0 or not exe.is_file():
        manifest["status"] = "compile_failure" if compile_result.returncode != 89 else "compiler_unavailable"
        manifest["compile_result"] = {"exit_code": compile_result.returncode}
        freeze_manifest(evidence, manifest)
        print(f"{manifest['status']}: see {evidence}")
        return 90

    shutil.copy2(exe, evidence / "outputs" / exe.name)
    manifest["outputs"]["native_executable"] = {
        "path": f"outputs/{exe.name}", "sha256": sha256(exe), "bytes": exe.stat().st_size
    }
    execute_script = evidence / "commands" / "native.cmd"
    command_script(execute_script, f'"{exe}"\r\nexit /b %ERRORLEVEL%')
    write_text(evidence / "commands" / "native_command.txt", f'"{exe}"\n')
    run_result = run_cmd(execute_script, temp_dir)
    write_text(evidence / "outputs" / "native_stdout.txt", run_result.stdout)
    write_text(evidence / "outputs" / "native_stderr.txt", run_result.stderr)
    manifest["commands"]["native"] = {
        "script": str(execute_script), "command": f'"{exe}"', "exit_code": run_result.returncode
    }
    manifest["fixture_result"] = parse_fixture_output(run_result.stdout)
    for output in sorted(temp_dir.iterdir()):
        if output.is_file() and output.name not in {exe.name}:
            destination = evidence / "outputs" / output.name
            if destination.resolve() != output.resolve():
                shutil.copy2(output, destination)
    manifest["outputs"]["native_stdout"] = {
        "path": "outputs/native_stdout.txt", "sha256": sha256(evidence / "outputs" / "native_stdout.txt")
    }
    manifest["outputs"]["native_stderr"] = {
        "path": "outputs/native_stderr.txt", "sha256": sha256(evidence / "outputs" / "native_stderr.txt")
    }
    manifest["outputs"]["fixture_files"] = {}
    for path in sorted((evidence / "outputs").glob("*.bin")):
        manifest["outputs"]["fixture_files"][path.name] = {
            "sha256": sha256(path), "bytes": path.stat().st_size
        }
    for path in sorted((evidence / "inputs").rglob("*")):
        if path.is_file():
            manifest["outputs"].setdefault("input_snapshots", {})[str(path.relative_to(evidence))] = {
                "sha256": sha256(path), "bytes": path.stat().st_size
            }
    for path in sorted((evidence / "outputs").iterdir()):
        if path.is_file():
            manifest["outputs"].setdefault("output_hashes", {})[str(path.relative_to(evidence))] = {
                "sha256": sha256(path), "bytes": path.stat().st_size
            }
    manifest["commands"]["command_file_hashes"] = {}
    for path in sorted((evidence / "commands").iterdir()):
        if path.is_file():
            manifest["commands"]["command_file_hashes"][str(path.name)] = {
                "sha256": sha256(path), "bytes": path.stat().st_size
            }
    compiler_text = (evidence / "outputs" / "compile_stdout.txt").read_text(
        encoding="utf-8", errors="replace")
    compiler_version = re.search(r"Microsoft \(R\).*Compiler Version[^\r\n]*", compiler_text)
    manifest["compiler"] = {
        "version_line": compiler_version.group(0) if compiler_version else "not reported",
        "canonical_flags_file": "tools/build/msvc_flags.txt",
        "canonical_include_file": "tools/build/msvc_includes.txt",
        "environment_script": "tools/build/msvc_env.bat",
    }
    manifest["parameter_contract"] = {
        "sampler_prefix_bytes": 72,
        "tint_volume_caller_words": {
            "address_u_v_w": [2, 2, 2], "mag_min_mip": [1, 1, 0],
            "words_6_7_8_9_10": [1, 1, 13, 0, 1],
            "trailing_flags_0x40_to_0x44": [0, 0, 0, 1, 1],
        },
        "lod_bias": -0.5,
        "lod_bias_attribution": "Observed in CgsFont::CreateTextureState; used with Tint's volume LUT caller words to make the float mapping observable.",
        "raw_parameter_file": "outputs/texturestate_parameters_prefix.bin",
    }

    if run_result.returncode == 77:
        manifest["status"] = "native_setup_unsupported"
    elif run_result.returncode == 78:
        manifest["status"] = "native_setup_failure"
    elif manifest["fixture_result"].get("status") == "red_contract_failure" and run_result.returncode == 1:
        manifest["status"] = "red_contract_failure"
    elif manifest["fixture_result"].get("status") == "pass" and run_result.returncode == 0:
        manifest["status"] = "pass"
    else:
        manifest["status"] = "native_unclassified_failure"

    report_text = make_report(manifest, evidence)
    write_text(evidence / "REPORT.md", report_text)
    freeze_manifest(evidence, manifest)
    print(f"{manifest['status']}: {evidence}")
    print((evidence / "REPORT.md").read_text(encoding="utf-8"))
    return {"pass": 0, "red_contract_failure": 1, "native_setup_unsupported": 77,
            "native_setup_failure": 78, "native_unclassified_failure": 90}.get(manifest["status"], 90)


def make_report(manifest: dict, evidence: Path) -> str:
    result = manifest.get("fixture_result", {})
    lines = [
        "# TextureState producer fixture",
        "",
        f"- Frozen result: `{manifest.get('status', 'in progress')}`.",
        f"- Checkout: `{manifest.get('branch')}` at `{manifest.get('head')}`.",
        f"- Compile: exit `{manifest.get('commands', {}).get('compile', {}).get('exit_code', 'not run')}` with the canonical MSVC flags/includes.",
        f"- Native D3D9: exit `{manifest.get('commands', {}).get('native', {}).get('exit_code', 'not run')}`.",
        f"- Numeric result: `{result.get('checks', 'unavailable')}` checks, `{result.get('failures', 'unavailable')}` failures.",
        "",
        "The fixture links the production PC `TextureState::Initialize`, compiles the actual SDK `SamplerState::Initialize` source through the recorded adapter, and sends both generated runtime blocks through `WorldTextureFiltering.h`. The SDK-generated block is a positive control; the production state must match it, preserve its supplied resource/raster identities, and sample green at the volume LUT's green=1 boundary.",
        "",
        "The 72-byte parameter prefix uses the Tint volume LUT caller's U/V/W, filter, word, and trailing-flag bytes. The nonzero `-0.5` LOD bias comes from the existing Font caller and makes the float field observable; both callers and the exact prefix bytes are frozen in `inputs/` and `outputs/texturestate_parameters_prefix.bin`.",
        "",
        "A `red_contract_failure` means the executable completed D3D9 setup and the numeric boundary checks failed. `native_setup_unsupported` and `native_setup_failure` preserve their respective outcomes without counting them as a producer regression.",
        "",
        "Frozen command and output files are under `commands/` and `outputs/`; original ARTIST exports and source inputs are under `inputs/`. `manifest.json` records every input snapshot, command, and output SHA-256.",
        "",
        f"Evidence directory: `{evidence}`.",
        "",
    ]
    native_stdout = evidence / "outputs" / "native_stdout.txt"
    if native_stdout.is_file():
        lines += ["## Native output", "", "```text", native_stdout.read_text(encoding="utf-8", errors="replace").rstrip(), "```", ""]
    return "\n".join(lines)


def freeze_manifest(evidence: Path, manifest: dict) -> None:
    write_text(evidence / "manifest.json", json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
