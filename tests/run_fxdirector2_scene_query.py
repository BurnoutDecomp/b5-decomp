"""FX-DIRECTOR2 (crash parity 2026-09-25): THE DIRECTOR'S CAMERA SCENE-QUERY CLOSURE.

  The director's cameras ASK the scene manager whether they can see their target and whether they are
  about to hit the world, and READ THE ANSWERS a pass later -- MainDirector's PreScene pass calls
  BehaviourManager::GenerateSceneQueries @0x8221F1C0 (0x82255834), DoUpdate_Director @0x823E8DE0 runs the
  queries in between (SceneQueryInterface::Append @0x823C4FF8 -> WorldModule::ExternalSceneQueriesUpdate),
  DirectorModule::Update routes the results into the post offices (ProcessSceneQueryResults @0x82239278)
  and the PostScene pass calls BehaviourManager::ProcessSceneQueryResults @0x8221F438 (0x8224FF30), where a
  VisibilityCollisionPolicy (@0x822402F8 / @0x82224530) FAILS its camera with a ValidityAccount reason.
  On the PC none of it existed: the post offices were opaque spans, the policies had no bodies, the
  ICE-anim behaviour wrote 3 of its policy's ~25 Construct stores and never re-targeted it.

Numeric: tests/FxDirector2SceneQuery.cpp compiles the PRODUCTION BrnSceneQueryInterface.cpp,
BrnVisibilityTest.cpp, BrnGeometryCollisionPredictor.cpp and BrnVisibilityCollisionPolicy.cpp, plus the
extracted DirectorModule::ProcessSceneQueryResults, SceneQueryInputBuffer::Construct / GetResultsQueue,
SceneQueryInterface::Append, ValidityAccount::SetFlag and CameraState::ClearFlag, and runs them against a
fixture scene manager that answers through a real results queue: the post offices, the id minting, the
router, Append, the visibility test, the prediction, the ground constraint and every failure reason --
the later-frame timeouts against the console's own roll sequence, and the ground constraint's two height
tripwires by polarity (C18-C20: `bge` skips on NaN, 0x82240228 / 0x8220E3CC). A revision without the bodies does
not build (every numeric check counts as failed, the missing pieces named).
Wiring: the module / manager / director / game-module call sites and their order, the ICE-anim Construct
and SetTarget, the mounts, one CollisionPolicy::Fail, the forward-declaration class key.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_scene_query.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report

DIR = "src/GameSource/Director/"
SQI_CPP = DIR + "Utils/BrnSceneQueryInterface.cpp"
VISTEST_CPP = DIR + "Camera/BrnVisibilityTest.cpp"
PREDICTOR_CPP = DIR + "Camera/BrnGeometryCollisionPredictor.cpp"
POLICY_CPP = DIR + "Camera/BrnVisibilityCollisionPolicy.cpp"
POLICY_H = DIR + "Camera/BrnCollisionPolicy.h"
ACCOUNT_CPP = DIR + "Camera/BrnCameraValidityAccount.cpp"
STATE_CPP = DIR + "Camera/BrnCameraState.cpp"
MODULE_CPP = DIR + "DirectorModule/BrnDirectorModule.cpp"
SQIO_CPP = DIR + "DirectorModule/BrnDirectorModuleIOSceneQuery.cpp"
MANAGER_CPP = DIR + "Camera/BrnBehaviourManager.cpp"
MAIN_CPP = DIR + "BrnMainDirector.cpp"
ICE_CPP = DIR + "Camera/Behaviours/BrnBehaviourIceAnim.cpp"
BEHAVIOUR_H = DIR + "Camera/Behaviours/Behaviour.h"
GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
APPEND_CPP = "src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.cpp"
HEADERS = (POLICY_H, DIR + "Utils/BrnPostBox.h", DIR + "Utils/BrnPostOffice.h", DIR + "Utils/BrnDirectorPostOfficeTypes.h",
           DIR + "Utils/BrnSceneQueryInterface.h", DIR + "DirectorModule/BrnDirectorModuleIOSceneQuery.h",
           "src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h",
           "src/GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h")
EXTRA_SOURCES = (STRSTREAM_CPP, REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
                 REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp")
BUILD_BAT = WORKFLOW / "tools/build/build_game_exe.bat"
NUMERIC_CHECKS = 6 + 5 + 3 + 2 + 10 + 3 + 20

WHOLE_TUS = (SQI_CPP, VISTEST_CPP, PREDICTOR_CPP, POLICY_CPP)
EXTRACTS = (
    (ACCOUNT_CPP, "void ValidityAccount::SetFlag(s32 leFlag)", "namespace BrnDirector { namespace Camera {", "} }"),
    (STATE_CPP, "void CameraState::ClearFlag(u32 luIndex)", "namespace BrnDirector { namespace Camera {", "} }"),
    (APPEND_CPP, "void CgsSceneManager::SceneManagerIO::SceneQueryInterface::Append(const SceneQueryInterface& lOther)", "", ""),
    (MODULE_CPP, "void DirectorModule::ProcessSceneQueryResults(", "namespace BrnDirector {", "}"),
    (SQIO_CPP, "void SceneQueryInputBuffer::Construct()", "namespace BrnDirector { namespace DirectorIO {", "} }"),
    (SQIO_CPP, "const SceneQueryInputBuffer::ResultsQueue* SceneQueryInputBuffer::GetResultsQueue() const",
     "namespace BrnDirector { namespace DirectorIO {", "} }"),
)
# The production policy body must be the DWARF one (a revision with the see-through slice has no Generate).
POLICY_BODIES = ("void VisibilityCollisionPolicy::GenerateSceneQueries(", "void VisibilityCollisionPolicy::ProcessSceneQueryResults(",
                 "void CollisionPolicy::Fail(Camera& lrCamera, s32 leFailedFlag)")


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def numeric(tree):
    text = "// GENERATED by run_fxdirector2_scene_query.py\n"
    missing = []
    for relative in WHOLE_TUS:
        source = tree.read(relative)
        if not source:
            missing.append(relative)
        text += f"\n// ---- {relative}\n{source}\n"
    policy = tree.read(POLICY_CPP)
    missing += [signature for signature in POLICY_BODIES if signature not in policy]
    for relative, signature, head, tail in EXTRACTS:
        extracted = body(tree.read(relative), signature)
        if not extracted:
            missing.append(f"{relative}: {signature}")
            continue
        text += f"\n// ---- {relative}\n{head}\n{extracted}\n{tail}\n"
    header_missing = [header for header in HEADERS if not tree.read(header)]
    if missing or header_missing:
        print(f"[fxdirector2] the closure bodies are missing from this revision -- the numeric test cannot build: "
              f"{missing + header_missing}", flush=True)
        return None
    shadow = {}
    if tree.rev is not None:
        for header in HEADERS:
            shadow[header] = tree.read(header)
    return compile_and_run(Path(__file__).with_name("FxDirector2SceneQuery.cpp"), "fxdirector2_scene_query.inc", text,
                           "fxdirector2_scene_query", shadow=shadow, extra_sources=EXTRA_SOURCES)


def in_order(text, needles):
    """True when every needle is found, each after the previous one."""
    at = -1
    for needle in needles:
        at = text.find(needle, at + 1)
        if at < 0:
            return False
    return True


def wiring(tree):
    module = tree.read(MODULE_CPP)
    construct = squash(body(module, "void DirectorModule::Construct("))
    psqu = squash(body(module, "s32 DirectorModule::PreSceneQueryUpdate("))
    update = squash(body(module, "s32 DirectorModule::Update("))
    yield ("DirectorModule::Construct constructs the six post offices (fine, nearest, fast-DS, sphere, volume fine, deepest)",
           in_order(construct, ["mLineTestFinePostOffice.Construct();", "mLineTestNearestPostOffice.Construct();",
                                "mLineTestFastDoubleSidedPostOffice.Construct();", "mSphereTestFastPostOffice.Construct();",
                                "mVolumeTestFinePostOffice.Construct();", "mVolumeTestDeepestPostOffice.Construct();"]))
    yield ("PreSceneQueryUpdate hands the per-frame handle ALL SIX offices and clears it -- the pass that mints ids",
           "lSceneQuery.Construct(lpProducerOf(lpSceneQueryOutputBuffer),&mLineTestFinePostOffice,&mLineTestNearestPostOffice,"
           "&mLineTestFastDoubleSidedPostOffice,&mSphereTestFastPostOffice,&mVolumeTestFinePostOffice,"
           "&mVolumeTestDeepestPostOffice);lSceneQuery.Clear();" in psqu)
    yield ("Update delivers this frame's answers first (ProcessSceneQueryResults), then builds the producer-only handle",
           in_order(update, ["ProcessSceneQueryResults(lpSceneQueryInputBuffer);",
                             "lSceneQuery.Construct(lpProducerOf(lpSceneQueryOutputBuffer),0,0,0,0,0,0);"]))

    manager = tree.read(MANAGER_CPP)
    generate = squash(body(manager, "void BehaviourManager::GenerateSceneQueries(bool lbPaused"))
    process = squash(body(manager, "void BehaviourManager::ProcessSceneQueryResults(bool lbPaused"))
    helper_process = squash(body(manager, "void BehaviourManager::BehaviourHelper::ProcessSceneQueryResults("))
    helper_generate = squash(body(manager, "void BehaviourManager::BehaviourHelper::GenerateSceneQueries("))
    loop = ("mBehaviourHelperIndexArray.GetLength();", "mBehaviourUpdateDuringPauseFlags.IsBitSet(")
    yield ("BehaviourManager::GenerateSceneQueries / ProcessSceneQueryResults walk the live helpers with the pause filter "
           "(0x8221F1C0 / 0x8221F438)",
           all(needle in generate and needle in process for needle in loop)
           and ".GenerateSceneQueries(lrSharedInfo);" in generate and ".ProcessSceneQueryResults(lrSharedInfo);" in process)
    yield ("the helpers skip a behaviour with no policy or already failed; a policy failure fails the behaviour with "
           "E_FAILED_COLLISION_POLICY (5)",
           "if(lpPolicy!=0&&!lpBehaviour->HasFailed())lpPolicy->GenerateSceneQueries(lrSharedInfo,mCamera);" in helper_generate
           and in_order(helper_process, ["if(lpPolicy!=0&&!lpBehaviour->HasFailed())",
                                         "lpPolicy->ProcessSceneQueryResults(lrSharedInfo,mCamera);",
                                         "if(lpPolicy->HasFailed())", "lpBehaviour->Fail(mCamera,5);"]))

    main = tree.read(MAIN_CPP)
    pre = squash(body(main, "void MainDirector::UpdateCameraBehavioursPreScene("))
    post = squash(body(main, "void MainDirector::UpdateCameraBehavioursPostScene("))
    yield ("MainDirector: the cameras ASK right after UpdateAllBehaviours (0x82255834) and READ before the collision pass "
           "(0x8224FF30), each with the shared-info build",
           in_order(pre, ["mBehaviourManager.UpdateAllBehaviours(", "BuildCollisionPolicySharedInfo(lpIO,liPlayerCarIndex,",
                          "mBehaviourManager.GenerateSceneQueries("])
           and in_order(post, ["BuildCollisionPolicySharedInfo(lpIO,liPlayerCarIndex,", "mBehaviourManager.ProcessSceneQueryResults(",
                               "PostCollisionUpdateAllBehaviours("]))

    game = code_only(body(tree.read(GAME_CPP), "void BrnGameModule::DoUpdate_Director(bool lbPostGui)"))
    append = game.find("->GetSceneQueryInterface()->Append(")
    psqu_call = game.rfind("mDirectorModule.PreSceneQueryUpdate(", 0, append if append >= 0 else 0)
    yield ("DoUpdate_Director runs the console's leg between the passes: PreSceneQueryUpdate -> Append -> "
           "ExternalSceneQueriesUpdate -> results Append -> Update (0x823E8FC4 .. 0x823E9144)",
           append >= 0 and psqu_call >= 0
           and in_order(game[append:], ["->GetSceneQueryInterface()->Append(", "ExternalSceneQueriesUpdate(",
                                        "GetResultsQueue()->Append(", "mDirectorModule.Update("]))

    ice = tree.read(ICE_CPP)
    director_sources = "".join(tree.read(DIR + name) for name in (
        "Camera/Behaviours/BrnBehaviourIceAnim.cpp", "Camera/Behaviours/BrnBehaviourGyroCam.cpp", POLICY_H[len(DIR):]))
    yield ("BehaviourIceAnim: Construct runs the policy's whole Construct; Update re-targets it at the player every frame "
           "(0x82247204..0x82247258); no see-through setters left",
           "mCollisionPolicy.Construct();" in squash(body(ice, "void BehaviourIceAnim::Construct("))
           and "mCollisionPolicy.SetTarget(lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform,lrSharedInfo.mPlayerInfo.mAABB,"
               in squash(ice)
           and "SetSeeThrough" not in code_only(director_sources))

    bat = BUILD_BAT.read_text(encoding="utf-8", errors="replace") if BUILD_BAT.exists() else ""
    yield ("the build mounts the three policy TUs (BrnVisibilityCollisionPolicy / BrnGeometryCollisionPredictor / "
           "BrnVisibilityTest)",
           all(f'echo "%SRC%\\GameSource\\Director\\Camera\\{name}.cpp"' in bat
               for name in ("BrnVisibilityCollisionPolicy", "BrnGeometryCollisionPredictor", "BrnVisibilityTest")))
    fails = len(re.findall(r"void\s+CollisionPolicy::Fail\s*\(\s*Camera&\s+lrCamera\s*,\s*s32\s+leFailedFlag\s*\)",
                           code_only(tree.read(POLICY_CPP)) + code_only(tree.read(DIR + "Camera/BrnCameraCollisionPolicy.cpp"))))
    yield ("one CollisionPolicy::Fail(Camera&, s32) (0x82206450), in the mounted policy TU", fails == 1)
    yield ("Behaviour.h forward-declares SceneQueryInterface with its definition's class key (struct -- MSVC mangles it)",
           re.search(r"^\s*struct\s+SceneQueryInterface\s*;", code_only(tree.read(BEHAVIOUR_H)), re.M) is not None)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to read the sources from (default: the working tree)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    checks = list(wiring(tree))
    result = numeric(tree)
    sys.exit(report("fxdirector2_scene_query", checks, result, NUMERIC_CHECKS))


if __name__ == "__main__":
    main()
