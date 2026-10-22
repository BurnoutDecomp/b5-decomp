"""Hosted startup IO lifecycle and selected PC GUI lock-ownership regression.

--base-only-postworld recreates the old effective inherited-base construction.
--legacy-gui-write-lock restores only the former calibration inner write lock.
Each control must execute numeric checks, not merely fail setup or compilation.
The full GuiModule core, VM, GameData payload and native ABI are outside this fixture.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path

from fxgs_common import Tree, code_only, compile_and_run, definition, report

TAG = "STARTUP_IO_OWNERSHIP"
NUMERIC_TOTAL = 62
INPUT = "src/GameShared/GameClasses/System/Input/"
MODULE = "src/GameShared/GameClasses/Module/"
GUI = "src/GameSource/Gui/BrnGuiModule.cpp"
FLOW = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev", help="Read production source/headers from this b5 revision")
    parser.add_argument("--output-dir", type=Path, help="Retain receipts in a new or empty directory")
    controls = parser.add_mutually_exclusive_group()
    controls.add_argument("--base-only-postworld", action="store_true")
    controls.add_argument("--legacy-gui-write-lock", action="store_true")
    args = parser.parse_args()
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    os.environ.pop("BRN_IOBUF_ZERO", None)
    tree = Tree(args.rev)
    inputs = {}
    missing = []

    def read(relative):
        if relative not in inputs:
            inputs[relative] = tree.read(relative)
        return inputs[relative]

    def body(relative, signature):
        try:
            return definition(read(relative), signature)
        except ValueError:
            missing.append(relative + ": " + signature)
            return ""

    postworld_original = body(INPUT + "CgsInputModuleIO.cpp", "void PostWorldInputBuffer::Construct()")
    postworld = postworld_original
    if args.base_only_postworld and postworld:
        postworld = postworld[:postworld.index("{")] + "{\n    CgsModule::IOBuffer::Construct();\n}"
    calibration_original = body(GUI, "void GuiModule::UpdateCalibrationScreen(")
    calibration = calibration_original
    if args.legacy_gui_write_lock:
        start = "mColourCalibrationScreen.Update("
        end = "&mGuiOutQueue, mGuiConfig.mpTextureAllocator);"
        if calibration.count(start) != 1 or calibration.count(end) != 1:
            missing.append("unique production calibration call for legacy lock control")
        else:
            calibration = calibration.replace(start, "lpGameDataInput->LockForWrite();\n            " + start, 1)
            calibration = calibration.replace(end, end + "\n            lpGameDataInput->UnlockForWrite();", 1)
    wrapper = body(GUI, "void GuiModule::Update()")
    input_methods = [postworld] + [body(INPUT + "CgsInputModuleIO.cpp", signature) for signature in (
        "void PostWorldInputBuffer::Destruct()",
        "const PostWorldInputBuffer::BindRequestQueue* PostWorldInputBuffer::GetBindRequestQueue() const",
        "const PostWorldInputBuffer::UnBindRequestQueue* PostWorldInputBuffer::GetUnBindRequestQueue() const",
        "const PostWorldInputBuffer::PadMappingQueue* PostWorldInputBuffer::GetPadMappingQueue() const",
        "void PostWorldInputBuffer::PostBindRequest(", "void PostWorldInputBuffer::PostUnbindRequest(",
        "void PostWorldInputBuffer::PostMappingRequest(",
        "const CgsInput::Device::WheelFFSpring* PostWorldInputBuffer::GetWheelFFSpring() const",
        "void PostWorldInputBuffer::SetWheelFFSpring(",
    )]
    module_methods = [body(MODULE + "CgsIOBuffer.cpp", signature) for signature in (
        "void IOBuffer::Construct()", "void IOBuffer::Destruct()",
        "void IOBuffer::LockForWrite()", "void IOBuffer::UnlockForWrite()",
        "void IOBuffer::LockForRead() const", "void IOBuffer::UnlockForRead() const",
    )] + [body(MODULE + "CgsIOBufferStack.cpp", signature) for signature in (
        "bool IOBufferHostZeroFillEnabled()", "void IOBufferStack::Construct(",
        "bool IOBufferStack::Prepare(", "void* IOBufferStack::Alloc(", "bool IOBufferStack::Free(",
    )]
    shadow = {relative: read(relative) for relative in (
        "src/types.hpp", "src/GameShared/GameClasses/Core/CgsAssert.h",
        "src/GameShared/GameClasses/Containers/CgsFlagSet.h",
        MODULE + "CgsIOBuffer.h", MODULE + "CgsIOBufferStack.h", MODULE + "CgsModuleIOHelper.h",
        MODULE + "CgsEventQueue.h", MODULE + "CgsBaseEventQueue.h",
        INPUT + "CgsInputModuleIO.h", INPUT + "CgsInputTypes.h",
        "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h",
        "src/GameShared/GameClasses/System/Timer/CgsTime.h",
    )}
    missing.extend(relative + ": missing header" for relative, text in shadow.items() if not text)
    initial = code_only(body(FLOW, "void MainGameFlowStateInitialLoadingScreen::Update()"))
    scripted = code_only(body(FLOW, "void LoadingScriptedState::Update()"))
    core = code_only(body(GUI, "void GuiModule::Update(BrnResource::GameDataIO::InputBuffer*"))
    header = code_only(read(INPUT + "CgsInputModuleIO.h"))
    constructor = code_only(postworld_original)
    destruction = code_only(input_methods[1])
    normal_wrapper = code_only(wrapper)
    calibration_code = code_only(calibration_original)
    stack_header = code_only(read(MODULE + "CgsIOBufferStack.h"))
    helper_header = code_only(read(MODULE + "CgsModuleIOHelper.h"))
    game_module = code_only(read("src/GameSource/Game/BrnGameModule.cpp"))

    def in_order(text, pieces):
        cursor = 0
        for piece in pieces:
            found = text.find(piece, cursor)
            if found < 0:
                return False
            cursor = found + len(piece)
        return True

    wiring = [
        ("derived PostWorld lifecycle is declared and recovered", (
            not missing and "void Construct();" in header and "void Destruct();" in header and
            in_order(destruction, ["mPadMappingQueue.Destruct();", "mUnBindRequestQueue.Destruct();",
                                   "mBindRequestQueue.Destruct();", "CgsModule::IOBuffer::Destruct();"]))),
        ("PostWorld constructs original 8/8/4 owned queues and spring", (
            in_order(constructor, ["CgsModule::IOBuffer::Construct();", "mBindRequestQueue.Construct();",
                                   "mUnBindRequestQueue.Construct();", "mPadMappingQueue.Construct();",
                                   "mWheelFFSpring.mfSpringCoefficient = 0.0f;", "mWheelFFSpring.mfSpringSaturation = 0.0f;"]) and
            bool(re.search(r"EventQueue<BaseInputEvent,\s*8> BindRequestQueue", header)) and
            bool(re.search(r"EventQueue<BaseInputEvent,\s*8> UnBindRequestQueue", header)) and
            bool(re.search(r"EventQueue<PadMapping,\s*4> PadMappingQueue", header)))),
        ("real IOHelper and stack dispatch derived construction and retirement", (
            "(*lpOutBuffer)->Construct();" in stack_header and "(*lpInOutBuffer)->Destruct();" in stack_header and
            "mpStack->CreateIOBuffer(&mpBuffer, lpcName)" in helper_header and
            "mpStack->DestroyIOBuffer(&mpBuffer)" in helper_header)),
        ("normal GUI wrapper owns locks; calibration/core borrow the supplied pair", (
            in_order(normal_wrapper, ["lpGameDataInput->LockForWrite();", "lpGameDataOutput->LockForRead();",
                                      "Update(lpGameDataInput, lpGameDataOutput);", "lpGameDataOutput->UnlockForRead();",
                                      "lpGameDataInput->UnlockForWrite();"]) and
            "LockFor" not in calibration_code and "UnlockFor" not in calibration_code and
            "GetScriptedLoadGameData" not in calibration_code and "GetScriptedLoadGameData" not in core and
            "lpGameDataInput->Lock" not in core and "lpGameDataInput->Unlock" not in core and
            "UpdateCalibrationScreen(lpGameDataInput, lpGameDataOutput);" in core)),
        ("both loading callers pass their already-owned actual GameData pair", (
            in_order(initial, ["lpGameDataInput->LockForWrite();", "lrGui.Update(lpGameDataInput, lpGameDataOutput);",
                               "lpGameDataInput->UnlockForWrite();"]) and
            in_order(scripted, ["s_GameDataInput.LockForWrite();", "lrGui.Update(&s_GameDataInput, &s_GameDataOutput);",
                                "s_GameDataInput.UnlockForWrite();"]))),
        ("normal GameMain retains the selected no-argument GUI entry", "mGuiModule.Update();" in game_module),
    ]

    numeric = None
    if missing:
        print("Missing production definitions/headers:\n" + "\n".join(missing))
    else:
        generated = ("namespace CgsModule {\n" + "\n\n".join(module_methods) + "\n}\n" +
                     "namespace CgsInput { namespace InputIO {\n" + "\n\n".join(input_methods) + "\n} }\n" +
                     "namespace BrnGui {\n" + wrapper + "\n" + calibration + "\n}\n")
        numeric = compile_and_run(Path(__file__).with_name("StartupIOOwnership.cpp"),
                                  "startup_io_ownership.inc", generated, TAG,
                                  shadow=shadow, output_dir=args.output_dir)
    receipt_matches = args.output_dir is None
    if args.output_dir is not None:
        receipt_path = args.output_dir / "result.json"
        if receipt_path.exists():
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            expected_process = 0 if numeric is not None and numeric[1] == 0 else 1
            receipt_matches = numeric is not None and receipt.get("numeric") == {
                "checks": numeric[0], "failures": numeric[1]
            } and receipt.get("process_exit_code") == expected_process
            receipt.update({
                "fixture_revision": args.rev or "working-tree",
                "negative_control": "base-only-postworld" if args.base_only_postworld else
                                    "legacy-gui-write-lock" if args.legacy_gui_write_lock else None,
                "environment": {"BRN_IOBUF_ZERO": "unset", "NoDefaultCurrentDirectoryInExePath": "unset"},
                "production_inputs": [{"path": relative,
                                       "sha256_utf8_text": hashlib.sha256(text.encode("utf-8")).hexdigest()}
                                      for relative, text in inputs.items()],
            })
            receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    wiring.extend([
        ("exactly 62 numeric checks executed", numeric is not None and numeric[0] == NUMERIC_TOTAL),
        ("retained process status agrees with numeric result", receipt_matches),
    ])
    print(f"{TAG}_NUMERIC_CHECKS={numeric[0] if numeric is not None else 0}")
    print(f"{TAG}_NUMERIC_FAILURES={numeric[1] if numeric is not None else NUMERIC_TOTAL}")
    return report("run_startup_io_ownership", wiring, numeric, NUMERIC_TOTAL)


if __name__ == "__main__":
    raise SystemExit(main())
