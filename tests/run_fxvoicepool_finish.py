"""FX-VOICEPOOL (crash parity 2026-09-24): crash voices never finished on PC.

The seven collision-sound states filled after seven impacts and stayed full: no crash voice ever
reported "finished", so no state detached, and from then on an impact was voiced only by evicting a
holder it outranked. The chain, against ARTIST:
  * CollisionEffect::ProcessUpdate @0x826BCFE0 sets the collision finished when Logic::Voice::
    IsPlaying @0x826943F8 goes false -- the playback voice leaves PLAYING when Slot::Update
    @0x82693B10 sees SplicerContentSlot::DoUpdatePlaying @0x826E9D58 return Splice::IsPlaying
    @0x826A3720 == false;
  * a splice sample ends in SpliceSample::Update @0x826A3A50 when (mfBasePitch / mLocPitch) *
    mfEnvLength < mtTimeIntoPlayback (0x826A3B8C..0x826A3BAC), the clock advancing by the voice's
    Pitch parameter * dt (0x826A3B74..0x826A3B84). Its rw audio-core voice is NEVER expelled by the
    sample's end on the console either: the play record is zeroed, expelMode = 0.0 (0x826A3D58..
    0x826A3D80), and SndPlayer1::RemoveRequest @0x82BA0460 expels only for expelMode 1;
  * the Pitch parameter is CollisionEffect::GetPitch() (0x826BD488..0x826BD4A0), the Send01 gain
    GetGain(). Both read effect+0x34 = the EFFECT's own EffectBase::mpDynamicMixIo (EffectBase at
    +4: vtable off_820B135C; SetDMixIOPtr @0x826808D8 stores +0x30; the control is +0x38,
    AttachController @0x8268812C). The PC read the CONTROL's endpoint, which the mix map never
    connects: gain 0 (silent) and pitch 0 -- the splice clock froze and no crash voice ever ended.
  * GetPitch also takes a size pitch of 0.0 as 1.0 (0x82688210..0x82688220).
Constants from the image: flt_820AA8F8 = 3.0518509e-05 (0x38000100), flt_820AA8F4 = 0.000244140625
(0x39800000), flt_82001C98 = 1.0, flt_82001CC0 = 0.0.

Numeric: tests/FxVoicepoolFinish.cpp compiles the PRODUCTION GetGain / GetPitch against a fixture
effect with the real Nicotine::DMixIO endpoints (DMixIO.cpp + NFSMixShape.cpp), and the PRODUCTION
SpliceObjects.cpp whole, and plays impacts through Splice::Play / Update / IsPlaying on the pitch
GetPitch returns. --rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvoicepool_finish.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report

EFFECT_CPP = "src/GameSource/Sound/Collision/BrnCollisionEffect.cpp"
CONTROL_CPP = "src/GameSource/Sound/Collision/BrnCollisionControl.cpp"
SPLICE_CPP = "src/GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.cpp"
DMIX_SOURCES = (REPO / "src/SDKs/EATech/include/Nicotine/DMixIO.cpp",
                REPO / "src/SDKs/EATech/include/NFSMix/NFSMixShape.cpp")
SIGNATURES = ("f32 CollisionEffect::GetGain() const", "f32 CollisionEffect::GetPitch() const")
NUMERIC_CHECKS = 19


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    effect = tree.read(EFFECT_CPP)
    control = tree.read(CONTROL_CPP)
    gain, pitch = (body(effect, signature) for signature in SIGNATURES)

    def own_endpoint(text):
        return (re.search(r"\bGetDMixIOPtr\s*\(\s*\)", text) is not None and
                re.search(r"mpCollisionControl", text) is None)

    return [
        ("GetGain reads the effect's OWN endpoint (effect+0x34 = EffectBase::mpDynamicMixIo), not the control's",
         own_endpoint(gain)),
        ("GetPitch reads the effect's OWN endpoint, and takes a 0.0 size pitch as 1.0 (0x82688210..0x82688220)",
         own_endpoint(pitch) and re.search(r"==\s*0\.0f\s*\)\s*\w+\s*=\s*1\.0f", pitch) is not None),
        ("a finished crash voice and the state it frees are witnessed (BRN_COLLISION_AUDIO_DIAG)",
         "[collision-audio] voice finished" in effect and "[collision-audio] detach finished" in control),
    ]


def numeric(tree):
    try:
        source = tree.read(EFFECT_CPP)
        bodies = "\n\n".join(definition(source, signature) for signature in SIGNATURES)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    splice = tree.read(SPLICE_CPP)
    if not splice:
        print("NUMERIC: cannot build -- " + SPLICE_CPP + " absent")
        return None
    return compile_and_run(Path(__file__).with_name("FxVoicepoolFinish.cpp"),
                           "fxvoicepool_effect_bodies.inc", bodies, "FxVoicepoolFinish",
                           extra_sources=DMIX_SOURCES,
                           extra_files={"fxvoicepool_spliceobjects.inc": splice})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxvoicepool_finish", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
