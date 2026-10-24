"""Exercise the recovered Flowtype arm against the exact old-return source control.

The candidate and pinned 8fec93b negative control compile the production
TrafficEntityModule selector and TrafficCarStreamer sink bodies with the real
Array<char,16>. The six real TrafficCarStreamer virtual definitions are also
included only to close the production class vtable; this fixture does not call
or claim coverage for those streaming callbacks.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, compile_and_run, report


MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
STREAMER_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.cpp"
RESOURCE_SUPPORT_CPP = (
    "src/GameShared/GameClasses/System/Resource/CgsBaseResourcePtr.cpp",
    "src/GameShared/GameClasses/System/Resource/CgsResourcePtr.cpp",
    "src/GameShared/GameClasses/System/Resource/CgsResourceHandle.cpp",
)
RESOURCE_SUPPORT_SOURCES = tuple(REPO / path for path in RESOURCE_SUPPORT_CPP)
FIXTURE_CPP = Path(__file__).with_name("TrafficFlowtypeOverride.cpp")
OLD_RETURN_REV = "8fec93bd4d2806ca9b60fb53dc272fe28081f8b8"
NUMERIC_CHECKS = 44

FLOW_SIGNATURE = "void TrafficEntityModule::AddVehiclesToTargetList()"
SINK_SIGNATURE = "void TrafficCarStreamer::AddVehiclesToTargetList("
SINK_LOOP = "    for( u32 luVehicle = 0; luVehicle < luNumVehicles; luVehicle++ )"

# These real definitions are link providers for TrafficCarStreamer's emitted base vtable.
# The fixture does not invoke their streaming behavior.
STREAMER_CALLBACKS = (
    "s32 TrafficCarStreamer::QueryLoad(",
    "s32 TrafficCarStreamer::QueryUnload(",
    "void TrafficCarStreamer::OnLoadBegin(",
    "void TrafficCarStreamer::OnUnloadBegin(",
    "void TrafficCarStreamer::OnLoadComplete(",
    "void TrafficCarStreamer::OnUnloadComplete(",
)


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

    callback_bodies = []
    for signature in STREAMER_CALLBACKS:
        if streamer_source.count(signature) != 1:
            raise ValueError("source does not contain exactly one real streamer callback: " + signature)
        callback_bodies.append(definition(streamer_source, signature))

    flow = flow.replace("TrafficEntityModule::", "FlowtypeFixture::", 1)
    sink = sink.replace(SINK_LOOP, SINK_CAPTURE + SINK_LOOP, 1)
    return "\n\n".join(("namespace BrnTraffic {", flow, sink, *callback_bodies, "}"))


def numeric(revision, output_dir=None, shared_provider_revision=None):
    try:
        bodies = extracted_bodies(Tree(revision))
    except (ValueError, IndexError) as error:
        print(f"NUMERIC: cannot build from {revision}: {error}")
        return None
    result = compile_and_run(
        FIXTURE_CPP,
        "traffic_flowtype_override.inc",
        bodies,
        "TrafficFlowtypeOverride",
        extra_sources=RESOURCE_SUPPORT_SOURCES,
        output_dir=output_dir,
    )
    if output_dir is not None:
        source_objects = {
            path: subprocess.check_output(
                ["git", "-C", str(REPO), "rev-parse",
                 f"{shared_provider_revision if path in RESOURCE_SUPPORT_CPP else revision}:{path}"],
                text=True,
            ).strip()
            for path in (MODULE_CPP, STREAMER_CPP, *RESOURCE_SUPPORT_CPP)
        }
        extraction = {
            "schema": 2, "source_revision": revision,
            "shared_provider_revision": shared_provider_revision,
            "source_objects": source_objects,
            "include_name": "traffic_flowtype_override.inc",
            "generated_include_lf_sha256": hashlib.sha256(bodies.encode("utf-8")).hexdigest(),
            "coverage": "Production selector and sink fixture; six real streamer callbacks included only for vtable link closure, not exercised; high bytes stop at capture; native #0081 unverified.",
        }
        (Path(output_dir) / "extraction.json").write_text(
            json.dumps(extraction, indent=2) + "\n", encoding="utf-8"
        )
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default="HEAD", help="candidate B5 revision; default HEAD")
    parser.add_argument("--output-dir", type=Path,
                        help="retain both variants in a fresh artifact directory outside B5")
    args = parser.parse_args()
    output = args.output_dir.resolve() if args.output_dir is not None else None
    if output is not None:
        if (output.is_relative_to(REPO) or REPO.parent.is_relative_to(output)):
            parser.error("--output-dir must be an artifact directory outside B5, not a source ancestor")
        if output.exists():
            parser.error("--output-dir must be fresh; existing artifacts are never overwritten")
        output.mkdir(parents=True)

    # Both variants compile the same actual checkout providers. Only the extracted
    # selector/sink bodies vary; record that distinction instead of attributing
    # baseline provider objects to files that were never compiled.
    shared_provider_revision = subprocess.check_output(
        ["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True).strip()
    candidate_revision = subprocess.check_output(
        ["git", "-C", str(REPO), "rev-parse", args.rev], text=True).strip()
    if candidate_revision != shared_provider_revision:
        parser.error("candidate revision must match the checkout supplying the real shared providers")
    candidate = numeric(candidate_revision, output / "candidate" if output is not None else None,
                        shared_provider_revision)
    old_return = numeric(OLD_RETURN_REV, output / "baseline" if output is not None else None,
                         shared_provider_revision)
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
