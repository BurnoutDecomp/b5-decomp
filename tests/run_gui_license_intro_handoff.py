"""Hosted selected-frame License intro handoff, with actual production bodies.

No game is started. --old-tail-cache restores only the old placement in the current
source; it must execute the same 64 checks and report the binding-before-use witness.
The extracted full Intro handler and unguarded License body run against explicit
profile/calendar/VM/collaborator/FSM boundaries. This is not a full GUI/ABI fixture.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path

from fxgs_common import Tree, code_only, compile_and_run, definition, report

TAG = "GUI_LICENSE_INTRO_HANDOFF"
NUMERIC_TOTAL = 64
WITNESS = "FAIL license profile bound before cache use"
GUI = "src/GameSource/Gui/BrnGuiModule.cpp"
GUI_H = "src/GameSource/Gui/BrnGuiModule.h"
INTRO = "src/GameSource/Gui/Flow/Screen/States/BrnIntro.cpp"
INTRO_H = "src/GameSource/Gui/Flow/Screen/States/BrnIntro.h"
LICENSE = "src/GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.cpp"
LICENSE_H = "src/GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"
PROFILE_H = "src/GameSource/GameState/Progression/BrnProfile.h"
GSM = "src/GameSource/GameState/BrnGameStateModule.cpp"
ACTIONS = "src/GameSource/GameState/BrnGameActions.h"
TRANSLATOR = "src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp"
PUSH_H = "src/GameSource/Game/GameBridgeGameStateToX.h"
EVENTS_H = "src/GameSource/Gui/BrnGuiEventTypeDefs.h"
GUI_EVENT_H = "src/GameShared/GameClasses/Gui/CgsGuiEvent.h"
INTERFACE = "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.cpp"
QUEUE_H = "src/GameShared/GameClasses/Module/CgsVariableEventQueue.h"


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = root

    def read(self, relative):
        if self.root is not None and (self.root / relative).is_file():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        return super().read(relative)


def statement(text, signature):
    start = text.index(signature)
    tokens = re.finditer(
        r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'
        r"'(?:\\.|[^'\\])*'|;", text[start:])
    for token in tokens:
        if token[0] == ";":
            return text[start:start + token.end()]
    raise ValueError("unterminated declaration: " + signature)


def in_order(text, pieces):
    pos = 0
    for piece in pieces:
        found = text.find(piece, pos)
        if found < 0:
            return False
        pos = found + len(piece)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev", help="Read production sources from this b5 revision")
    parser.add_argument("--root", type=Path, help="Optional production-source overlay")
    parser.add_argument("--output-dir", type=Path, help="Retain receipts in a new/empty folder")
    parser.add_argument("--old-tail-cache", action="store_true")
    args = parser.parse_args()
    if args.output_dir and args.output_dir.exists() and any(args.output_dir.iterdir()):
        parser.error("Refusing to overwrite fixture receipts: " + str(args.output_dir))
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    tree = RootTree(args.rev, args.root)
    inputs = {}

    def read(path):
        if path not in inputs:
            inputs[path] = tree.read(path).replace("\r\n", "\n").replace("\r", "\n")
        return inputs[path]

    def body(path, signature):
        return definition(read(path), signature)

    try:
        queue_text = read(QUEUE_H)
        queue_defs = []
        for sig in (
            "void VariableEventQueue<BUFSIZE, ALIGN>::Construct()",
            "void VariableEventQueue<BUFSIZE, ALIGN>::Clear()",
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetMaxLength() const",
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetLength() const",
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetEventPaddingSize(",
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetFirstEvent(",
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetNextEvent(",
            "bool VariableEventQueue<BUFSIZE, ALIGN>::AddEvent(const Event*",
            "void VariableEventQueue<BUFSIZE, ALIGN>::OutputQueueContents() const",
        ):
            queue_defs.append("template <s32 BUFSIZE, s32 ALIGN>\n" + definition(queue_text, sig))
        queue_inc = (
            "namespace CgsModule {\nstruct Event {};\n"
            "template<class T, s32 N> class EventQueue;\n"
            + statement(queue_text, "const s32 KI_MAX_EVENT_TYPES_IN_QUEUE ") + "\n"
            + definition(queue_text, "class BaseVariableEventQueue") + ";\n"
            "template <s32 BUFSIZE, s32 ALIGN>\n"
            + definition(queue_text, "class VariableEventQueue") + ";\n"
            "namespace detail { const char* const KAC_VEQ_FILE = \"CgsVariableEventQueue.h\"; }\n"
            + "\n\n".join(queue_defs) + "\n}\n"
        )

        action_line = re.search(
            r"\bE_ACTION_PROGRESSION_PROFILE_DATA\s*=\s*(\d+)", read(ACTIONS))
        if action_line is None:
            raise ValueError("canonical profile action id missing")
        events_inc = (
            "namespace BrnGameState { namespace GameStateModuleIO {\n"
            "enum EGameActionType { E_ACTION_PROGRESSION_PROFILE_DATA = "
            + action_line.group(1) + " };\n"
            "template<EGameActionType T>\n" + body(ACTIONS, "struct GameAction") + ";\n"
            + body(ACTIONS, "struct ProgressionProfileDataAction") + ";\n}}\n"
            "namespace CgsGui {\ntemplate<s32 EventTypeId>\n"
            + body(GUI_EVENT_H, "struct GuiEvent :") + ";\n"
            + body(INTERFACE, "struct RegisterEventRecord") + ";\n"
            + statement(read(INTERFACE), "const s32 KI_REGISTER_RECORD_SIZE ")
            + "\n}\nnamespace BrnGui {\n"
            + body(EVENTS_H, "struct GuiEventProgressionProfileData") + ";\n"
            + body(GUI, "struct GuiEventCache :") + ";\n"
            + body(GUI, "struct RegisterEventRecord\n") + ";\n"
            + body(INTRO, "struct GuiEventProfilePointer :") + ";\n"
            + body(INTRO, "struct GuiEventControllerAction :") + ";\n}\n"
        )

        core = body(GUI, "void GuiModule::Update(BrnResource::GameDataIO::InputBuffer*")
        clean_core = code_only(core)
        handoff_match = re.search(
            r"\{\s*GuiEventCache lCacheEvent;\s*"
            r"lCacheEvent\.mpGuiCache = &mGuiCache;\s*"
            r"RouteEventToFlow\(reinterpret_cast<const CgsModule::Event\*>\(&lCacheEvent\),"
            r"\s*64,\s*static_cast<s32>\(sizeof\(lCacheEvent\)\)\);\s*\}",
            core)
        if handoff_match is None:
            raise ValueError("unique actual cache handoff block missing")
        handoff = handoff_match[0]
        if len(re.findall(r"RouteEventToFlow\(reinterpret_cast<const CgsModule::Event\*>\(&lCacheEvent\),", core)) != 1:
            raise ValueError("cache observer handoff is not unique")
        first = core.index("mBurnoutSkillsManager.Update();")
        last = core.index("mOverlayInQueue.Clear();", first) + len("mOverlayInQueue.Clear();")
        frame = core[first:last]
        if handoff not in frame:
            raise ValueError("selected frame slice does not contain the actual pre-flow handoff")
        effective_frame = frame.replace(handoff, "", 1) if args.old_tail_cache else frame
        tail = handoff if args.old_tail_cache else ""
        # The control relocates the SAME current-source block; no old revision or
        # synthetic dispatch implementation can satisfy the numeric negative gate.

        dispatch = body(GUI, "void GuiModule::DispatchInboundGuiEvents()")
        routing = body(GUI, "void GuiModule::RouteEventToFlow(")
        drain = body(GUI, "void GuiModule::DrainFlowOutputQueue(")
        reg_start = drain.index("case 34:")
        reg_end = drain.index("case 36:", reg_start)
        registration_arm = drain[reg_start:reg_end]

        producer = body(GSM, "void GameStateModule::PreWorldUpdate(")
        start = producer.index("GameStateModuleIO::ProgressionProfileDataAction lProfileData;")
        end = producer.index("sizeof(lProfileData));", start) + len("sizeof(lProfileData));")
        producer_fragment = producer[start:end]
        translator_source = read(TRANSLATOR)
        case_start = translator_source.index("case BrnGameState::GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA:")
        case_end = translator_source.index("break;", case_start) + len("break;")
        translator_arm = translator_source[case_start:case_end]
        push = "template<class GuiEventT>\n" + body(PUSH_H, "static void PushGuiEvent(")

        # Actual data declarations, shared only where both original TUs use the same
        # table/format constants. Namespace coalescing is a fixture boundary.
        license_constants = []
        for signature in (
            "static const f32 KF_REQUIRED_WINS_TICK_UP_RATE",
            "static const CgsGui::sResourceTuple KA_LICENSE_RESOURCES_AVAILABLE",
            "static const CgsGui::sResourceTuple KA_ELITE_LICENSE_RESOURCE",
            "static const CgsGui::sResourceTuple KA_FINISHED_ELITE_LICENSE_RESOURCE",
            "static const s32 KI_MAX_REQUIRED_RANKS",
            "static const char KAC_UPGRADE_STRINGID",
            "static const char KAC_ONE_UPGRADE_STRINGID",
            "static const char KAC_ELITE_UPGRADE_STRINGID",
            "static const char KAC_ONE_ELITE_UPGRADE_STRINGID",
            "static const char KAC_DIRT_CONTROL_VAR",
            "static const char KAC_PLAYERNAME_TEXTFIELD_NAME",
            "static const char KAC_NEXT_RANK_TEXTFIELD_NAME",
            "static const char KAC_DATE_ISSUED_TEXTFIELD_NAME",
            "static const char KAC_COMPLETIONMONTHFIELD_NAME",
            "static const char KAC_COMPLETIONDATEFIELD_NAME",
            "static const char KAC_COMPLETIONYEARFIELD_NAME",
            "const char KAC_UPGRADE_PENDING_TEXT",
            "const s32 KI_CHANNEL_GUI_OUT",
            "const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_ID_LOOKUP",
            "const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_INTEGER_FIELD",
            "const CgsLanguage::LanguageManager::ParameterFormatType KE_FORMAT_PARAM_WINS",
        ):
            license_constants.append(statement(read(LICENSE), signature))
        intro_constants = []
        for signature in (
            "const s32 Intro::maiEventToObserve",
            "const s32 Intro::miNumEventsObserved",
            "const char Intro::KAC_SCREENANIM_NAME",
            "const char Intro::KAC_LICENSECOMP_NAME",
            "const char Intro::KAC_PHOTOBOOTHCOMP_NAME",
            "const char Intro::KAC_WELCOMETEXT1COMP_NAME",
            "const char Intro::KAC_WELCOMETEXT2COMP_NAME",
            "const char Intro::KAC_MONTHTEXTFIELD_NAME",
            "const char Intro::KAC_DAYTEXTFIELD_NAME",
            "const char Intro::KAC_YEARTEXTFIELD_NAME",
            "const s32 KI_EVENT_CONTROLLER",
            "const s32 KI_EVENT_APT_TRIGGER",
            "const s32 KI_EVENT_GUI_CACHE",
            "const s32 KI_EVENT_PROFILE",
            "const s32 KI_EVENT_VOICEOVER_STARTED",
            "const s32 KI_EVENT_VOICEOVER_STOPPED",
            "const s32 KI_EVENT_CAM_PIC",
            "const s32 KI_CHANNEL_GUI_INTERNAL",
            "const s32 KI_NUM_MONTH_STRINGIDS",
            "const s32 KI_DATE_SCRATCH_LEN",
            "const char* const KAPC_MONTH_STRINGIDS",
            "const CgsLanguage::LanguageManager::ParameterFormatType KE_MONTH_TEXT_FORMAT",
        ):
            intro_constants.append(statement(read(INTRO), signature))
        intro_months = re.findall(r'"([^"\\]*)"',
            statement(read(INTRO), "const char* const KAPC_MONTH_STRINGIDS"))
        license_months = re.findall(r'"([^"\\]*)"',
            statement(read(LICENSE), "const char* const KAPC_MONTH_STRINGIDS"))
        if intro_months != license_months or len(intro_months) != 12:
            raise ValueError("fixture namespace coalescing requires identical native month tables")

        post_record = "template<s32 N>\n" + body(INTRO, "struct GuiCommandEvent16")
        post = "template<s32 N>\n" + body(INTRO, "void PostCommand16(")
        license_body = body(LICENSE, "void LicenseComponent::SetPlayerInfo(")
        recovered_license = license_body.replace("LicenseComponent::SetPlayerInfo(",
                                                 "LicenseComponent::SetPlayerInfoRecovered(", 1)
        methods = [
            body(LICENSE, "void LicenseComponent::Construct("),
            body(LICENSE, "void LicenseComponent::SetCachePointer("),
            body(LICENSE, "void LicenseComponent::SetProfilePointer("),
            recovered_license, body(LICENSE, "void LicenseComponent::UpdateDirt()"),
            body(INTRO, "void Intro::OnEnter()"),
            body(INTRO, "void Intro::HandleIncomingEvents()"),
            routing, dispatch,
        ]
        bodies = (
            "namespace CgsGui {\n" + body(INTERFACE, "void StateInterface::RegisterForEvents(")
            + "\n}\nnamespace BrnGui {\n"
            + "\n".join(license_constants + intro_constants)
            + "\ntypedef CgsModule::VariableEventQueue<18432,16> InGuiEventQueue;\n"
            + post_record + ";\n" + post + "\n" + "\n\n".join(methods)
            + "\nvoid GuiModule::RecoveredFrameSlice() {\n" + effective_frame + "\n}\n"
            + "void GuiModule::RecoveredTailControl() {\n" + tail + "\n}\n"
            + """void GuiModule::FixtureCommitRegistrationOutput(CgsGui::StateInterface* iface) {
const s32 liFlow = E_GUIFLOW_SCREEN;
auto* queue = iface->GetOutputEventQueue();
const CgsModule::Event* lpEvent = nullptr; s32 liSize = 0;
s32 liId = queue->GetFirstEvent(&lpEvent, &liSize);
while (liId >= 0 && lpEvent != nullptr) {
    switch(liId) {
""" + registration_arm + """
    default: break;
    }
    const CgsModule::Event* next = nullptr;
    liId = queue->GetNextEvent(lpEvent, &next, &liSize); lpEvent = next;
}
queue->Clear();
}
}
namespace BrnGameState {
void FixturePublishProfile(ProgressionBoundary& mProgressionManager,
CgsModule::VariableEventQueue<13312,16>* lpActionQueue) {
""" + producer_fragment + "\n}\n}\nnamespace BrnGame {\n" + push + """
void FixtureTranslateProfile(const CgsModule::Event* lpAction,s32 id,
CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput) {
switch(id) {
""" + translator_arm + "\ndefault: break;\n}\n}\n}\n"
        )

        bounds = "\n".join(statement(read(GUI_H), "static const s32 " + key + " ")
                           for key in ("KI_NUM_EVENT_OBSERVERS", "KI_MAX_OBSERVED_EVENT_ID",
                                       "KI_MAX_PRIORITY_CLAIMS_PER_FLOW"))
        extra_files = {
            "license_handoff_queue.inc": queue_inc,
            "license_handoff_events.inc": events_inc,
            "license_handoff_license_enum.inc": statement(read(LICENSE_H), "static const s32 KI_MAX_RANK ")
                + "\n" + body(LICENSE_H, "enum ELicenseStates") + ";",
            "license_handoff_intro_enum.inc": body(INTRO_H, "enum EIntroState") + ";",
            "license_handoff_profile_getter.inc":
                body(PROFILE_H, "bool GetHaveSet100PercentCompletedDate() const"),
            "license_handoff_module_bounds.inc": bounds,
        }
        profile_branch = code_only(license_body)
        intro_handler = code_only(methods[6])
        wiring = [
            ("current-frame inbound profile precedes one cache handoff before flows",
             in_order(clean_core, ["DispatchInboundGuiEvents();",
                 "RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64,",
                 "mScreenFlow.Update();", "mScreenInQueue.Clear();",
                 "DrainFlowOutputQueue(E_GUIFLOW_SCREEN);"])),
            ("actual producer retains owned pointer/pointer/availability action 193",
             action_line.group(1) == "193" and in_order(code_only(producer_fragment),
                 ["lProfileData.mpProgressionProfile = mProgressionManager.GetProfile();",
                  "lProfileData.mpProgressionData = mProgressionManager.GetProgressionData();",
                  "lProfileData.mbAreRoadRulesAvailable = mProgressionManager.AreRoadRulesAvailable();",
                  "GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA, sizeof(lProfileData)"])),
            ("actual translator uses canonical GUI payload and shared publisher",
             all(s in code_only(translator_arm) for s in
                 ["lEvent.mpProfile = lrAction.mpProgressionProfile;",
                  "lEvent.mpProgressionData = lrAction.mpProgressionData;",
                  "lEvent.mbRoadRulesAvailable = lrAction.mbAreRoadRulesAvailable;",
                  "PushGuiEvent(lEvent, lpGuiInput);"])),
            ("actual Intro binds profile and uses License at cache delivery",
             "mLicenseComponent.SetProfilePointer(mpProfile);" in intro_handler
             and "mLicenseComponent.SetPlayerInfo(mpGuiCache->GetPlayerName()," in intro_handler),
            ("native License remains unguarded and real SetProfilePointer owns binding",
             "if (mpProfile->GetHaveSet100PercentCompletedDate())" in profile_branch
             and "if (mpProfile != 0" not in profile_branch
             and "mpProfile = lpProfile;" in code_only(methods[2])),
            ("actual observer routing and registration arm retained",
             "lapQueues[lf]->AddEvent(lpEvent, liId, liSize);" in code_only(routing)
             and "mabObservedEventIds[liFlow][liType] = (liId == 34);" in code_only(registration_arm)),
            ("actual FIFO queue construction, copying and traversal compiled",
             len(queue_defs) == 9 and "memcpy(" in code_only(queue_defs[7])
             and "miFirstEventOffset" in code_only(queue_defs[5])),
            ("control relocates current handoff only, keeping the same numeric fixture",
             handoff in frame and (not args.old_tail_cache or
                 handoff not in effective_frame and tail == handoff)),
        ]
    except (ValueError, OSError) as error:
        print("SETUP: " + str(error))
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            receipt = {"tag": TAG, "process_exit_code": None, "numeric": None,
                       "setup_error": str(error), "negative_control":
                           "old-tail-cache" if args.old_tail_cache else None}
            (args.output_dir / "result.json").write_text(json.dumps(receipt, indent=2)+"\n")
        return 2

    numeric = compile_and_run(
        Path(__file__).with_name("GuiLicenseIntroHandoff.cpp"),
        "license_handoff_bodies.inc", bodies, TAG,
        extra_files=extra_files, output_dir=args.output_dir)
    if args.output_dir and (args.output_dir / "result.json").is_file():
        receipt_path = args.output_dir / "result.json"
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        receipt.update({
            "fixture_revision": args.rev or "working-tree",
            "negative_control": "old-tail-cache" if args.old_tail_cache else None,
            "production_inputs": [
                {"path": path, "sha256_utf8_text": hashlib.sha256(text.encode()).hexdigest()}
                for path, text in sorted(inputs.items())],
            "wiring": [{"label": label, "pass": bool(ok)} for label, ok in wiring],
            "numeric_expected": NUMERIC_TOTAL,
            "intended_negative_witness": WITNESS if args.old_tail_cache else None,
            "boundaries": "Large collaborators/calendar/profile/VM/FSM shell; real queue, "
                "provider/translation statements, module routing/dispatch, Intro/License bodies; "
                "test wrapper rejects unbound member calls before delegating to the unguarded body",
        })
        receipt_path.write_text(json.dumps(receipt, indent=2)+"\n", encoding="utf-8")
    if numeric is None:
        return 2
    if numeric[0] != NUMERIC_TOTAL:
        print("SETUP: executed numeric count differs from the frozen 64-check contract")
        return 2
    return report("GUI license intro handoff", wiring, numeric, NUMERIC_TOTAL)


if __name__ == "__main__":
    raise SystemExit(main())
