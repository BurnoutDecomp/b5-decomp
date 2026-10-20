"""Hosted regression for production CgsGui animation initialization and updates.

Run after applying the source repair. --rev reads a complete b5 revision.
--omit-channel-construct removes only the real template's Construct call from
the extracted Animator body; both modes poison its template identically.
--output-dir retains generated bodies/headers, commands, stdout and result.json.
No APT VM, movie, online backend or gameplay behavior is simulated here.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path

from fxgs_common import Tree, code_only, compile_and_run, definition, report

TAG = "GUI_ANIMATION_INITIALIZATION"
NUMERIC_TOTAL = 68
APT = "src/GameShared/GameClasses/Gui/View/AptInterface/"
COMPONENTS = "src/GameSource/Gui/Flow/Shared/Components/"
STATES = "src/GameSource/Gui/Flow/Screen/States/"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev", help="Read production owners and headers from this b5 revision")
    parser.add_argument("--omit-channel-construct", action="store_true",
                        help="Negative control: omit only template channel Construct")
    parser.add_argument("--output-dir", type=Path,
                        help="Retain receipts in a new or empty directory")
    args = parser.parse_args()
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    tree = Tree(args.rev)
    inputs = {}

    def read(relative):
        if relative not in inputs:
            inputs[relative] = tree.read(relative)
        return inputs[relative]

    missing = []

    def body(relative, signature):
        try:
            return definition(read(relative), signature)
        except ValueError:
            missing.append(relative + ": " + signature)
            return ""

    animator = APT + "CgsAptAnimator.cpp"
    channel = APT + "CgsAptAnimChannel.cpp"
    interpolator = APT + "CgsAptInterpolator.cpp"
    construct = body(animator, "void Animator::Construct(")
    original_construct = code_only(construct)
    declarations = "AnimChannel lDefaultChannel;"
    construct_call = "lDefaultChannel.Construct();"
    injection_ready = construct.count(declarations) == 1 and construct.count(construct_call) == 1
    if injection_ready:
        construct = construct.replace(declarations,
                                      declarations + "\n        FixturePoisonDefaultChannel(lDefaultChannel);", 1)
        if args.omit_channel_construct:
            construct = construct.replace(construct_call,
                                          "/* negative control: channel Construct omitted */", 1)
    else:
        missing.append("Animator::Construct: unique real template declaration/Construct hook")

    methods = [
        body(interpolator, "void Interpolator::Construct()"),
        body(interpolator, "f32 Interpolator::GetCurrentValue() const"),
        body(APT + "CgsAptInterpolator_wS34_00.cpp", "void Interpolator::SetInterpolator("),
        body(APT + "CgsAptInterpolator_wS34_00.cpp", "f32 Interpolator::Update("),
        body(APT + "CgsAptInterpolator_wS34_00.cpp", "void Interpolator::Reset()"),
        body(channel, "AnimChannel::AnimChannel()"),
        body(channel, "void AnimChannel::Construct()"),
        body(channel, "Interpolator* AnimChannel::GetInterpolator("),
        body(channel, "void AnimChannel::SetAnimation("),
        body(channel, "void AnimChannel::Update("),
        body(APT + "CgsAptAnimChannel_wS34_00.cpp", "void AnimChannel::Stop()"),
        body(animator, "const char Animator::maDefaultChannelTypeNames[") + ";",
        construct,
        body(animator, "void Animator::Destruct()"),
        body(animator, "void Animator::Update("),
        body(animator, "void Animator::SetAnimationChannel("),
        body(animator, "void Animator::SetAnimation("),
        body(animator, "void Animator::SetAptValues()"),
        body(APT + "CgsAptAnimator_wS34_00.cpp", "void Animator::SetAptValue("),
        body(APT + "CgsAptAnimData.cpp", "void AnimChannelData::Construct()"),
        body(APT + "CgsAptAnimData.cpp", "void AnimChannelData::SetData("),
        body(APT + "CgsAptAnimDataAnimData.cpp", "AnimData::AnimData()"),
        body(APT + "CgsAptAnimDataAnimData.cpp", "AnimData::AnimatorChannel& AnimData::AnimatorChan("),
        body(APT + "CgsAptAnimDataAnimData.cpp", "const AnimData::AnimatorChannel& AnimData::AnimatorChan("),
        body(APT + "CgsAptAnimData_wS34_00.cpp", "void AnimData::Construct()"),
        body(APT + "CgsAptAnimData_wS34_00.cpp", "AnimChannelData* AnimData::GetChannelData("),
        body(APT + "CgsAptAnimData_wS34_00.cpp", "void AnimData::AddAnimationChannel("),
    ]

    shadow = {relative: read(relative) for relative in (
        "src/types.hpp",
        "src/GameShared/GameClasses/Core/CgsAssert.h",
        "src/GameShared/GameClasses/Containers/CgsArray.h",
        APT + "CgsAptAnimator.h", APT + "CgsAptAnimChannel.h",
        APT + "CgsAptInterpolator.h", APT + "CgsAptAnimData.h",
    )}
    # The fixture supplies only this boundary's recording API before including
    # Animator.h. It does not replace any animation owner, body or Array method.
    shadow[APT + "CgsAptObjectController.h"] = (
        "#pragma once\n// APT VM boundary supplied by GuiAnimationInitialization.cpp.\n")
    missing.extend(relative + ": missing header" for relative, text in shadow.items() if not text)

    online = code_only(body(STATES + "BrnOnlineGameOptions_wI_05.cpp", "void OnlineGameOptions::OnEnter()"))
    help_bar = code_only(body(COMPONENTS + "BrnHelpBar.cpp", "void HelpBar::Construct("))
    brn_animator = code_only(body(COMPONENTS + "BrnAnimator.cpp", "void Animator::Construct("))
    getter_source = code_only(read(interpolator))
    getter_header = code_only(read(APT + "CgsAptInterpolator.h"))
    wiring = [
        ("online options binds its original help-bar arguments", bool(re.search(
            r"mHelpBar\.Construct\(KAC_HELP_BAR_COMPONENT,\s*KI_MAX_HELP_BAR_ITEMS,\s*mpStateInterface,\s*0\)", online))),
        ("help-bar binds each animator to its corresponding item", bool(re.search(
            r"maAnimators\[liItem\]\.Construct\(&maItems\[liItem\]\)", help_bar))),
        ("BrnGui animator requests all six default CgsGui channels", bool(re.search(
            r"CgsGui::Animator::Construct\(lpTarget,\s*CgsGui::Animator::maDefaultChannelTypeNames,\s*"
            r"CgsGui::AnimData::ANIMATOR_CHANNEL_MAX\)", brn_animator))),
        ("canonical Interpolator owns the real virtual getter", (
            '#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptInterpolator.h"' in getter_source and
            re.search(r"\b(?:class|struct)\s+Interpolator\b", getter_source) is None and
            "virtual f32 GetCurrentValue() const;" in getter_header and
            "return mfCurrentValue;" in getter_source)),
        ("all owners extracted; real template Construct precedes Append", not missing and bool(re.search(
            r"AnimChannel lDefaultChannel;\s*lDefaultChannel\.Construct\(\);[\s\S]*"
            r"maAnimChannels\.Append\(lDefaultChannel\)", original_construct))),
    ]

    numeric = None
    if missing:
        print("Missing production definitions or headers:\n" + "\n".join(missing))
    else:
        generated = "namespace CgsGui {\n" + "\n\n".join(methods) + "\n}\n"
        numeric = compile_and_run(Path(__file__).with_name("GuiAnimationInitialization.cpp"),
                                  "gui_animation_initialization.inc", generated, TAG,
                                  shadow=shadow, output_dir=args.output_dir)

    count_matches = numeric is not None and numeric[0] == NUMERIC_TOTAL
    receipt_matches = args.output_dir is None
    if args.output_dir is not None:
        receipt_path = args.output_dir / "result.json"
        if receipt_path.exists():
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            expected_process = 0 if numeric is not None and numeric[1] == 0 else 1
            receipt_matches = (numeric is not None and receipt.get("numeric") == {
                "checks": numeric[0], "failures": numeric[1]
            } and receipt.get("process_exit_code") == expected_process)
            receipt.update({
                "fixture_revision": args.rev or "working-tree",
                "negative_control": "omit-channel-construct" if args.omit_channel_construct else None,
                "production_inputs": [{"path": relative,
                                       "sha256_utf8_text": hashlib.sha256(text.encode("utf-8")).hexdigest()}
                                      for relative, text in inputs.items()],
            })
            receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    wiring.extend([
        ("exactly 68 numeric checks executed", count_matches),
        ("retained process status agrees with numeric result", receipt_matches),
    ])
    print(f"{TAG}_NUMERIC_CHECKS={numeric[0] if numeric is not None else 0}")
    print(f"{TAG}_NUMERIC_FAILURES={numeric[1] if numeric is not None else NUMERIC_TOTAL}")
    return report("run_gui_animation_initialization", wiring, numeric, NUMERIC_TOTAL)


if __name__ == "__main__":
    raise SystemExit(main())
