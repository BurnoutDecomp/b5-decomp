"""FX-WATERSND (crash parity 2026-09-24): a player crash into water stopped the game.

FxEffect::UpdateParams @0x826BC338 posts sound message 4 with FxType 8 (E_CRASH_IN_WATER) on the
rising edge of HasCrashedIntoWater(player) (bl 0x823A7BC8 @0x826BC420). FxEffect::Notify
@0x826F7248 case 8 must hand VoiceWrapper::Create the COLLISION state manager's own splice bank:

  0x826F749C  lwz   r11, 0x28(r31)      ; mpLogicModule
  0x826F74A4  lwz   r11, 0x2968(r11)    ; module+0x2950 Environment (0x826AFF18) -> map slot 5
                                        ;   (AddStateManager 0x82680E0C: +4 + 4*type; Collision = 5)
  0x826F74AC  addis r30, r11, 1
  0x826F74B0  addi  r30, r30, -0x7DD8   ; +0x8228 = mCollisionSplicerBank[0] (Prepare 0x826F8B78)

The pre-fix arm set the bank to NULL under a stale "no PC producer posts type 8" FLAG:
[ASSERT] mCreateParams.mpContent (CgsVoiceWrapper.cpp:44), then an access violation reading 0x8.

Wiring: the production arm walks GetEnvironment().GetStateManager(5) -> GetSplicerBank(bank 0), and
the anchors that make slot 5 the collision manager hold in the tree. Numeric: tests/FxWaterSndNotify.cpp
compiled against the PRODUCTION Notify / FindFreeVoice / UpdateParams / selector block (+ IntClamp).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxwatersnd_notify.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

FX_CPP = "src/GameSource/Sound/Global/BrnFxEffect.cpp"
COLLISION_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
COLLISION_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
MODULE_H = "src/GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
ENVIRONMENT_CPP = "src/GameShared/GameClasses/Sound/Logic/CgsEnvironment.cpp"
CLAMP_CPP = "src/GameShared/GameClasses/Numeric/CgsBranchlessOperations.cpp"

NOTIFY = "void FxEffect::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)"
FIND_FREE = "s32 FxEffect::FindFreeVoice() const"
UPDATE_PARAMS = "void FxEffect::UpdateParams("
SELECTORS = "namespace\n{\n    // [DIAG] NOT IN THE X360 BINARY -- witness budget."
INT_CLAMP = "s32 IntClamp(s32 liValue, s32 liLow, s32 liHigh)"

FIXTURE = "FxWaterSndFixture"
NUMERIC_CHECKS = 35

SUBSTITUTIONS = [
    (r"\bFxEffect::", FIXTURE + "::"),
    (r"(?:BrnSound::Logic::)?Collision::CollisionStateManager\b", "CollisionStateManagerFixture"),
    (r"BrnSound::Module::SoundLogicModule\b", "SoundLogicModuleFixture"),
    (r"CgsSound::Logic::VoiceWrapper::CreateParams\b", "CreateParamsFixture"),
    (r"CgsSound::Logic::Content\b", "ContentFixture"),
    (r"CgsSound::Playback::Name::MakeHash\b", "MakeHashFixture"),
    (r"BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface\b", "VehicleInterfaceFixture"),
]


def crash_in_water_arm(notify_code):
    """The code (comments stripped) of Notify's E_CRASH_IN_WATER arm, up to the next case label."""
    match = re.search(r"case\s+FxMessage::E_CRASH_IN_WATER\s*:(.*?)(?=case\s+FxMessage::|default\s*:)",
                      notify_code, re.S)
    return match.group(1) if match else ""


def wiring(tree):
    fx = tree.read(FX_CPP)
    notify = body_or_empty(fx, NOTIFY)
    arm = crash_in_water_arm(notify)
    collision_cpp = tree.read(COLLISION_CPP)
    type_info = body_or_empty(collision_cpp, "CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* "
                                             "CollisionStateManager::GetStaticTypeInfo()")
    collision_h = code_only(tree.read(COLLISION_H))
    module_h = code_only(tree.read(MODULE_H))
    environment = tree.read(ENVIRONMENT_CPP)
    add = body_or_empty(environment, "bool Environment::AddStateManager(StateManager* apStateManager)")
    get = body_or_empty(environment, "StateManager* Environment::GetStateManager(s32 liStateManId) const")
    return [
        ("Notify case 8 walks GetEnvironment().GetStateManager(5) (module+0x2968)",
         re.search(r"GetEnvironment\(\)\s*\.\s*GetStateManager\(\s*5\s*\)", arm) is not None),
        ("Notify case 8 casts to Collision::CollisionStateManager and takes "
         "GetSplicerBank(E_COLLISION_SPLICE_BANK_COLLISION) (+0x8228)",
         "CollisionStateManager" in arm and
         re.search(r"&\s*\w+\s*->\s*GetSplicerBank\(\s*(?:Collision::)?E_COLLISION_SPLICE_BANK_COLLISION\s*\)",
                   arm) is not None),
        ("Notify case 8 no longer pins the bank to NULL",
         arm != "" and re.search(r"lpBank\s*=\s*(?:0|nullptr|NULL)\s*;", arm) is None),
        ("Notify tail asserts liIndexToUse >= 0 / < KI_NUMBER_OF_FX_VOICES (0x826F7630..0x826F7674)",
         '"liIndexToUse >= 0"' in fx and '"liIndexToUse < KI_NUMBER_OF_FX_VOICES"' in fx),
        ("CollisionStateManager registers ObjectID 5 (its Environment map slot)",
         re.search(r"sTypeInfo\(\s*5\s*,", type_info) is not None),
        ("CollisionStateManager::GetSplicerBank returns mCollisionSplicerBank[bank]",
         re.search(r"GetSplicerBank\([^)]*\)\s*const\s*\{[^}]*return\s+mCollisionSplicerBank\[\s*\w+\s*\]",
                   collision_h, re.S) is not None),
        ("Logic::Module::GetEnvironment returns the embedded mEnvironment (module+0x2950)",
         re.search(r"Environment&\s+GetEnvironment\(\)\s*\{\s*return\s+mEnvironment;\s*\}", module_h) is not None),
        ("Environment keys mapStateManagers by state type (AddStateManager 0x82680E0C / GetStateManager 0x8268D1C0)",
         "mapStateManagers[liStateType] = apStateManager" in add and
         "return mapStateManagers[liStateManId]" in get),
    ]


def substitute(text):
    for pattern, replacement in SUBSTITUTIONS:
        text = re.sub(pattern, replacement, text)
    return text


def numeric(tree):
    fx = tree.read(FX_CPP)
    try:
        selectors = definition(fx, SELECTORS)
        find_free = definition(fx, FIND_FREE)
        notify = definition(fx, NOTIFY)
        update = definition(fx, UPDATE_PARAMS)
        clamp = definition(tree.read(CLAMP_CPP), INT_CLAMP)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    methods = "\n".join([selectors, substitute(find_free), substitute(notify), substitute(update)])
    return compile_and_run(Path(__file__).with_name("FxWaterSndNotify.cpp"), "fxwatersnd_methods.inc",
                           methods, "FxWaterSndNotify",
                           shadow={"src/GameSource/Sound/Global/BrnFxEffect.h":
                                   tree.read("src/GameSource/Sound/Global/BrnFxEffect.h")},
                           extra_files={"fxwatersnd_intclamp.inc": clamp})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxwatersnd_notify", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
