"""Hosted selected-frame License intro handoff, with actual production bodies.

No game is started. --old-tail-cache restores only the old placement in the current
source; it must execute the same 64 checks and report the binding-before-use witness.
The exact production Profile-350 dispatch arm and common observer fan-out, full Intro
handler, and unguarded License body run against explicit profile/calendar/VM/
collaborator/FSM boundaries. This is not a full GUI/ABI fixture.
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
INTERFACE_H = "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
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
        # The retained-inbox route calls the genuine packed-byte Append. Its
        # const source accessors must be emitted beside the other real queue bodies.
        for sig in (
            "s32 VariableEventQueue<BUFSIZE, ALIGN>::GetSizeInBytes() const",
            "const char* VariableEventQueue<BUFSIZE, ALIGN>::GetFirstWritePointer() const",
        ):
            queue_defs.append("template <s32 BUFSIZE, s32 ALIGN>\n" + definition(queue_text, sig))
        queue_defs.append(
            "template <s32 BUFSIZE, s32 ALIGN>\n"
            "template <s32 SRCBUF, s32 SRCALIGN>\n"
            + definition(queue_text,
                "bool VariableEventQueue<BUFSIZE, ALIGN>::Append("
                "const VariableEventQueue<SRCBUF, SRCALIGN>& lrSource)"))
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
            + body(INTERFACE_H, "struct GuiEventRegisterForEvents") + ";\n"
            + "\n}\nnamespace BrnGui {\n"
            + body(EVENTS_H, "struct GuiEventProgressionProfileData") + ";\n"
            + body(GUI, "struct GuiEventCache :") + ";\n"
            + body(GUI, "struct RegisterEventRecord\n") + ";\n"
            + body(INTRO, "struct GuiEventProfilePointer :") + ";\n"
            + body(INTRO, "struct GuiEventControllerAction :") + ";\n}\n"
        )

        core = body(GUI, "void GuiModule::Update(BrnResource::GameDataIO::InputBuffer*")
        clean_core = code_only(core)

        # Extract the current retained-inbox append/retirement and the current
        # Profile-350/cache64 input-cursor handoff from this same Update body.
        inbox_start = core.index("if (mpGuiEventInputBuffer != 0)")
        inbox_unlock_end = core.index("mpGuiEventInputBuffer->UnlockForWrite();", inbox_start)
        inbox_end = core.index("}", inbox_unlock_end) + 1
        input_inbox = core[inbox_start:inbox_end]
        input_dispatch_setup = "\n".join(statement(core, signature) for signature in (
            "bool lbHasSaveImageEvent = false;",
            "CgsGui::ImageFileInfo lSaveImageInfo;",
            "const CgsModule::Event* lpLastInboundEvent =",
        ))
        cache_decl = statement(core, "GuiEventCache lCacheEvent;")
        cache_pointer = statement(core, "lCacheEvent.mpGuiCache = &mGuiCache;")
        cache_pointer_end = core.index("lCacheEvent.mpGuiCache = &mGuiCache;") + len("lCacheEvent.mpGuiCache = &mGuiCache;")
        cache_manager = statement(core[cache_pointer_end:], "mCustomRendererManager.RecvEvent(")
        cache_manager_end = core.index("mCustomRendererManager.RecvEvent(", cache_pointer_end)
        cache_guard_start = core.index("if (mpGuiEventInputBuffer != 0)", cache_manager_end)
        cache_source = core[cache_guard_start:]
        cache_lock = statement(cache_source, "mpGuiEventInputBuffer->LockForWrite();")
        cache_queue = statement(cache_source,
            "CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpGuiEvents =")
        cache_add_match = re.search(
            r"if \(lpGuiEvents->AddEvent\(reinterpret_cast<const CgsModule::Event\*>\(&lCacheEvent\),\s*"
            r"64,\s*static_cast<s32>\(sizeof\(lCacheEvent\)\)\)\)\s*\{\s*"
            r"s32 liCacheEventSize = 0;\s*"
            r"if \(lpLastInboundEvent != 0\)\s*"
            r"lpGuiEvents->GetNextEvent\(lpLastInboundEvent, &lpLastInboundEvent, &liCacheEventSize\);\s*"
            r"else\s*lpGuiEvents->GetFirstEvent\(&lpLastInboundEvent, &liCacheEventSize\);\s*\}",
            cache_source)
        if cache_add_match is None:
            raise ValueError("cache64 common-input insertion/cursor source block is missing")
        cache_unlock = statement(cache_source, "mpGuiEventInputBuffer->UnlockForWrite();")
        cache_resume = statement(cache_source,
            "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, lpLastInboundEvent);")
        input_route = (
            "void GuiModule::RecoveredInputCacheRoute() {\n"
            + input_inbox + "\n"
            + input_dispatch_setup + "\n"
            + cache_decl + "\n" + cache_pointer + "\n" + cache_manager + "\n"
            + "if (mpGuiEventInputBuffer != 0) {\n"
            + cache_lock + "\n" + cache_queue + "\n" + cache_add_match[0] + "\n"
            + cache_unlock + "\n"
            # Unrelated debug-refresh calls between cursor step and resume are fixture boundaries.
            + cache_resume + "\n}\n}\n"
        )
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
        # Use the real cache handoff and following flow update as the observation slice.
        # The current Update's retained inbox, profile dispatch and cache64 cursor now precede it.
        flow_start = core.index("mHudFlow.Update();", handoff_match.end())
        flow_end = core.index("mOverlayInQueue.Clear();", flow_start) + len("mOverlayInQueue.Clear();")
        frame = core[handoff_match.start():flow_end]
        if handoff not in frame:
            raise ValueError("selected frame slice does not contain the actual pre-flow handoff")
        effective_frame = frame.replace(handoff, "", 1) if args.old_tail_cache else frame
        tail = handoff if args.old_tail_cache else ""
        # The control relocates the SAME current-source block; no old revision or
        # synthetic dispatch implementation can satisfy the numeric negative gate.

        dispatch_source = body(GUI, "const CgsModule::Event* GuiModule::DispatchInboundGuiEvents(")
        dispatch_declaration = statement(
            read(GUI_H), "const CgsModule::Event* DispatchInboundGuiEvents(")
        dispatch_cursor_contract = (
            "lpAfterEvent = 0" in code_only(dispatch_declaration)
            and in_order(code_only(dispatch_source), [
                "const CgsModule::Event* lpLastDispatchedEvent = lpAfterEvent;",
                "lpAfterEvent != 0",
                "lpInQueue->GetNextEvent(lpAfterEvent, &lpEvent, &liSize)",
                "lpInQueue->GetFirstEvent(&lpEvent, &liSize)",
                "lpLastDispatchedEvent = lpEvent;",
                "lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize)",
                "return lpLastDispatchedEvent;",
            ])
        )
        profile_case_labels = list(re.finditer(r"(?m)^[ \t]*case 350:", dispatch_source))
        if len(profile_case_labels) != 1:
            raise ValueError("native profile-350 dispatch arm is not unique")
        profile_case_start = profile_case_labels[0].start()
        # The actual350 arm falls through to a shared switch break. GuiCache
        # delivery is now after that switch, not inside the350 arm.
        profile_case_end = dispatch_source.index("break;", profile_case_start) + len("break;")
        profile_case = dispatch_source[profile_case_start:profile_case_end]
        cache_tail_matches = list(re.finditer(
            r"if \(liId != 14 && liId != 16\)\s*"
            r"mGuiCache\.RecEvent\(lpEvent, liId\);", dispatch_source))
        manager_tail_matches = list(re.finditer(
            r"if \(liId != 14 && liId != 16\)\s*"
            r"mCustomRendererManager\.RecvEvent\(lpEvent, liId\);", dispatch_source))
        if len(cache_tail_matches) != 1 or len(manager_tail_matches) != 1:
            raise ValueError("actual guarded cache/renderer common tails are not unique")
        cache_tail = cache_tail_matches[0]
        manager_tail = manager_tail_matches[0]
        if not profile_case_end < cache_tail.start() < cache_tail.end() < manager_tail.start():
            raise ValueError("Profile350/shared-break/cache/renderer source order changed")
        dispatch_tail_end = dispatch_source.index(
            "const CgsModule::Event* lpNext = 0;", manager_tail.end())
        dispatch_tail = cache_tail[0] + "\n" + dispatch_source[manager_tail.start():dispatch_tail_end]
        # Unrelated keyboard/save-image/platform arms and the separate tracker/
        # passive-witness block remain fixture boundaries. The actual selected
        # cache guard, renderer guard and observer fan-out are copied unchanged;
        # the one-record queue walk remains the existing test boundary.
        dispatch = (
            "const CgsModule::Event* GuiModule::DispatchInboundGuiEvents(\n"
            "    bool& rbHasSaveImageEvent, CgsGui::ImageFileInfo& lrSaveImageInfo,\n"
            "    const CgsModule::Event* lpAfterEvent) {\n"
            "    (void)rbHasSaveImageEvent; (void)lrSaveImageInfo;\n"
            "    if (mpGuiEventInputBuffer == nullptr) return lpAfterEvent;\n"
            "    mpGuiEventInputBuffer->LockForRead();\n"
            "    const auto* lpInQueue = mpGuiEventInputBuffer->GetGuiEvents();\n"
            "    const CgsModule::Event* lpEvent = nullptr;\n"
            "    s32 liSize = 0;\n"
            "    const CgsModule::Event* lpLastDispatchedEvent = lpAfterEvent;\n"
            "    s32 liId = lpAfterEvent != nullptr\n"
            "        ? lpInQueue->GetNextEvent(lpAfterEvent, &lpEvent, &liSize)\n"
            "        : lpInQueue->GetFirstEvent(&lpEvent, &liSize);\n"
            "    while (liId >= 0 && lpEvent != nullptr) {\n"
            "        switch (liId) {\n"
            + profile_case
            + "\n            default: break;\n"
            + "        }\n"
            + dispatch_tail
            + "        lpLastDispatchedEvent = lpEvent;\n"
            + "        const CgsModule::Event* lpNext = nullptr;\n"
            + "        liId = lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize);\n"
            + "        lpEvent = lpNext;\n"
            + "    }\n"
            + "    mpGuiEventInputBuffer->UnlockForRead();\n"
            + "    return lpLastDispatchedEvent;\n"
            + "}\n"
        )
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
        translator_arm = definition(
            translator_source,
            "case BrnGameState::GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA:")
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
            + "\n" + input_route
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
            ("retained output inbox is drained before Profile-350 and cache64 use the common caller input",
             in_order(code_only(input_route), [
                 "lpGuiEvents->Append<18432, 16>(mStoredGuiEvents);",
                 "mStoredGuiEvents.Clear();",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo);",
                 "mCustomRendererManager.RecvEvent(",
                 "lpGuiEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent),",
                 "lpGuiEvents->GetNextEvent(lpLastInboundEvent, &lpLastInboundEvent, &liCacheEventSize);",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, lpLastInboundEvent);",
             ])),
            ("actual source orders retained batch, profile dispatch, cache64 cursor and observer handoff before the flow tick",
             in_order(clean_core, [
                 "lpGuiEvents->Append<18432, 16>(mStoredGuiEvents);",
                 "mStoredGuiEvents.Clear();",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo);",
                 "lCacheEvent.mpGuiCache = &mGuiCache;",
                 "mCustomRendererManager.RecvEvent(",
                 "lpGuiEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent),",
                 "lpGuiEvents->GetNextEvent(lpLastInboundEvent, &lpLastInboundEvent, &liCacheEventSize);",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, lpLastInboundEvent);",
                 "RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64,",
                 "mHudFlow.Update();",
             ])),
            ("actual Profile-350 dispatch arm binds profile/cache and reaches subscribed flow",
             "mProfileHost.GetProfileManager().SetProgressionProfile" in code_only(profile_case)
             and "mGuiCache.RecEvent(lpEvent, liId);" not in code_only(profile_case)
             and in_order(code_only(dispatch_tail),
                 ["if (liId != 14 && liId != 16)",
                  "mGuiCache.RecEvent(lpEvent, liId);",
                  "if (liId != 14 && liId != 16)",
                  "mCustomRendererManager.RecvEvent(lpEvent, liId);",
                  "RouteEventToFlow(lpEvent, liId, liSize);"])),
            ("dispatcher preserves the optional suffix cursor and last-event return",
             dispatch_cursor_contract),
            ("inbound Profile and cache64 cursor ordering reaches the pre-flow handoff",
             in_order(clean_core, [
                 "const CgsModule::Event* lpLastInboundEvent =",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo);",
                 "lCacheEvent.mpGuiCache = &mGuiCache;",
                 "mCustomRendererManager.RecvEvent(",
                 "mpGuiEventInputBuffer->LockForWrite();",
                 "if (lpGuiEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent),",
                 "if (lpLastInboundEvent != 0)",
                 "lpGuiEvents->GetNextEvent(lpLastInboundEvent, &lpLastInboundEvent, &liCacheEventSize);",
                 "else",
                 "lpGuiEvents->GetFirstEvent(&lpLastInboundEvent, &liCacheEventSize);",
                 "mpGuiEventInputBuffer->UnlockForWrite();",
                 "mDebugComponent.Update(mpGuiEventInputBuffer);",
                 "mSatNavDebugComponent.Update(mpGuiEventInputBuffer);",
                 "mGuiCacheDebugComponent.Update(mpGuiEventInputBuffer);",
                 "DispatchInboundGuiEvents(lbHasSaveImageEvent, lSaveImageInfo, lpLastInboundEvent);",
                 "RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64,",
                 "mHudFlow.Update();", "mScreenFlow.Update();", "mScreenInQueue.Clear();",
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
             len(queue_defs) == 12 and "memcpy(" in code_only(queue_defs[7])
             and "miFirstEventOffset" in code_only(queue_defs[5])
             and "return miBufferWritePos - miFirstEventOffset;" in code_only(queue_defs[9])
             and "return &macData[miFirstEventOffset];" in code_only(queue_defs[10])
             and in_order(code_only(queue_defs[11]), [
                 "lrSource.GetSizeInBytes();",
                 "lrSource.GetFirstWritePointer();",
                 "memcpy(&macData[miBufferWritePos], lpSrc, liSizeInBytes);",
                 "miBufferWritePos += liSizeInBytes;",
                 "miLength += lrSource.GetLength();",
             ])),
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
                "provider/translation statements, the exact production Profile-350 dispatch arm "
                "and common fan-out, the retained-inbox/profile350/cache64 input route, module routing, "
                "Intro/License bodies; debug refresh calls remain fixture boundaries; "
                "the selected event-loop wrapper handles the fixture's one-record queue; "
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
