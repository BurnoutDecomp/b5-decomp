"""Execute the recovered Route Map draw bodies against recording sinks.

The runner extracts the production DrawPoint, DrawDeadEndsInMap, DrawX, ToMapCoords, and
RenderHUD bodies at invocation time, then compiles them with the canonical MSVC flags.
"""
from contextlib import redirect_stderr, redirect_stdout
from hashlib import sha256
from io import StringIO
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import STRSTREAM_CPP, Tree, compile_and_run, definition, report  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
ARTIFACTS = WORKFLOW / "scratch/DEBUG_MENU_1011/traffic_batch/route_link_helpers"
DEBUG_CPP = "src/GameSource/World/AI/Route/BrnRouteMapDebugComponent.cpp"
DEBUG_HEADER = "src/GameSource/World/AI/Route/BrnRouteMapDebugComponent.h"
BASE_HEADER = "src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

BODY_SIGNATURES = (
    "void RouteMapDebugComponent::DrawPoint(",
    "void RouteMapDebugComponent::DrawDeadEndsInMap(",
    "void RouteMapDebugComponent::DrawX(",
    "Vector2 RouteMapDebugComponent::ToMapCoords(Vector3",
    "void RouteMapDebugComponent::RenderHUD(",
)
EXPECTED_LABELS = ["Euclidean", "EuclideanXBiased", "EuclideanYBiased", "Manhattan", "Diagonal"]

BASE_HEADER_TEXT = r'''#pragma once
#include "types.hpp"
namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct Debug3DImmediateRender;
    namespace DebugUI
    {
        struct StringList { s32 miValue; const char* mpcName; };
    }
    class DebugComponent
    {
    public:
        virtual ~DebugComponent() = default;
        virtual void Update() {}
        virtual void RenderWorld(Debug3DImmediateRender*) {}
        virtual void RenderHUD(Debug2DImmediateRender*) {}
    protected:
        virtual const char* GetName() const { return ""; }
        virtual const char* GetPath() const { return ""; }
        virtual bool IsSimple() const { return false; }
        virtual void OnActivate() {}
        virtual void OnRegister() {}
    };
}
'''


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    tree = Tree()
    source = tree.read(DEBUG_CPP)
    bodies = []
    missing = []
    for signature in BODY_SIGNATURES:
        try:
            bodies.append(definition(source, signature))
        except ValueError:
            missing.append(signature)

    table_match = re.search(
        r"const char\* const KAPC_ASTAR_DISTANCE_FUNCTION_NAMES\[\]\s*=\s*\{[\s\S]*?\};",
        source,
    )
    table_values = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', table_match.group(0)) if table_match else []
    render_hud = bodies[-1] if len(bodies) == len(BODY_SIGNATURES) else ""
    wiring = [
        ("all five production bodies are extracted", not missing),
        ("actual ARTIST heuristic table contains the five ordered labels", table_values == EXPECTED_LABELS),
        ("RenderHUD indexes the existing manager enum directly", bool(re.search(
            r"mpAIModule\s*->\s*mRouteRequestManager\.meDefaultAStarDistanceFunction", render_hud))),
        ("RenderHUD no longer calls an invented distance-name getter", "GetDefaultDistanceFunctionName" not in render_hud),
    ]

    numeric = None
    fixture_output = ""
    if not missing and table_match:
        inc_text = (
            "namespace BrnAI {\nnamespace {\n"
            + table_match.group(0)
            + "\n}\n\n"
            + "\n\n".join(bodies)
            + "\n}\n"
        )
        capture = StringIO()
        with redirect_stdout(capture), redirect_stderr(capture):
            numeric = compile_and_run(
                Path(__file__).with_name("RouteMapDrawHelpers.cpp"),
                "route_map_draw_helpers.inc",
                inc_text,
                "RouteMapDrawHelpers",
                shadow={BASE_HEADER: BASE_HEADER_TEXT},
                extra_sources=(STRSTREAM_CPP,),
            )
        fixture_output = capture.getvalue()
    else:
        print("Missing production bodies or source table; numeric fixture not run.")

    summary_capture = StringIO()
    with redirect_stdout(summary_capture):
        result = report("RouteMapDrawHelpers", wiring, numeric, 1)
    summary = fixture_output + summary_capture.getvalue()
    sys.stdout.write(summary)

    source_paths = [DEBUG_CPP, DEBUG_HEADER]
    source_paths.extend((
        "tests/RouteMapDrawHelpers.cpp",
        "tests/run_route_map_draw_helpers.py",
    ))
    lines = [
        "# Route Map draw helper fixture",
        "",
        "Command: `Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue; python b5-decomp/tests/run_route_map_draw_helpers.py`",
        "The runner extracts production bodies at invocation time and compiles the fixture with the canonical MSVC flags and include list.",
        "",
        f"Result: {'PASS' if result == 0 else 'FAIL'}.",
        "",
        "## Output",
        "",
        "```text",
        summary.rstrip(),
        "```",
        "",
        "## SHA-256",
        "",
    ]
    for relative in source_paths:
        lines.append(f"- `{relative}`: `{digest(REPO / relative)}`")
    (ARTIFACTS / "fixture-result.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Fixture report: {ARTIFACTS / 'fixture-result.md'}")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
