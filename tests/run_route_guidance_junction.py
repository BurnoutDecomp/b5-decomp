"""Issue 70: replay the reported junction, with true left/right controls, without game assets."""
import argparse
import json
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/GameState/StreetData/BrnGameStateStreetManager.cpp")
methods, declarations = [], []
for signature in ["bool StreetManager::CanContinueWalking(", "bool StreetManager::AreRoadsAFollowOnPair(",
                  "void StreetManager::PushSectionIndex(", "BrnStreetData::RoadIndex StreetManager::GetRoadIndexFromAISectionIndex(",
                  "void StreetManager::WalkAISection(", "void StreetManager::FindUpcomingStreetsByRecursiveWalking(",
                  "void StreetManager::FindUpcomingStreetsFromRoute(", "f32 StreetManager::CalculateDistanceToTurning("]:
    body = definition(source, signature).replace("StreetManager::", "Guidance::")
    methods.append(body)
    declarations.append(body[:body.index("{")].replace("Guidance::", "") + ";")
methods.insert(0, definition(source, "int32_t CompareSectionWalkData("))
code = "namespace BrnGameState {\n" + "\n".join(methods) + "\n}\n"
code += definition(tree.read("src/SharedClasses/AI/AISectionsResourceType.cpp"),
                   "const BrnAI::AISection* BrnAI::AISectionsData::GetAISection(")
signal = definition(source, "void StreetManager::SendUpcomingRoadMessage(")
signal = signal[signal.index("if ( !lbForce )"):signal.index("CgsModule::VariableEventQueue<13312, 16>* lpQueue")]

fixture = json.loads(Path(__file__).with_name("data").joinpath("RouteGuidanceLambertNewton.json").read_text())
def f(value):
    return repr(float(value)) + "f"
size = 1 + max([s["index"] for s in fixture["sections"]]
               + [p[0] for s in fixture["sections"] for p in s["portals"]])
lines = [f"sections.resize({size});corners.resize({size});portals.resize({size});",
         f"streets.streets.resize({fixture['street_count']});",
         "for(auto& section:sections){section.miSpanIndex=-1;section.mx8Flags=1;}",
         f"data.mpaSections=sections.data();data.muNumSections={size};",
         "position={" + ",".join(map(f,fixture["position"])) + ",0};",
         "direction={" + ",".join(map(f,fixture["direction"])) + ",0};",
         f"startSection={fixture['start_section']};currentRoad={fixture['current_road']};",
         f"leftRoad={fixture['expected_left_road']};rightRoad={fixture['expected_right_road']};"]
for section in fixture["sections"]:
    i = section["index"]
    lines += [f"sections[{i}].miSpanIndex={section['span']};sections[{i}].mx8Flags={section['flags']};",
              f"sections[{i}].mpaCorners=corners[{i}].data();"]
    for j in range(4):
        lines += [f"corners[{i}][{j}]={{{f(section['corners'][j*2])},{f(section['corners'][j*2+1])}}};"]
    lines += [f"portals[{i}].resize({len(section['portals'])});sections[{i}].mu8NumPortals={len(section['portals'])};",
              f"sections[{i}].mpaPortals=portals[{i}].data();"]
    for j,(link,x,y,z) in enumerate(section["portals"]):
        lines += [f"portals[{i}][{j}].mPositionX={f(x)};portals[{i}][{j}].mPositionY={f(y)};",
                  f"portals[{i}][{j}].mPositionZ={f(z)};portals[{i}][{j}].mu16LinkSection={link};"]
    if 0 <= section["span"] < fixture["street_count"]:
        lines += [f"streets.streets[{section['span']}].road={section['road']};"]
lines += [f"cases.resize({len(fixture['routes'])});"]
for i,route in enumerate(fixture["routes"]):
    lines += [f"cases[{i}].name={json.dumps(route['name'])};cases[{i}].turn={route['expected_turn']};",
              f"cases[{i}].route.miNodeCount={len(route['nodes'])};cases[{i}].route.meStatus=BrnAI::Route::E_STATUS_COMPLETE;"]
    for j,(section,x,z) in enumerate(route["nodes"]):
        lines += [f"cases[{i}].route.maNodes[{j}].Construct({f(x)},{f(z)},{section},0);"]
result = compile_and_run(Path(__file__).with_name("RouteGuidanceJunction.cpp"), "junction_methods.inc", code,
                         "RouteGuidanceJunction", extra_files={"junction_declarations.inc":"\n".join(declarations),
                         "junction_geometry.inc":"\n".join(lines), "junction_signal.inc":signal},
                         extra_sources=[REPO / "src/SharedClasses/AI/AISection.cpp",
                                        REPO / "src/GameSource/World/AI/BrnAIPortal.cpp",
                                        REPO / "src/SharedClasses/StreetData/BrnStreetData.cpp", STRSTREAM_CPP])
raise SystemExit(report("run_route_guidance_junction", [], result, 1))
