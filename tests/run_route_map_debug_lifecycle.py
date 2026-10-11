"""Bounded lifecycle regression for the restored original Route Map debug owner."""
from hashlib import sha256
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, compile_and_run, definition, report  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
ARTIFACTS = WORKFLOW / "scratch/DEBUG_MENU_1011/traffic_batch/route_lifecycle_fixture"

MODULE_CPP = "src/GameSource/World/AI/Route/BrnRouteMapModule.cpp"
MODULE_H = "src/GameSource/World/AI/Route/BrnRouteMapModule.h"
DEBUG_CPP = "src/GameSource/World/AI/Route/BrnRouteMapDebugComponent.cpp"
DEBUG_H = "src/GameSource/World/AI/Route/BrnRouteMapDebugComponent.h"
AIMODULE_CPP = "src/GameSource/World/AI/BrnAIModule.cpp"

BODY_SPECS = [
    (MODULE_CPP, "void RouteMapModule::Construct(AIModule* lpAIModule)"),
    (MODULE_CPP, "bool RouteMapModule::Prepare(CgsResource::ResourceHandle lHandle)"),
    (MODULE_CPP, "bool RouteMapModule::Release()"),
    (MODULE_CPP, "void RouteMapModule::Destruct()"),
    (DEBUG_CPP, "void RouteMapDebugComponent::Construct(RouteMapModule* lpRouteMapModule, AIModule* lpAIModule)"),
    (DEBUG_CPP, "void RouteMapDebugComponent::Destruct()"),
    (DEBUG_CPP, "const char* RouteMapDebugComponent::GetName() const"),
    (DEBUG_CPP, "const char* RouteMapDebugComponent::GetPath() const"),
    (DEBUG_CPP, "void RouteMapDebugComponent::OnActivate()"),
    (DEBUG_CPP, "void RouteMapDebugComponent::Advance100SectionsCallback(void* lpUserData)"),
    (DEBUG_CPP, "void RouteMapDebugComponent::Back100SectionsCallback(void* lpUserData)"),
]


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    tree = Tree()
    module_cpp = tree.read(MODULE_CPP)
    module_h = tree.read(MODULE_H)
    debug_cpp = tree.read(DEBUG_CPP)
    debug_h = tree.read(DEBUG_H)
    aimodule_cpp = tree.read(AIMODULE_CPP)

    extracted = []
    missing = []
    for relative, signature in BODY_SPECS:
        try:
            extracted.append(definition(tree.read(relative), signature))
        except ValueError:
            missing.append(signature)

    typed_member = bool(re.search(
        r"\bRouteMapDebugComponent\s+mRoupeMapDebugComponent\s*;", module_h))
    aimodule_body = body_or_empty(aimodule_cpp, "void AIModule::Construct()")
    aimodule_calls_member = "mRouteMapModule.Construct(this);" in aimodule_body
    module_body = body_or_empty(module_cpp, BODY_SPECS[0][1])
    route_constructs_debug_member = "mRoupeMapDebugComponent.Construct(this, lpAIModule);" in module_body
    debug_identity_methods = all(
        signature in debug_h for signature in ("const char* GetName() const;", "const char* GetPath() const;"))

    wiring = [
        ("all 11 tested bodies are extracted from production source", not missing),
        ("RouteMapModule owns a typed RouteMapDebugComponent with original member spelling", typed_member),
        ("AIModule::Construct calls its RouteMapModule member with this", aimodule_calls_member),
        ("RouteMapModule::Construct initializes the embedded component with both owners", route_constructs_debug_member),
        ("debug header declares both identity overrides", debug_identity_methods),
    ]

    numeric = None
    if not missing:
        fixture = Path(__file__).with_name("RouteMapDebugLifecycle.cpp")
        numeric = compile_and_run(
            fixture,
            "route_map_debug_lifecycle_methods.inc",
            "\n\n".join(extracted),
            "RouteMapDebugLifecycle",
        )
    else:
        print("Missing production bodies: " + "; ".join(missing))

    result = report("RouteMapDebugLifecycle", wiring, numeric, 1)
    numeric_checks, numeric_failures = numeric if numeric is not None else (1, 1)
    wiring_failures = sum(1 for _, passed in wiring if not passed)
    source_paths = [MODULE_CPP, MODULE_H, DEBUG_CPP, DEBUG_H, AIMODULE_CPP]
    lines = [
        "# Route Map debug lifecycle regression",
        "",
        "Command: `Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue; python b5-decomp/tests/run_route_map_debug_lifecycle.py`",
        "Working directory: `D:\\Reverse\\BP-Decomp_Workflow`",
        f"Result: {'PASS' if result == 0 else 'FAIL'}; wiring {len(wiring) - wiring_failures}/{len(wiring)}, numeric {numeric_checks - numeric_failures}/{numeric_checks}.",
        "",
        "The runner extracts production method bodies at invocation time and compiles them through `tests/fxgs_common.py::compile_and_run`.",
        "",
        "## SHA-256",
        "",
    ]
    for relative in source_paths:
        lines.append(f"- `{relative}`: `{digest(REPO / relative)}`")
    lines.append(f"- `b5-decomp/tests/RouteMapDebugLifecycle.cpp`: `{digest(Path(__file__).with_name('RouteMapDebugLifecycle.cpp'))}`")
    lines.append(f"- `b5-decomp/tests/run_route_map_debug_lifecycle.py`: `{digest(Path(__file__))}`")
    (ARTIFACTS / "result.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Fixture report: {ARTIFACTS / 'result.md'}")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
