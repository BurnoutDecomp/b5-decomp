"""FX-TAILS-B (crash parity 2026-09-24, item 6): the pass-by chain -- Slope::GetValue @0x826897F0,
PassbyEffect (Prepare / Attach / UpdateParams / ProcessUpdate / Detach and helpers),
PassbyState (Attach / UpdateParams) and PassbyStateManager (Prepare / Release / UpdateParams /
UpdateDynamicPropBys / DynamicPropByCache::Insert).

Before the fix none of these had a body: PassbyStateManager::Prepare returned true at once with no
states, UpdateParams was an empty stub, PassbyEffect was a two-member shell, and Slope::GetValue
(f32, ECurveType) and ~Slope were declared only. Every pass-by the AI cars, the player's tumbling
car and the flung props posted was dropped each frame -- nothing was ever voiced.

Numeric: tests/FxTailsBPassby.cpp compiles the PRODUCTION bodies (and their constants) against
recording fixtures; CgsSoundUtils.h, passbybin.h and BrnCommonTypes.h are the real headers. --rev
reads a b5 revision (the RED side: <fix>~1, where the bodies are absent and every numeric check
counts as failed).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_passby.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report
from run_fxvoicepool_curve import optional, LAST_ELEMENT, GET_OUTPUT, TABLE, READ_HELPER, UTILS_CPP

EFFECT_CPP = "src/GameSource/Sound/Passby/BrnPassbyEffect.cpp"
STATE_CPP = "src/GameSource/Sound/Passby/BrnPassbyState.cpp"
MANAGER_CPP = "src/GameSource/Sound/Passby/BrnPassbyStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Passby/BrnPassbyStateManager.h"
NUMERIC_CHECKS = 98

SLOPE_CTOR = "Slope::Slope(const SlopeParams& params)"
SLOPE_GET_VALUE = "f32 Slope::GetValue(f32 lfInput, Curve::ECurveType leCurve) const"

EFFECT_BODIES = (
    "s32 PassbyEffect::GetController(",
    "void PassbyEffect::AttachController(",
    "bool PassbyEffect::Prepare(",
    "const PassbyStateManager::Passby& PassbyEffect::GetPassbyData() const",
    "bool PassbyEffect::Attach()",
    "u32 PassbyEffect::ChooseSampleId(",
    "bool PassbyEffect::UpdatePosition()",
    "f32 PassbyEffect::GetRelativeVelocityMag() const",
    "void PassbyEffect::UpdateParams(",
    "void PassbyEffect::ProcessUpdate()",
    "bool PassbyEffect::Detach()",
)
STATE_BODIES = (
    "void PassbyState::Attach(void* apvAttachment)",
    "void PassbyState::UpdateParams(f32 afDeltaTime)",
)
MANAGER_BODIES = (
    "void PassbyStateManager::DynamicPropByCache::Update(",
    "PassbyStateManager::DynamicPropByCache::Item*\nPassbyStateManager::DynamicPropByCache::Find(",
    "PassbyStateManager::DynamicPropByCache::Item*\nPassbyStateManager::DynamicPropByCache::Insert(",
    "PassbyStateManager::Passby::Passby(\n        Vector3 lStaticPos,",
    "f32 DistanceBetween(",
    "void PassbyStateManager::UpdateDynamicPropBys(",
    "bool PassbyStateManager::Prepare()",
    "bool PassbyStateManager::Release()",
    "void PassbyStateManager::UpdateParams(",
)


def normalise(text):
    return text.replace("\r\n", "\n")


def statement(source, pattern, what):
    """One `const ... ;` statement (possibly spanning lines) matched by `pattern`."""
    match = re.search(pattern, code_only(source), re.S)
    if match is None:
        raise ValueError("constant absent: " + what)
    return match.group(0)


def effect_constants(source):
    names = ("KF_MIN_CAR_SPEED", "KF_MAX_CAR_SPEED", "KF_MIXER_AZIMUTH_TO_DEGREES", "KF_MIXER_Q12_TO_PITCH",
             "KF_COLLISION_SPEED_MAX", "KF_COLLISION_SPEED_SCALE", "KF_COLLISION_PITCH_RANGE",
             "KF_COLLISION_PITCH_MIN")
    pieces = [statement(source, r"const f32 " + name + r"\s*=\s*[^;]*;", name) for name in names]
    pieces.append(statement(source, r"const s32 KAI_PASSBY_DUCKING_ARRAY\[[^\]]*\]\s*=\s*\{[^}]*\};",
                            "KAI_PASSBY_DUCKING_ARRAY"))
    return "\n".join(pieces)


def wiring(tree):
    effect = normalise(tree.read(EFFECT_CPP))
    manager = normalise(tree.read(MANAGER_CPP))
    utils = normalise(tree.read(UTILS_CPP))
    prepare = body_or_empty(manager, "bool PassbyStateManager::Prepare()")
    update = body_or_empty(manager, "void PassbyStateManager::UpdateParams(")
    return [
        ("PassbyEffect is registered with ObjectID 0x40000 (sTypeInfo @0x82F2F98C, CRT 0x82C633B4)",
         re.search(r"ClassTypeInfo<CgsSound::Logic::EffectObject>\s+sTypeInfo\(\s*0x40000\s*,\s*\"PassbyEffect\"",
                   code_only(effect)) is not None
         and "EffectObject::AddToClassTypeInfoArray(PassbyEffect::GetStaticTypeInfo())" in code_only(effect)),
        ("Passby3DControl is registered with ObjectID 0x40000 (sTypeInfo @0x82F2F97C, CRT 0x82C633A4)",
         re.search(r"ClassTypeInfo<CgsSound::Logic::EffectControl>\s+sTypeInfo\(\s*0x40000\s*,\s*\"Passby3DControl\"",
                   code_only(effect)) is not None
         and "EffectControl::AddToClassTypeInfoArray(Passby3DControl::GetStaticTypeInfo())" in code_only(effect)),
        ("PassbyStateManager::Prepare runs the console's state machine (LoadAsset, the bank, PrepareStates(1, 8, 0)) "
         "instead of returning true at once",
         "PassbyAsset.bundle" in prepare and re.search(r"PrepareStates\(\s*1\s*,\s*KU_NUMBER_OF_PASSBY_STATES\s*,\s*0\s*\)",
                                                          prepare) is not None
         and re.match(r"[^{]*\{\s*return\s+true\s*;", prepare) is None),
        ("PassbyStateManager::UpdateParams dispatches the posts to free states and empties them",
         "GetFreeState" in update and "->Attach(" in update and re.search(r"muPostedPassbyCount\s*=\s*0", update) is not None
         and "UpdateDynamicPropBys" in update),
        ("Slope has a destructor body (the declaration alone made any Slope object unlinkable)",
         re.search(r"Slope::~Slope\(\)\s*\{", code_only(utils)) is not None),
    ]


def numeric(tree):
    utils = normalise(tree.read(UTILS_CPP))
    effect = normalise(tree.read(EFFECT_CPP))
    state = normalise(tree.read(STATE_CPP))
    manager = normalise(tree.read(MANAGER_CPP))
    header = normalise(tree.read(MANAGER_H))
    try:
        curve_pieces = []
        constant = LAST_ELEMENT.search(code_only(utils))
        if constant:
            curve_pieces.append(constant.group(0))
        table = optional(utils, TABLE)
        if table:
            curve_pieces.append(table + ";")
        helper = optional(utils, READ_HELPER)
        if helper:
            curve_pieces.append(helper)
        curve_pieces.append(definition(utils, GET_OUTPUT))
        min_span = statement(utils, r"static const f32 KF_MIN_INPUT_SPAN\s*=\s*[^;]*;", "KF_MIN_INPUT_SPAN")
        curve_pieces.append(min_span)
        curve_pieces.append(definition(utils, SLOPE_CTOR))
        curve_pieces.append(definition(utils, SLOPE_GET_VALUE))
        curve_pieces.append(definition(utils, "Slope::~Slope()"))

        effect_pieces = [effect_constants(effect)]
        effect_pieces += [definition(effect, signature) for signature in EFFECT_BODIES]

        state_pieces = [statement(state, r"static const f32 KF_TIMEOUT_TIMER\s*=\s*[^;]*;", "KF_TIMEOUT_TIMER")]
        state_pieces += [definition(state, signature) for signature in STATE_BODIES]

        manager_pieces = [statement(manager, r"static const f32 KF_PROP_BY_CACHE_LIFETIME\s*=\s*[^;]*;",
                                    "KF_PROP_BY_CACHE_LIFETIME")]
        manager_pieces += [definition(manager, signature) for signature in MANAGER_BODIES]

        post = definition(header, "bool PostPassby( const Passby& lrPassby )")
        post_body = post[post.index("{"):]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTailsBPassby.cpp"), "fxtailsb_passby_effect_bodies.inc",
                           "\n\n".join(effect_pieces), "FxTailsBPassby",
                           extra_files={
                               "fxtailsb_passby_curve_bodies.inc": "\n\n".join(curve_pieces),
                               "fxtailsb_passby_state_bodies.inc": "\n\n".join(state_pieces),
                               "fxtailsb_passby_manager_bodies.inc": "\n\n".join(manager_pieces),
                               "fxtailsb_passby_post_body.inc": post_body,
                           })


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_passby", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
