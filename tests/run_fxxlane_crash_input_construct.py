"""Regression for BrnWorld::CrashIO::InputBuffer_PreScene::Construct (crash parity FOLLOWUPS 19, FX-XLANE).

No out-of-line X360 symbol: CgsIOBufferStack::CreateIOBuffer<InputBuffer_PreScene> @0x827CEB30 inlines it
right after Alloc(0xAE90) -- stb 1 (IOBuffer::Construct), TimerStatusInterface::Clear(+4),
NetworkInputInterface::Construct(+0x40), VehicleDriverInputInterface::Construct(+0x3CD0),
RCEntityActiveRaceCarOutputInterface::Clear(+0x5180), VariableEventQueue<13312,16>::Construct(+0x7A70).

The harness runs the extracted body the way the PC CreateIOBuffer<T> does (placement new, then
T::Construct()) on 0xCD-poisoned storage of the real type, with the real member bodies linked.
Before the fix the tree had NO InputBuffer_PreScene::Construct, so CreateIOBuffer<T>'s call bound to
the base CgsModule::IOBuffer::Construct; with --pre-fix <rev> (a revision without the definition)
the runner models exactly that binding.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_crash_input_construct.py [--pre-fix <b5 rev>]
The pre-fix binding (b5 5e1d5a7f) fails 6/9 checks; the reconstruction passes 9/9.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import REPO, build_and_run, definition, pre_fix_rev, read

TU = "src/GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO_InputBuffers.cpp"
SIGNATURE = "    void InputBuffer_PreScene::Construct()"
PRE_FIX_BINDING = ("    // --pre-fix model: no InputBuffer_PreScene::Construct existed, so CreateIOBuffer<T>'s\n"
                   "    // T::Construct() call resolved to the base IOBuffer::Construct.\n"
                   "    void InputBuffer_PreScene::Construct() { CgsModule::IOBuffer::Construct(); }")
EXTRA = [
    "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
    "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp",
    "src/GameSource/World/CrashModule/SharedIO/NetworkInputInterface.cpp",
    "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.cpp",
    "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp",
    "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
    "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",
    "src/GameShared/GameClasses/System/Resource/CgsResourcePtr.cpp",
]


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(TU, rev)
    if SIGNATURE in text:
        body = definition(text, SIGNATURE)
    else:
        print("[run_fxxlane_crash_input_construct] no InputBuffer_PreScene::Construct in this revision: "
              "modelling the pre-fix binding (base IOBuffer::Construct)")
        body = PRE_FIX_BINDING
    rc = build_and_run(Path(__file__).with_name("FxXlaneCrashInputConstruct.cpp"), {"methods.inc": body},
                       "fxxlane_crash_input_construct", extra_sources=[REPO / p for p in EXTRA],
                       open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
