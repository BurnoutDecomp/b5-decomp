"""Focused production Pool create/host-layout/deferred-assert fixtures.

Preserve receipts with --output-dir. --negative restores one reviewed defect in
the extracted production body; each control must fail observable checks.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from fxgs_common import definition

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = next(path for path in REPO.parents if (path / "tools/build/msvc_env.bat").is_file())
BASE = "src/GameShared/GameClasses/"
RESOURCE = BASE + "System/Resource/"


def read(relative):
    return (REPO / relative).read_text(encoding="utf-8-sig")


def extract(source, signatures):
    return "\n".join(definition(source, signature) for signature in signatures)


def production(mode, negative):
    module = read(RESOURCE + "CgsResourcePoolModule.cpp")
    driver = read(RESOURCE + "CgsPoolModule.cpp")
    pool = read(RESOURCE + "CgsResourcePool.cpp")
    manager = read(BASE + "Development/AssertSystem/CgsAssertManager.cpp")
    result = "namespace CgsDev { namespace Assert {\n" + definition(manager, "Manager::Manager()") + "\n} }\n"
    if mode == "protocol":
        helpers = module[module.index("    namespace\n"):module.index("    // @ 0x828D80E8")]
        code = extract(module, ["s32 PoolModule::GetPoolIndex(", "void PoolModule::CreatePool(",
            "void PoolModule::ProcessReceiverQueue(PoolIO::OutputBuffer*", "void PoolModule::ProcessInputBuffer(const PoolIO::InputBuffer*",
            "void PoolModule::SendCreatePoolMemoryRequest(const Events::CreatePoolRequest*",
            "void PoolModule::DoCreatePoolRequest(const CgsMemory::MemoryIO::CreateResourceResponse*"])
        if negative == "tag10":
            start = code.index("            else if (liId == 10)")
            end = code.index("            else if (liId == 11)", start)
            code = code[:start] + code[end:]
        if negative == "event-sentinel":
            assert code.count("while (lpEvent != nullptr)") == 2
            code = code.replace("while (lpEvent != nullptr)", "while (liId != -1 && lpEvent != nullptr)")
        code += "\n" + extract(driver, ["void PoolModule::ConvertPoolRequestOptions(",
            "void PoolModule::GetRequiredResourceDescriptor(", "void PoolModule::DoDeletePoolRequest(",
            "void PoolModule::DoUnacquireResourceRequest("])
        # A small scheduling fixture owns the real typed fields. Only the owner qualification changes;
        # bodies, signatures, containers and canonical request/resource types remain production code.
        code = code.replace("PoolModule::", "PoolProtocolFixture::")
        result += "namespace CgsResource {\n" + helpers + code + "\n" + definition(pool, "s32  Pool::GetId(") + "\n}\n"
    else:
        layout = pool[pool.index("        struct PoolManagementStorageLayout"):pool.index("    // ---- lookup")]
        # layout already contains the anonymous namespace's closing brace.
        result += "namespace CgsResource { namespace {\n" + definition(pool, "u32 GetManagementHashLength(") + "\n" + layout
        result += extract(pool, ["void Pool::Construct(", "u32 Pool::GetOverheadMemoryRequired(",
            "void Pool::InitPool(", "void Pool::InitManagementData(", "const s16* Pool::CreateBatchEntrySlots("])
        result += "\n}\n"
        if negative == "batch-count":
            result = result.replace("sizeof(s16) * luMaxResources", "sizeof(s16) * 2u * luMaxResources", 1)
            result = result.replace("Malloc(sizeof(s16) * luMax)", "Malloc(sizeof(s16) * 2u * luMax)", 1)
    return result


def run(mode, output, negative):
    output.mkdir(parents=True, exist_ok=True)
    generated = output / "PoolCreateProduction.inc"
    generated.write_text(production(mode, negative), encoding="utf-8")
    flags_file = WORKFLOW / "tools/build/msvc_flags.txt"
    includes_file = WORKFLOW / "tools/build/msvc_includes.txt"
    flags = [line.strip() for line in flags_file.read_text().splitlines() if line.strip() and not line.startswith("#")]
    includes = [line.strip() for line in includes_file.read_text().splitlines() if line.strip() and not line.startswith("#")]
    mapped = [REPO / Path(path.replace("\\", "/")).relative_to("b5-decomp") for path in includes]
    sources = [BASE + "Core/CgsAssert.cpp", BASE + "Development/CgsStrStream.cpp",
        BASE + "Development/StackUnpick/CgsStackUnpick.cpp", "vendor/renderware/src/rw/core/debug/DebugCriticalSection.cpp",
        "vendor/renderware/src/rw/BaseResourceDescriptor.cpp"]
    if mode == "protocol":
        sources += [BASE + "Module/CgsIOBuffer.cpp", BASE + "Module/CgsBaseEventReceiverQueue.cpp",
            RESOURCE + "CgsPoolModuleIO_OutputBuffer.cpp", RESOURCE + "CgsSmallResource.cpp"]
    else:
        sources += [BASE + "Memory/CgsLinearMalloc.cpp", RESOURCE + "CgsResourceHeap.cpp"]
    if negative == "shared-buffer":
        original = read(BASE + "Core/CgsAssert.cpp")
        restored = original.replace("gbDeferredAssertPC ? macBuffer : gpcMessageBuffer", "gpcMessageBuffer")
        restored = restored.replace("gbDeferredAssertPC ? KI_MESSAGEBUFFERSIZE : giMessageBufferSize", "giMessageBufferSize")
        assert restored != original
        (output / "CgsAssertSharedBuffer.cpp").write_text(restored, encoding="utf-8")
        sources[0] = str(output / "CgsAssertSharedBuffer.cpp")
    response = output / "fixture.rsp"
    options = flags + ["/D_ALLOW_KEYWORD_MACROS=1", "/Dprivate=public", "/Dprotected=public", "/Gw",
        f'/DPOOL_LAYOUT={1 if mode == "layout" else 0}', f'/I"{output}"']
    options += [f'/I"{path}"' for path in mapped]
    options += [f'"{Path(__file__).with_name("PoolCreateProtocol.cpp")}"']
    options += [f'"{Path(source) if Path(source).is_absolute() else REPO / source}"' for source in sources]
    options += ['/Fe:PoolCreateProtocol.exe']
    response.write_text("\n".join(options) + "\n", encoding="utf-8")
    script = output / "compile.cmd"
    script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
        + '" >nul 2>&1\nif errorlevel 1 exit /b 200\ncl @"' + str(response) + '" /link /OPT:REF user32.lib\nexit /b %ERRORLEVEL%\n',
        encoding="utf-8", newline="\r\n")
    inputs = [Path(__file__), Path(__file__).with_name("PoolCreateProtocol.cpp"), generated, flags_file, includes_file]
    inputs += [Path(source) if Path(source).is_absolute() else REPO / source for source in sources]
    (output / "input_manifest.json").write_text(json.dumps([
        {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()} for path in inputs
    ], indent=2) + "\n", encoding="utf-8")
    env = os.environ.copy()
    env.pop("NoDefaultCurrentDirectoryInExePath", None)
    compile_result = subprocess.run(["cmd", "/c", str(script)], cwd=output, env=env, capture_output=True, timeout=120)
    (output / "compile.stdout.txt").write_bytes(compile_result.stdout)
    (output / "compile.stderr.txt").write_bytes(compile_result.stderr)
    (output / "compile.exit_code.txt").write_text(str(compile_result.returncode) + "\n")
    if compile_result.returncode:
        print(compile_result.stdout.decode("utf-8", errors="replace"))
        print(compile_result.stderr.decode("utf-8", errors="replace"))
        return {"mode": mode, "compiled": False, "exit_code": compile_result.returncode}
    executed = subprocess.run([str(output / "PoolCreateProtocol.exe")], cwd=output, env=env, capture_output=True, timeout=30)
    (output / "run.stdout.txt").write_bytes(executed.stdout)
    (output / "run.stderr.txt").write_bytes(executed.stderr)
    (output / "run.exit_code.txt").write_text(str(executed.returncode) + "\n")
    print(executed.stdout.decode("utf-8", errors="replace"))
    if executed.stderr: print(executed.stderr.decode("utf-8", errors="replace"))
    return {"mode": mode, "compiled": True, "exit_code": executed.returncode}


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--negative", choices=["tag10", "event-sentinel", "batch-count", "shared-buffer"])
    args = parser.parse_args()
    temporary = None
    if args.output_dir is None:
        temporary = tempfile.TemporaryDirectory(prefix="pool_create_")
        output = Path(temporary.name)
    else:
        output = args.output_dir.resolve()
        if output.exists() and any(output.iterdir()): raise SystemExit("Refusing to overwrite existing receipts: " + str(output))
    modes = ["layout"] if args.negative == "batch-count" else ["protocol"] if args.negative else ["protocol", "layout"]
    results = [run(mode, output / mode, args.negative) for mode in modes]
    result = {"negative_control": args.negative, "results": results,
        "source_sha256": {path: hashlib.sha256((REPO / path).read_bytes()).hexdigest() for path in [
            BASE + "Core/CgsAssert.h", BASE + "Core/CgsAssert.cpp", RESOURCE + "CgsResourcePoolModule.cpp", RESOURCE + "CgsResourcePool.cpp"]}}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    failed = any(not item["compiled"] or item["exit_code"] != 0 for item in results)
    print("PoolCreateProtocol verdict:", "FAIL" if failed else "PASS", "negative:", args.negative or "none")
    if temporary: temporary.cleanup()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
