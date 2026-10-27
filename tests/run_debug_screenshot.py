"""Hosted screenshot source-slice fixture; authoring this file does not run it.

The whole worker inherits the existing owned Windows Job. Compilation/execution
uses fxgs_common.compile_and_run with retained artifacts; no second compiler path.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys

TAG = "DebugScreenshot"
DATA = "tests/evidence/debug_screenshot"
BASELINE = "0117424f57852e7244a91c9664a6ac8a48c7b2b3"
RENDER = "src/GameSource/Graphics/BrnRendererModule.cpp"
HEADER = "src/GameSource/Graphics/BrnRendererModule.h"
WRITER = "src/GameSource/Graphics/BrnRendererModule_wBT_01.cpp"
DEVICE = "src/pc/gcm/renderengine/device.h"
FORMATTER = "src/GameShared/GameClasses/Core/CgsStringUtils.cpp"
NATIVE = "src/pc/gcm/renderengine/device.cpp"
DEBUG = "src/GameSource/Graphics/BrnRendererModuleDebugComponent.cpp"


def identity(path: Path) -> dict:
    raw = path.read_bytes(); lf = raw.replace(b"\r\n", b"\n")
    return {"raw_sha256": hashlib.sha256(raw).hexdigest(), "raw_bytes": len(raw),
            "lf_sha256": hashlib.sha256(lf).hexdigest(), "lf_bytes": len(lf)}


def text(path: Path) -> str:
    return path.read_bytes().replace(b"\r\n", b"\n").decode("utf-8-sig")


def digest(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def selected(common, root: Path) -> dict:
    render_source = text(root / RENDER)
    render = common.definition(render_source, "void BrnRendererModule::Render(")
    end = common.definition(render_source, "void BrnRendererModule::EndOfFrame(")
    handoff = common.definition(end, "if (mbUpdateThreadTakeScreenshot)")
    terminal_start = render.rfind("lRenderStage.Next(RENDER_PRESENT);")
    if terminal_start < 0: raise ValueError("normal production present terminal missing")
    terminal = render[terminal_start:render.rfind("}")].rstrip()
    # The complete actual debug2D if-expression, without translating its logic.
    dispatch = render.index("mIm2dDebugRenderBuffer.Dispatch(&mIm2dRenderer)")
    begin = render.rfind("if (", 0, dispatch)
    opening = render.index("(", begin); depth = 1; cursor = opening + 1
    while depth:
        if render[cursor] == "(": depth += 1
        elif render[cursor] == ")": depth -= 1
        cursor += 1
    condition = render[opening + 1:cursor - 1]
    after = common.code_only(render[cursor:dispatch]).strip()
    if after: raise ValueError("unexpected code between overlay predicate and real dispatch")
    compact = re.sub(r"\s+", "", common.code_only(render))
    monitor_gate = ("if(mbRenderHudImmediateMode&&(!mbDispatchThreadTakeScreenshot||mbCaptureOverlaysInScreenshot))"
                    "RenderThreeThreadMonitors(") in compact
    early = common.definition(render, "if (siWorldOnly == 1)")
    early_compact = re.sub(r"\s+", "", common.code_only(early))
    early_order = "renderengine::Device::ShowPixelBuffer();CgsDev::PerfMonGpu::StopProfiling();TakeScreenshot();return;" in early_compact
    gui = common.definition(render, "if (mbRenderHudImmediateMode)\n    {")
    gui_independent = "mbDispatchThreadTakeScreenshot" not in common.code_only(gui) and "mbCaptureOverlaysInScreenshot" not in common.code_only(gui)
    debug = re.sub(r"\s+", "", common.code_only(text(root / DEBUG)))
    request_binding = debug.count('RegisterVariable(&mpRendererModule->mbUpdateThreadTakeScreenshot,"Takescreenshot");') == 1
    overlay_binding = debug.count('RegisterVariable(&mpRendererModule->mbCaptureOverlaysInScreenshot,"Captureoverlaysinscreenshot");') == 1
    writer = common.definition(text(root / WRITER), "void BrnRendererModule::TakeScreenshot()")
    descriptor = common.definition(text(root / DEVICE), "struct LockedFrontBuffer")
    formatter = common.definition(text(root / FORMATTER), "void CgsCore::SPrintf(")
    native = text(root / NATIVE)
    native_lock = common.definition(native, "bool renderengine::Device::LockFrontBuffer(")
    native_unlock = common.definition(native, "void renderengine::Device::UnlockFrontBuffer(")
    header = text(root / HEADER)
    for member in ["mbUpdateThreadTakeScreenshot", "mbDispatchThreadTakeScreenshot", "mbCaptureOverlaysInScreenshot", "mbRenderHudImmediateMode"]:
        if not re.search(r"\bbool\s+" + member + r"\s*;", header): raise ValueError("actual bool declaration changed: " + member)
    if not re.search(r"\bu32\s+muScreenshotCounter\s*;", header): raise ValueError("actual counter declaration changed")
    if not re.search(r"\bKU_SCREENSHOT_TEXT_LENGTH\s*=\s*(32|0x20)\b", header): raise ValueError("actual caption length changed")
    return {"writer": writer, "descriptor": descriptor, "formatter": formatter, "handoff": handoff,
            "terminal": terminal, "overlay_condition": condition,
            "native_lock": native_lock, "native_unlock": native_unlock,
            "source_contracts": {"thread_square_gate": monitor_gate, "early_present_consumer_order": early_order,
                                 "ordinary_gui_independent_of_request": gui_independent,
                                 "actual_menu_request_binding": request_binding, "actual_menu_overlay_binding": overlay_binding}}


def compose(parts: dict, has_writer: bool) -> tuple[str, str]:
    types = "#define SCREENSHOT_HAS_PRODUCTION_WRITER " + str(int(has_writer)) + "\n"
    types += "namespace renderengine { class Device { public: static void ShowPixelBuffer();\n"
    if has_writer:
        types += parts["descriptor"] + ";\n"
        types += "static bool LockFrontBuffer(LockedFrontBuffer&); static void UnlockFrontBuffer(LockedFrontBuffer&);\n"
    types += "}; }\n"
    code = parts["formatter"] + "\n"
    if has_writer: code += parts["writer"] + "\n"
    # Only wrapper signatures/storage are fixture glue. These statement and
    # expression payloads are byte-bound production slices, including the old
    # terminal with no writer call; no dummy old TakeScreenshot is introduced.
    code += "void BrnRendererModule::FixtureHandoff() {\n" + parts["handoff"] + "\n}\n"
    code += "void BrnRendererModule::FixturePresent() {\n" + parts["terminal"] + "\n}\n"
    code += "bool BrnRendererModule::FixtureOverlayPermission() const { return (" + parts["overlay_condition"] + "); }\n"
    return code, types


def worker(root: Path, workflow: Path, output: Path) -> int:
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    os.environ["VSLANG"] = "1033"
    from fxgs_common import compile_and_run
    import fxgs_common as common
    if common.REPO.resolve() != root or common.WORKFLOW.resolve() != workflow:
        raise RuntimeError("shared compile helper resolves a different checkout")
    contract_path = root / DATA / "source_contract.json"
    legacy_path = root / DATA / "legacy0117424.json"
    plan_path = root / DATA / "test_plan.json"
    contract = json.loads(text(contract_path)); legacy = json.loads(text(legacy_path)); plan = json.loads(text(plan_path))
    if legacy["commit"] != BASELINE or legacy["writer_present"] is not False:
        raise ValueError("not the unchanged original request-only control")
    if identity(legacy_path)["lf_sha256"] != contract["legacy_snapshot_lf_sha256"]:
        raise ValueError("original source-slice payload changed")
    for relative, expected in contract["fixture_inputs"].items():
        if identity(root / relative)["lf_sha256"] != expected:
            raise ValueError("reviewed fixture input changed: " + relative)
    actual = selected(common, root)
    for name, expected in contract["candidate_definition_sha256"].items():
        if digest(actual[name]) != expected: raise ValueError("production slice changed: " + name)
    if not all(actual["source_contracts"].values()): raise ValueError("actual dispatch/overlay source contract failed")
    # The real formatter is shared and unchanged; the old producer/terminal and
    # overlay expression are immutable original fragments with original blob IDs.
    old = {**legacy["slices"], "formatter": actual["formatter"]}
    inputs = [RENDER, HEADER, WRITER, DEVICE, FORMATTER, NATIVE, DEBUG,
              "tests/fxgs_common.py", "tests/online_save_route_owned.py", "tests/DebugScreenshot.cpp",
              DATA + "/source_contract.json", DATA + "/legacy0117424.json", DATA + "/test_plan.json"]
    result = {"schema": 1, "fixture": TAG, "success": False, "executed": True,
              "scope": "production writer/formatter and bounded handoff/present/predicate slices; no native D3D/whole Render proof",
              "candidate_sha_metadata": os.environ.get("CANDIDATE_B5_SHA"), "baseline_commit": BASELINE,
              "inputs": {p: identity(root / p) for p in inputs}, "source_contracts": actual["source_contracts"], "variants": {}}
    for name, parts, has_writer in [("candidate",actual,True),("unchanged011742",old,False)]:
        code, types = compose(parts, has_writer)
        destination = output / name
        numeric = compile_and_run(root / "tests/DebugScreenshot.cpp", "debug_screenshot_extracted.inc", code, TAG,
                                  extra_files={"debug_screenshot_types.inc": types}, output_dir=destination)
        receipt_path = destination / "result.json"
        if numeric is None or not receipt_path.is_file():
            raise RuntimeError("control must compile and execute; missing numeric result: " + name)
        receipt = json.loads(text(receipt_path))
        stdout = text(destination / "run.stdout.txt")
        observed = re.findall(r"^CHECK (\S+) (PASS|FAIL)$", stdout, re.MULTILINE)
        ids = [item[0] for item in observed]; failures = [item[0] for item in observed if item[1] == "FAIL"]
        wanted = plan["variants"][name]
        valid = (ids == plan["ordered_check_ids"] and failures == wanted["failure_ids"]
                 and list(numeric) == [len(ids),len(failures)] and receipt["process_exit_code"] == wanted["exit_code"]
                 and "UNEXPECTED_FIXTURE_EXCEPTION" not in stdout)
        result["variants"][name] = {"valid": valid, "raw_checks": numeric[0], "raw_failures": numeric[1],
                                    "raw_exit_code": receipt["process_exit_code"], "failure_ids": failures,
                                    "receipt": identity(receipt_path), "generated_source_sha256": digest(code)}
        if not valid: raise RuntimeError("candidate/control check transcript differs: " + name)
    result["success"] = True
    write_json(output / "worker_result.json", result)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow-root",type=Path,required=True); parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--run",action="store_true"); parser.add_argument("--worker",action="store_true",help=argparse.SUPPRESS)
    args = parser.parse_args()
    if (not args.run or os.name != "nt" or os.environ.get("GITHUB_ACTIONS") != "true" or
        os.environ.get("RUNNER_OS") != "Windows" or os.environ.get("RUNNER_ARCH") != "X64" or
        os.environ.get("CANDIDATE_RUNNER_ENVIRONMENT") != "github-hosted"):
        parser.error("only the reviewed GitHub-hosted Windows x64 candidate job may run this fixture")
    root=Path(__file__).resolve().parents[1]; workflow=args.workflow_root.resolve(); output=args.out.resolve()
    artifacts=(workflow / "build/candidate/artifacts").resolve()
    if root != workflow / "b5-decomp" or not output.is_relative_to(artifacts) or output == artifacts:
        parser.error("dedicated artifact directory in the exact candidate checkout required")
    if args.worker:
        if (output / "worker_result.json").exists(): parser.error("worker receipt already exists")
        return worker(root,workflow,output)
    if output.exists() and any(output.iterdir()): parser.error("fresh/empty output directory required")
    output.mkdir(parents=True,exist_ok=True)
    from online_save_route_owned import run_owned
    command=[sys.executable,"-B",str(Path(__file__).resolve()),"--run","--worker","--workflow-root",str(workflow),"--out",str(output)]
    owned=run_owned(command,output,output / "owned_worker.log",300)
    good=(owned.get("exit_code")==0 and owned.get("timed_out") is False and owned.get("owned_job") is True and
          owned.get("transcript_hash_after_cleanup") is True and owned.get("cleanup",{}).get("complete") is True and
          owned.get("cleanup",{}).get("job_active_processes_after_cleanup")==0)
    path=output / "worker_result.json"
    observed=json.loads(text(path)) if path.is_file() else {}
    result={"schema":1,"fixture":TAG,"success":good and observed.get("success") is True,
            "owned_worker":owned,"worker_result":observed}
    write_json(output / "fixture_result.json",result)
    return 0 if result["success"] else 1


if __name__ == "__main__":
    try: raise SystemExit(main())
    except Exception as error:
        print("SCREENSHOT_FIXTURE_ERROR",error,file=sys.stderr); raise SystemExit(2)
