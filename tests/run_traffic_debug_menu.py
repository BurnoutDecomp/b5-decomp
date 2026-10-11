"""Run one grouped production-body regression for the 108-row Traffic debug page."""
from pathlib import Path
import json
import os
import sys

sys.dont_write_bytecode = True
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)

from fxgs_common import REPO, WORKFLOW, Tree, code_only, compile_and_run, definition, report


COMPONENT = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.cpp"
COMPONENT_HEADER = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h"
MODULE = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
RENDER_MODULE = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule_Render.cpp"
ENTRIES = WORKFLOW / "progress/debug_menu/entries.json"


def cpp_string(value):
    return json.dumps(value, ensure_ascii=True)


def main():
    tree = Tree()
    component_source = tree.read(COMPONENT)
    component_header = tree.read(COMPONENT_HEADER)
    module_source = tree.read(MODULE)
    render_module_source = tree.read(RENDER_MODULE)
    signatures = (
        "void DebugComponent::Construct(TrafficEntityModule* lpModule)",
        "const char* DebugComponent::GetName() const",
        "void DebugComponent::Update()",
        "void DebugComponent::OnActivate()",
        "void DebugComponent::UpdateStats()",
        "void DebugComponent::FireAirRam(void* lpUserData)",
        "void DebugComponent::OnKillNearPlayer(void* lpUserData)",
        "void DebugComponent::FireKillZone(void* lpUserData)",
        "void DebugComponent::OnPickVehicle(void* lpUserData)",
        "void DebugComponent::ReloadBehaviourData(void* lpUserData)",
        "void DebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)",
        "void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)",
        "void DebugComponent::RenderPerfmon(",
        "void DebugComponent::RenderPerformanceTable(",
        "void DebugComponent::DrawActiveHulls(",
        "void DebugComponent::DrawPlayerCarHullId(",
        "void DebugComponent::DrawTextWithOffsets(",
    )
    methods = []
    missing = []
    for signature in signatures:
        try:
            methods.append(definition(component_source, signature))
        except (ValueError, IndexError):
            missing.append(signature)
    try:
        methods.append(definition(module_source, "void TrafficEntityModule::UpdateDEBUG()"))
    except (ValueError, IndexError):
        missing.append("void TrafficEntityModule::UpdateDEBUG()")
    try:
        methods.append(definition(module_source, "void TrafficEntityModule::DEBUG_AddFuzzyLogicData("))
    except (ValueError, IndexError):
        missing.append("void TrafficEntityModule::DEBUG_AddFuzzyLogicData(")

    expected_rows = []
    if ENTRIES.exists():
        export = json.loads(ENTRIES.read_text(encoding="utf-8-sig"))
        expected_rows = [
            row for row in export["entries"]
            if row.get("function") == "BrnTraffic::DebugComponent::OnActivate"
        ]

    expected_include = "const ExpectedRegistration gExpectedTrafficRegistrations[] = {\n"
    for row in expected_rows:
        kind = "E_FUNCTION" if row["kind"] == "function" else "E_VARIABLE"
        caption = row.get("caption")
        if caption is None:
            caption = "$macOverrideVehicleName"
        path = row.get("path") or ""
        expected_include += (
            "    {CgsDev::DebugComponent::%s, %s, %s},\n"
            % (kind, cpp_string(path), cpp_string(caption))
        )
    expected_include += "};\n"

    numeric = None
    if not missing and len(expected_rows) == 108:
        numeric = compile_and_run(
            Path(__file__).with_name("TrafficDebugMenu.cpp"),
            "traffic_debug_menu_methods.inc",
            "\n\n".join(methods),
            "TrafficDebugMenu",
            extra_files={"traffic_debug_expected.inc": expected_include},
        )

    wiring = [
        (f"production body present: {signature}", signature not in missing)
        for signature in signatures + ("void TrafficEntityModule::UpdateDEBUG()",)
    ]
    wiring.append(("production body present: TrafficEntityModule::DEBUG_AddFuzzyLogicData",
                   "void TrafficEntityModule::DEBUG_AddFuzzyLogicData(" not in missing))
    wiring.append(("original Traffic activation rows available", len(expected_rows) == 108))

    try:
        prepare = definition(module_source, "bool TrafficEntityModule::Prepare(")
        construct = definition(module_source, "void TrafficEntityModule::Construct()")
        post_physics = definition(module_source, "void TrafficEntityModule::PostPhysicsUpdate(")
        update_racecar_hulls = definition(module_source, "void TrafficEntityModule::UpdateRaceCarHulls(")
        update_vehicles = definition(module_source, "void TrafficEntityModule::UpdateVehicles(")
        update_non_decision = definition(module_source, "void TrafficEntityModule::UpdateNonDecisionFrame(")
        precalc_params = definition(module_source, "void TrafficEntityModule::UpdateParams_PrecalcBehaviourParams(")
        generate_dispatch = definition(render_module_source,
                                       "TrafficEntityModule::GenerateDispatchLists(")
    except (ValueError, IndexError):
        prepare = construct = post_physics = update_racecar_hulls = update_vehicles = update_non_decision = ""
        precalc_params = ""
        generate_dispatch = ""

    update_racecar_code = code_only(update_racecar_hulls)
    precalc_params_code = code_only(precalc_params)
    generate_dispatch_code = code_only(generate_dispatch)
    score_producer_order = [
        precalc_params_code.find("mFuzzyBehaviours.ProcessParamRules("),
        precalc_params_code.find("DEBUG_AddFuzzyLogicData(luParam, lafScores);"),
        precalc_params_code.find("if (mbAtStartLineSoProtectRaceCarsFromTraffic)"),
    ]
    camera_writer_order = [
        generate_dispatch_code.find("mCameraLastFrame = lBrnCamera;"),
        generate_dispatch_code.find("mFuzzyBehaviours.DEBUGSetLastCameraPos(lCameraPosition);"),
        generate_dispatch_code.find("if ( meState != E_STATE_RUNNING"),
    ]
    component_override_index = update_racecar_code.find("mpDebugComponent->UseCameraPosition()")
    component_camera_position_index = update_racecar_code.find(
        "lSimCentre = mCameraLastFrame.GetPosition()", component_override_index)
    camera_path_order = [
        update_racecar_code.find("lSimCentre = lpPlayerState->mTransform.wAxis"),
        update_racecar_code.find("E_FLAG_IS_PICTURE_PARADISE"),
        component_override_index,
        component_camera_position_index,
    ]

    wiring.extend([
        ("UpdateParams calls the original fuzzy debug writer after scoring, before score gating",
         all(position >= 0 for position in score_producer_order) and
         score_producer_order == sorted(score_producer_order) and
         precalc_params_code.count("DEBUG_AddFuzzyLogicData(luParam, lafScores);") == 1),
        ("GenerateDispatchLists stores the non-origin camera for fuzzy culling before state gates",
         all(position >= 0 for position in camera_writer_order) and
         camera_writer_order == sorted(camera_writer_order) and
         generate_dispatch_code.count("DEBUGSetLastCameraPos(lCameraPosition);") == 1),
        ("Prepare registers the native owner without a duplicate TrafficControls page",
         "mpDebugComponent->Register()" in prepare and "TrafficControls::Attach" not in prepare),
        ("Construct allocates and constructs the embedded Traffic debug owner",
         "new (lpDebugComponentStorage) DebugComponent" in construct and
         "mpDebugComponent->Construct(this)" in construct),
        ("PostPhysics calls UpdateDEBUG before external requests",
         0 <= post_physics.find("UpdateDEBUG();") < post_physics.find("HandleExternalRequests(lpInput, lpOutput);")),
        ("Use camera position is read by the original hull-centre path",
         "bool UseCameraPosition() const { return mbUseCameraPosition; }" in component_header and
         "mpDebugComponent->UseCameraPosition()" in update_racecar_hulls and
         "E_FLAG_IS_PICTURE_PARADISE" in update_racecar_hulls and
         "lSimCentre = mCameraLastFrame.GetPosition()" in update_racecar_hulls and
         all(position >= 0 for position in camera_path_order) and
         camera_path_order == sorted(camera_path_order)),
        ("PostPhysics writes the six original perf snapshots after the replay gate",
         all(name in post_physics for name in (
             "mPostPhysPerfmonData_Decision", "mParamPerfmonData_Decision",
             "mVehiclePerfmonData_Decision", "mPostPhysPerfmonData_NonDecision",
             "mParamPerfmonData_NonDecision", "mVehiclePerfmonData_NonDecision")) and
         "GetMonitorData(miPerfMon_UpdateParam" in post_physics),
        ("UpdateVehicles brackets its registered monitor handle",
         "StartMonitor(miPerfMon_UpdateVehicle)" in update_vehicles and
         "StopMonitor(miPerfMon_UpdateVehicle)" in update_vehicles),
        ("non-decision time-slice brackets its registered monitor handle",
         "StartMonitor(miPerfMon_UpdateParamNonDecision)" in update_non_decision and
         "StopMonitor(miPerfMon_UpdateParamNonDecision)" in update_non_decision),
    ])

    return report("run_traffic_debug_menu", wiring, numeric, 59)


if __name__ == "__main__":
    raise SystemExit(main())
