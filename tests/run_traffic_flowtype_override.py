"""Exercise the recovered Flowtype arm against the exact old-return source control.

The candidate and pinned 8fec93b negative control compile the production
TrafficEntityModule body, the production TrafficCarStreamer sink body, and the
existing Array<char,16> implementation into the same small fixture.
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, compile_and_run, report


MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
STREAMER_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.cpp"
BASE_RESOURCE_PTR_CPP = REPO / "src/GameShared/GameClasses/System/Resource/CgsBaseResourcePtr.cpp"
FIXTURE_CPP = Path(__file__).with_name("TrafficFlowtypeOverride.cpp")
OLD_RETURN_REV = "8fec93bd4d2806ca9b60fb53dc272fe28081f8b8"
NUMERIC_CHECKS = 44

FLOW_SIGNATURE = "void TrafficEntityModule::AddVehiclesToTargetList()"
SINK_SIGNATURE = "void TrafficCarStreamer::AddVehiclesToTargetList("
SINK_LOOP = "    for( u32 luVehicle = 0; luVehicle < luNumVehicles; luVehicle++ )"

# Test-only observation and range guard. For valid asset indices, the original extracted sink
# loop remains unchanged and executes. A byte >= 64 is captured at the call boundary and returns
# before that loop, because the real sink indexes maxLoadFlags[64] directly with the byte.
SINK_CAPTURE = r"""
    ++gFlowtypeSinkCalls;
    gFlowtypeCapturedAssetIds.clear();
    gbFlowtypeCaptureOnly = false;
    for (u32 luCapture = 0; luCapture < luNumVehicles; ++luCapture)
    {
        const u8 luCapturedAsset = lpauVehicleAssetIds[luCapture];
        gFlowtypeCapturedAssetIds.push_back(luCapturedAsset);
        if (luCapturedAsset >= KU_MAX_VEHICLE_ASSETS)
        {
            gbFlowtypeCaptureOnly = true;
        }
    }
    if (gbFlowtypeCaptureOnly)
    {
        return; // test-only guard; never forward out-of-catalogue bytes to the sink loop
    }

"""


def extracted_bodies(tree):
    module_source = tree.read(MODULE_CPP)
    streamer_source = tree.read(STREAMER_CPP)
    flow = definition(module_source, FLOW_SIGNATURE)
    sink = definition(streamer_source, SINK_SIGNATURE)
    if flow.count(FLOW_SIGNATURE) != 1:
        raise ValueError("source does not contain one AddVehiclesToTargetList body")
    if sink.count(SINK_LOOP) != 1:
        raise ValueError("production streamer sink loop shape changed; boundary guard not inserted")
    flow = flow.replace("TrafficEntityModule::", "FlowtypeFixture::", 1)
    sink = sink.replace(SINK_LOOP, SINK_CAPTURE + SINK_LOOP, 1)
    return "\n\n".join(("namespace BrnTraffic {", flow, sink, "}"))


def numeric(revision):
    try:
        bodies = extracted_bodies(Tree(revision))
    except (ValueError, IndexError) as error:
        print(f"NUMERIC: cannot build from {revision}: {error}")
        return None
    return compile_and_run(
        FIXTURE_CPP,
        "traffic_flowtype_override.inc",
        bodies,
        "TrafficFlowtypeOverride",
        extra_sources=[BASE_RESOURCE_PTR_CPP],
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default="HEAD", help="candidate B5 revision; default HEAD")
    args = parser.parse_args()

    candidate = numeric(args.rev)
    old_return = numeric(OLD_RETURN_REV)
    negative_control_detected = (
        old_return is not None
        and old_return[0] == NUMERIC_CHECKS
        and old_return[1] > 0
    )
    if old_return is None:
        print("NEGATIVE CONTROL: exact 8fec93b source did not compile/run; detection is unproven")
    else:
        print(
            "NEGATIVE CONTROL: exact 8fec93b source produced "
            f"{old_return[1]} expected recovered-arm failures out of {old_return[0]} checks"
        )

    return report(
        "run_traffic_flowtype_override",
        [("exact 8fec93b old-return control compiles and fails recovered-arm expectations",
          negative_control_detected)],
        candidate,
        NUMERIC_CHECKS,
    )


if __name__ == "__main__":
    sys.exit(main())
