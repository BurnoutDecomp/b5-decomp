"""Proposed hosted source-extracted GUI audio event wire fixture.

Do not run this from a developer workstation. It is for the hosted Windows/MSVC
fixture lane after the candidate source is integrated. The baseline and ID-only
controls must fail numerically with the expected witnesses; extraction or compile
failure is never an accepted negative control.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import posixpath
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
CANDIDATE_REV = "25d8523608e81df090ff123970cb25fbcb650708"
BASELINE_REV = "4c7c6ba2f97533315d5663d22499042cb007df31"


def resolve_layout() -> tuple[Path, Path]:
    if HERE.name == "tests":
        b5_root = HERE.parent.resolve()
        workflow = b5_root.parent
    elif HERE.name in ("gui_audio_wire_fixture_source_proposal_20261024_4c7",
                       "gui_audio_wire_fixture_corrected_controls_20261024_25d"):
        workflow = HERE.parents[2].resolve()
        b5_root = workflow / "b5-decomp"
    else:
        raise RuntimeError("Unsupported fixture location")
    if b5_root.name.lower() != "b5-decomp":
        raise RuntimeError("Resolved B5 root is not b5-decomp")
    if not (b5_root / "tests" / "fxgs_common.py").is_file():
        raise RuntimeError("Missing shared hosted fixture helper")
    if not (workflow / "tools" / "build" / "msvc_flags.txt").is_file():
        raise RuntimeError("Missing canonical workflow MSVC flags")
    return workflow, b5_root


WORKFLOW, B5_ROOT = resolve_layout()
sys.path.insert(0, str(B5_ROOT / "tests"))
from fxgs_common import Tree, definition, compile_and_run  # noqa: E402

EVENTS_CPP = "src/GameSource/Gui/BrnGuiEventTypeDefs.cpp"
EVENTS_H = "src/GameSource/Gui/BrnGuiEventTypeDefs.h"
GUI_CPP = "src/GameSource/Gui/BrnGuiModule.cpp"
SOUND_CPP = "src/GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.cpp"
CACHE_CPP = "src/GameSource/Gui/BrnGuiCache.cpp"
STATE_CPP = "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.cpp"
INTERPRETER_CPP = "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.cpp"
GUI_MODULE_SOURCE = "src/GameSource/Gui/BrnGuiModule.cpp"
STRING_UTILS_CPP = "src/GameShared/GameClasses/Core/CgsStringUtils.cpp"
STRSTREAM_CPP = "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
GUI_EVENT_QUEUE_CPP = "src/GameShared/GameClasses/Gui/CgsGuiEventQueue.cpp"
QUEUE_BASE_65536_CPP = "src/GameShared/GameClasses/Module/VariableEventQueue_65536_16.cpp"
GUI_MODULE_IO_CPP = "src/GameShared/GameClasses/Gui/CgsGuiModuleIO_OutputBuffer.cpp"
IO_BUFFER_CPP = "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"
ASSERT_CPP = "src/GameShared/GameClasses/Core/CgsAssert.cpp"

HEADER_ROOTS = (
    "src/types.hpp",
    "src/GameShared/GameClasses/Core/CgsAssert.h",
    "src/GameShared/GameClasses/Core/CgsStringUtils.h",
    "src/GameShared/GameClasses/Development/CgsStrStream.h",
    "src/GameShared/GameClasses/Development/Log/CgsLog.h",
    "src/GameShared/GameClasses/Gui/CgsGuiEvent.h",
    "src/GameShared/GameClasses/Gui/CgsGuiModuleIO.h",
    "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h",
    "src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h",
    "src/GameShared/GameClasses/Module/CgsVariableEventQueue.h",
    "src/GameShared/GameClasses/Sound/IO/CgsMessage.h",
    "src/GameSource/Gui/BrnGuiEventTypeDefs.h",
    "src/GameSource/Sound/Module/LogicModule/BrnMessageData.h",
)


def source_shadow(tree) -> dict[str, str]:
    pending = list(HEADER_ROOTS)
    visited: dict[str, str] = {}
    include_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
    while pending:
        relative = posixpath.normpath(pending.pop())
        if relative in visited:
            continue
        text = tree.read(relative)
        if not text:
            if relative in HEADER_ROOTS:
                raise ValueError("missing source header: " + relative)
            continue
        visited[relative] = text
        for include in include_re.findall(text):
            for candidate in (posixpath.normpath(posixpath.join(posixpath.dirname(relative), include)),
                              posixpath.normpath(posixpath.join("src", include))):
                if candidate.startswith("src/") and candidate not in visited and tree.read(candidate):
                    pending.append(candidate)
                    break
    return visited


def source_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def extract_case(tree, mode: str, legacy_tree=None) -> tuple[str, dict[str, str]]:
    events_cpp = tree.read(EVENTS_CPP)
    gui_cpp = tree.read(GUI_CPP)
    sound_cpp = tree.read(SOUND_CPP)
    cache_cpp = tree.read(CACHE_CPP)
    state_cpp = tree.read(STATE_CPP)
    interpreter_cpp = tree.read(INTERPRETER_CPP)
    assert_cpp = tree.read(ASSERT_CPP) if mode == "candidate" else ""
    source_pairs = [(EVENTS_CPP, events_cpp), (GUI_CPP, gui_cpp), (SOUND_CPP, sound_cpp),
                    (CACHE_CPP, cache_cpp), (STATE_CPP, state_cpp),
                    (INTERPRETER_CPP, interpreter_cpp)]
    if mode == "candidate":
        source_pairs.append((ASSERT_CPP, assert_cpp))
    for path, text in source_pairs:
        if not text:
            raise ValueError("missing production source: " + path)

    action_match = re.search(
        r"static const char\* const KAPC_GUI_AUDIO_ACTION_NAMES\[27\]\s*=\s*\{[\s\S]*?\};",
        events_cpp)
    if not action_match:
        raise ValueError("missing production audio action-name table")
    construct = definition(events_cpp, "void GuiAudioTriggerEvent::Construct(")
    construct_string_action = ""
    if mode in ("candidate", "id_only"):
        construct_string_action = definition(
            events_cpp, "void GuiAudioTriggerEvent::Construct(const char* lpComponentName,")
        if construct_string_action.count("GuiAudioTriggerEvent::Construct") != 1:
            raise ValueError("cannot isolate exact four-string GUI audio constructor")
    callback = definition(gui_cpp, "void GuiModule::FlaptSoundTriggerCallback(")
    legacy_type = legacy_construct = legacy_callback = ""
    legacy_hashes = {}
    if mode == "id_only":
        if legacy_tree is None:
            raise ValueError("ID-only fixture needs the exact baseline producer tree")
        legacy_header = legacy_tree.read(EVENTS_H)
        legacy_events_cpp = legacy_tree.read(EVENTS_CPP)
        legacy_gui_cpp = legacy_tree.read(GUI_CPP)
        old_type_match = re.search(
            r"struct GuiAudioTriggerEvent : public CgsGui::GuiEvent<201>\s*\{[\s\S]*?^\};",
            legacy_header, re.MULTILINE)
        legacy_action_match = re.search(
            r"static const char\* const KAPC_GUI_AUDIO_ACTION_NAMES\[27\]\s*=\s*\{[\s\S]*?\};",
            legacy_events_cpp)
        if not old_type_match or not legacy_action_match:
            raise ValueError("missing exact old producer type/action table for ID-only control")
        if legacy_action_match.group(0) != action_match.group(0):
            raise ValueError("baseline and candidate action tables differ; ID-only control is ambiguous")
        legacy_type = old_type_match.group(0).replace(
            "GuiAudioTriggerEvent", "LegacyGuiAudioTriggerEventIdOnly")
        old_construct_declaration = "    void Construct(s32 leAction, const char* lpComponentName,"
        if legacy_type.count(old_construct_declaration) != 1:
            raise ValueError("cannot locate baseline producer constructor declaration")
        legacy_type = legacy_type.replace(
            old_construct_declaration,
            "    s32 GetEventType() const { return 457; } // ID-only producer change\n" +
            old_construct_declaration, 1)
        legacy_type += '''
static_assert(sizeof(LegacyGuiAudioTriggerEventIdOnly) == 112 &&
              offsetof(LegacyGuiAudioTriggerEventIdOnly, macComponent) == 12 &&
              offsetof(LegacyGuiAudioTriggerEventIdOnly, meAction) == 44 &&
              offsetof(LegacyGuiAudioTriggerEventIdOnly, macLabel) == 48 &&
              offsetof(LegacyGuiAudioTriggerEventIdOnly, macMovie) == 80,
              "ID-only control preserves the exact old 112-byte producer layout");
'''
        legacy_construct = definition(
            legacy_events_cpp, "void GuiAudioTriggerEvent::Construct(")
        if legacy_construct.count("GuiAudioTriggerEvent::Construct") != 1:
            raise ValueError("cannot rename the exact baseline constructor body")
        legacy_construct = legacy_construct.replace(
            "GuiAudioTriggerEvent::Construct", "LegacyGuiAudioTriggerEventIdOnly::Construct", 1)
        legacy_callback = definition(
            legacy_gui_cpp, "void GuiModule::FlaptSoundTriggerCallback(")
        if legacy_callback.count("void GuiModule::FlaptSoundTriggerCallback(") != 1:
            raise ValueError("cannot rename the exact baseline callback body")
        legacy_callback = legacy_callback.replace(
            "void GuiModule::FlaptSoundTriggerCallback(",
            "void LegacyGuiModule::FlaptSoundTriggerCallbackIdOnly(", 1)
        legacy_cast = "GuiModule* lpThis = static_cast<GuiModule*>(lpUserData);"
        if legacy_callback.count(legacy_cast) != 1:
            raise ValueError("cannot isolate the baseline callback receiver type")
        legacy_callback = legacy_callback.replace(legacy_cast,
            "LegacyGuiModule* lpThis = static_cast<LegacyGuiModule*>(lpUserData);", 1)
        legacy_callback = legacy_callback.replace(
            "GuiAudioTriggerEvent", "LegacyGuiAudioTriggerEventIdOnly")
        legacy_hashes = {
            "source.baseline_type_declaration": source_hash(old_type_match.group(0)),
            "source.baseline_constructor": source_hash(
                definition(legacy_events_cpp, "void GuiAudioTriggerEvent::Construct(")),
            "source.baseline_callback": source_hash(
                definition(legacy_gui_cpp, "void GuiModule::FlaptSoundTriggerCallback(")),
            "control.legacy_type_after_only_id_override": source_hash(legacy_type),
            "control.legacy_constructor_renamed_only": source_hash(legacy_construct),
            "control.legacy_callback_renamed_and_type_substituted": source_hash(legacy_callback),
        }
    assert_provider = ""
    if mode == "candidate":
        if not assert_cpp:
            raise ValueError("missing actual CgsAssert.cpp provider")
        message_buffer_storage = re.search(
            r"namespace\s*\{\s*(char gacMessageBuffer\[KI_MESSAGEBUFFERSIZE\] = \{\};)\s*\}",
            assert_cpp)
        deferred_assert_flag = re.search(
            r"thread_local bool gbDeferredAssertPC = false;", assert_cpp)
        message_buffer_globals = re.search(
            r"char\* gpcMessageBuffer = gacMessageBuffer;\s*s32 giMessageBufferSize = KI_MESSAGEBUFFERSIZE;",
            assert_cpp)
        if not message_buffer_storage or not deferred_assert_flag or not message_buffer_globals:
            raise ValueError("cannot bind MessageBufferPC backing state from CgsAssert.cpp")
        message_buffer_constructor = definition(assert_cpp, "MessageBufferPC::MessageBufferPC()")
        assert_provider = f'''namespace CgsDev {{
namespace Assert {{
namespace {{
{message_buffer_storage.group(1)}
{deferred_assert_flag.group(0)}
}}
{message_buffer_globals.group(0)}
{message_buffer_constructor}
}}
}}
'''

    state_construct = definition(state_cpp, "void StateInterface::Construct()")
    get_output_queue = definition(state_cpp, "GuiStackEventQueue::GuiEventQueueLarge* StateInterface::GetOutputEventQueue()")

    state_specializer = ""
    if mode in ("candidate", "id_only"):
        state_specializer = "template <>\n" + definition(
            events_cpp, "void StateInterface::OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>(")
    process = definition(sound_cpp, "void SoundLogicModule::ProcessGuiEvents(")
    start = process.find("        case 201:")
    stop = process.find("        case 456:", start + 1)
    if start < 0 or stop < 0:
        raise ValueError("missing exact SoundLogic audio cases201/457")
    audio_cases = process[start:stop]
    wire_header = definition(sound_cpp, "struct GuiEventWireHeader") + ";"
    get_payload = definition(sound_cpp, "const u8* GetGuiPayload(")
    queue_sound_message = definition(sound_cpp, "template <typename T>\n    void QueueSoundMessage(")
    cache_rec_event = definition(cache_cpp, "void GuiCache::RecEvent(")
    cache_start = cache_rec_event.find("        case 201:")
    cache_stop = cache_rec_event.find("        case 77:", cache_start + 1)
    if cache_start < 0 or cache_stop < 0:
        raise ValueError("missing exact GuiCache::RecEvent case201 arm")
    cache_case_201 = cache_rec_event[cache_start:cache_stop]

    process_out_events = definition(
        interpreter_cpp, "void EventInterpreterModule::ProcessOutEvents(")
    staging_construct = (
        "CgsModule::VariableEventQueue<65536, 16> lStagingQueue;\n"
        "    lStagingQueue.Construct();")
    staging_copy_and_retire = (
        "        lStagingQueue.Append(*lpObserverOutQueue);\n"
        "        lpObserverOutQueue->CgsModule::VariableEventQueue<65536, 16>::Clear();")
    staging_read_first = "s32 liEventType = lStagingQueue.GetFirstEvent(&lpEvent, &liEventSize);"
    staging_read_next = "liEventType = lStagingQueue.GetNextEvent(lpEvent, &lpEvent, &liEventSize);"
    staging_destruct = "    lStagingQueue.Destruct();"
    for statement in (staging_construct, staging_copy_and_retire, staging_read_first,
                      staging_read_next, staging_destruct):
        if process_out_events.count(statement) != 1:
            raise ValueError("missing/ambiguous actual EventInterpreter queue step: " + statement)
    channel40_start = process_out_events.find("        case E_GUI_OUT_EVENT:")
    channel40_stop = process_out_events.find("        case E_GUI_OUT_VIEW_STATE:", channel40_start + 1)
    if channel40_start < 0 or channel40_stop < 0:
        raise ValueError("missing actual EventInterpreter channel40 branch")
    state_output_channel40_branch = process_out_events[channel40_start:channel40_stop]
    original_output_owner = "            OutputBuffer::GuiEventQueue* lpEventQueue = lpOutput->GetOutEventQueue();"
    if state_output_channel40_branch.count(original_output_owner) != 1:
        raise ValueError("cannot locate exact ProcessOutEvents channel40 output owner")
    # The projection substitutes only the output-buffer getter with its fixture queue member.
    # The native staging Append, explicit base Clear, record parse, and AddEvent call remain exact.
    state_output_channel40_branch = state_output_channel40_branch.replace(
        original_output_owner,
        "            CgsModule::VariableEventQueue<18432, 16>* lpEventQueue = &mGuiOutputQueue;", 1)
    state_output_drain = f'''struct StateOutputDrainProjection {{
    CgsModule::VariableEventQueue<18432,16> mGuiOutputQueue;
    void Drain(CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpObserverOutQueue) {{
        {staging_construct}
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        {staging_copy_and_retire}
        {staging_read_first}
        while (lpEvent != 0) {{
            switch (liEventType) {{
{state_output_channel40_branch}
            default:
                CGS_ASSERT(false, "Unexpected output event type in EventInterpreterModule::ProcessOutEvents");
                break;
            }}
            {staging_read_next}
        }}
        {staging_destruct}
    }}
}};
'''

    gui_module = tree.read(GUI_MODULE_SOURCE)
    store_statement = "mStoredGuiEvents.Append<18432, 16>(mGuiOutQueue);"
    drain_statement = "lpGuiEvents->Append<18432, 16>(mStoredGuiEvents);"
    clear_statement = "mStoredGuiEvents.Clear();"
    for statement in (store_statement, drain_statement, clear_statement):
        if gui_module.count(statement) < 1:
            raise ValueError("missing actual GUI inbox statement: " + statement)

    legacy_projection = f'''{legacy_type}
{legacy_construct}
''' if mode == "id_only" else ""
    legacy_callback_declaration = (
        "    static void FlaptSoundTriggerCallbackIdOnly(void*, const char*, const char*, const char*, const char*);\n"
        if mode == "id_only" else "")
    legacy_module_projection = f'''struct LegacyGuiModule {{
    CgsModule::VariableEventQueue<18432,16>* mpOutputBuffer;
{legacy_callback_declaration}
}};
''' if mode == "id_only" else ""
    projection = f'''namespace CgsGui {{
{state_construct}
{get_output_queue}
{state_specializer}
{state_output_drain}
}}
namespace BrnGui {{
{action_match.group(0)}
{legacy_projection}
struct GuiModule {{
#if defined(AUDIO_MODE_BASELINE)
    CgsModule::VariableEventQueue<18432,16>* mpOutputBuffer;
#else
    CgsGui::CgsGuiModuleIO::OutputBuffer* mpOutputBuffer;
#endif
    static void FlaptSoundTriggerCallback(void*, const char*, const char*, const char*, const char*);
}};
{legacy_module_projection}
{construct}
{construct_string_action}
{callback}
{legacy_callback}
}}
namespace AudioFixture {{
struct QueueHandoff {{
    CgsModule::VariableEventQueue<18432,16>& mGuiOutQueue;
    CgsModule::VariableEventQueue<18432,16> mStoredGuiEvents;
    explicit QueueHandoff(CgsModule::VariableEventQueue<18432,16>& queue) : mGuiOutQueue(queue) {{}}
    void StoreOutput() {{ {store_statement} }}
    void Drain(CgsGui::GuiEventQueueBase<32768,16>* lpGuiEvents) {{
        {drain_statement}
        {clear_statement}
    }}
}};
struct DriveThroughRowProjection {{
    CgsID mDriveThruId;
    bool mbHidden;
    CgsID GetCgsId() const {{ return mDriveThruId; }}
    void SetHiddenDriveThru(bool value) {{ mbHidden = value; }}
}};
struct GuiCacheCase201Projection {{
    s32 miNumDriveThroughs;
    s32 miCase201DispatchCount;
    DriveThroughRowProjection maDriveThroughInfo[1];
    void RecEvent(const CgsModule::Event* lpEvent, s32 liEventId) {{
        if (liEventId == 201) ++miCase201DispatchCount; // fixture witness around actual switch arm
        switch (liEventId) {{
{cache_case_201}
        default: break;
        }}
    }}
}};
}}
namespace BrnSound {{ namespace Module {{ namespace {{
{wire_header}
{get_payload}
{queue_sound_message}
}}
struct SoundDecoderProjection {{
    CgsModule::VariableEventQueue<8192,16> mMessageQueue;
    void ProcessGuiEvent(s32 liType, const CgsModule::Event* lpEvent, s32 liSize) {{
        const u8* lpuPayload = GetGuiPayload(lpEvent, liType, liSize);
        switch (liType) {{
{audio_cases}
        default: break;
        }}
    }}
}};
}} }} // BrnSound::Module
{assert_provider}
'''
    hashes = {
        EVENTS_CPP: source_hash(events_cpp), GUI_CPP: source_hash(gui_cpp),
        SOUND_CPP: source_hash(sound_cpp), CACHE_CPP: source_hash(cache_cpp),
        STATE_CPP: source_hash(state_cpp), INTERPRETER_CPP: source_hash(interpreter_cpp),
        "extracted.construct": source_hash(construct),
        "extracted.construct_string_action": source_hash(construct_string_action) if construct_string_action else "not-present",
        "extracted.message_buffer_provider": source_hash(assert_provider) if assert_provider else "not-present",
        "source.cgs_assert_cpp": source_hash(assert_cpp) if assert_cpp else "not-present",
        "extracted.callback": source_hash(callback),
        "extracted.state_construct": source_hash(state_construct),
        "extracted.get_output_queue": source_hash(get_output_queue),
        "extracted.state_specializer": source_hash(state_specializer) if state_specializer else "not-present",
        "extracted.audio_action_name_table": source_hash(action_match.group(0)),
        "extracted.gui_wire_header": source_hash(wire_header),
        "extracted.get_gui_payload": source_hash(get_payload),
        "extracted.queue_sound_message": source_hash(queue_sound_message),
        "extracted.sound_process_gui_events": source_hash(process),
        "extracted.audio_case_201_457": source_hash(audio_cases),
        "extracted.queue_handoff_statements": source_hash(store_statement+drain_statement+clear_statement),
        "extracted.gui_cache_case_201": source_hash(cache_case_201),
        "extracted.event_interpreter_process_out_events": source_hash(process_out_events),
        "extracted.event_interpreter_channel40_branch": source_hash(
            process_out_events[channel40_start:channel40_stop]),
        "control.state_output_channel40_branch_projection": source_hash(
            state_output_channel40_branch),
        "control.state_output_staging_append_and_base_clear": source_hash(
            staging_construct+staging_copy_and_retire+staging_read_first+
            staging_read_next+staging_destruct),
    }
    hashes.update(legacy_hashes)
    return projection, hashes


def read_receipt(out: Path) -> dict:
    path=out/"result.json"
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}


def run_mode(tree, mode: str, out: Path, legacy_tree=None):
    generated, hashes=extract_case(tree,mode,legacy_tree=legacy_tree)
    shadow=source_shadow(tree)
    strings=tree.read(STRING_UTILS_CPP)
    strstream=tree.read(STRSTREAM_CPP)
    gui_event_queue=tree.read(GUI_EVENT_QUEUE_CPP)
    queue_base_65536=tree.read(QUEUE_BASE_65536_CPP)
    gui_module_io_output_buffer=tree.read(GUI_MODULE_IO_CPP) if mode == "candidate" else ""
    io_buffer=tree.read(IO_BUFFER_CPP) if mode == "candidate" else ""
    if not strings or not strstream or not gui_event_queue or not queue_base_65536 or \
       (mode == "candidate" and (not gui_module_io_output_buffer or not io_buffer)):
        raise ValueError("missing source formatting, GUI event-queue, or OutputBuffer lock provider")
    hashes[GUI_EVENT_QUEUE_CPP]=source_hash(gui_event_queue)
    hashes[QUEUE_BASE_65536_CPP]=source_hash(queue_base_65536)
    if mode == "candidate":
        hashes[GUI_MODULE_IO_CPP]=source_hash(gui_module_io_output_buffer)
        hashes[IO_BUFFER_CPP]=source_hash(io_buffer)
    extra_files={"audio_extracted.inc":generated,"CgsStringUtils.cpp":strings,
        "CgsStrStream.cpp":strstream,"CgsGuiEventQueue.cpp":gui_event_queue,
        "VariableEventQueue_65536_16.cpp":queue_base_65536}
    extra_sources=(out/"CgsStringUtils.cpp",out/"CgsStrStream.cpp",
        out/"CgsGuiEventQueue.cpp",out/"VariableEventQueue_65536_16.cpp")
    if mode == "candidate":
        extra_files["CgsGuiModuleIO_OutputBuffer.cpp"]=gui_module_io_output_buffer
        extra_files["CgsIOBuffer.cpp"]=io_buffer
        extra_sources+=(out/"CgsGuiModuleIO_OutputBuffer.cpp",out/"CgsIOBuffer.cpp")
    tag={"candidate":"AUDIO_CANDIDATE","baseline":"AUDIO_BASELINE","id_only":"AUDIO_ID_ONLY"}[mode]
    flags={"candidate":"/DAUDIO_MODE_CANDIDATE=1","baseline":"/DAUDIO_MODE_BASELINE=1","id_only":"/DAUDIO_MODE_ID_ONLY=1"}[mode]
    # The copied support TU has a quoted local include; resolve it through this mode's
    # revision-shadow Core directory rather than the physical checkout or shadow root.
    core_shadow_include=out/"shadow"/"GameShared"/"GameClasses"/"Core"
    result=compile_and_run(HERE/"GuiAudioTriggerWire.cpp","audio_extracted.inc",generated,tag,
            extra_flags=flags + f' /I"{core_shadow_include}"',shadow=shadow,extra_sources=extra_sources,
        extra_files=extra_files,output_dir=out)
    receipt=read_receipt(out)
    receipt["source_text_sha256"]=hashes
    receipt["shadow_header_count"]=len(shadow)
    receipt["shadow_header_sha256"]={p:source_hash(t) for p,t in sorted(shadow.items())}
    receipt["source_tree"]=getattr(tree,"rev",None) or str(getattr(tree,"root",""))
    receipt["mode"]=mode
    stdout=(out/"run.stdout.txt").read_text(encoding="utf-8",errors="replace") if (out/"run.stdout.txt").exists() else ""
    assertion_match=re.search(r"AUDIO_[A-Z_]+: \d+ checks, \d+ failures, (\d+) assertions",stdout)
    receipt["assertion_count"]=int(assertion_match.group(1)) if assertion_match else None
    (out/"result.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
    return result,receipt,stdout


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-rev",default=CANDIDATE_REV,
                        help="candidate source with the reviewed shipping-owner correction")
    parser.add_argument("--baseline-rev",default=BASELINE_REV)
    parser.add_argument("--output-dir",required=True,type=Path)
    args=parser.parse_args()
    output=args.output_dir.resolve()
    if output.exists() and any(output.iterdir()):
        print("Refusing to overwrite non-empty output directory: "+str(output),file=sys.stderr);return 2
    output.mkdir(parents=True,exist_ok=True)
    candidate=Tree(args.candidate_rev)
    baseline=Tree(args.baseline_rev)
    manifest={"fixture":"GUI audio trigger payload/wire regression","status":"starting",
      "candidate_revision":args.candidate_rev,"baseline_revision":args.baseline_rev,
      "id_only_control":"exact baseline GuiEvent<201>(0,12) producer type/constructor/callback, isolated under test-only names; only its GetEventType is changed to457 while candidate payload type, channel-40 specialization, SoundLogic decoder, and Message<100-byte-payload> remain unchanged",
      "scope":"candidate callback/constructor, exact baseline-derived ID-only producer, state wrapper, inbox queue statements, actual cache case201, and candidate SoundLogic201/457 audio cases"}
    try:
        runs={}
        for mode,tree in (("candidate",candidate),("baseline",baseline),("id_only",candidate)):
            result,receipt,stdout=run_mode(tree,mode,output/mode,
                legacy_tree=baseline if mode=="id_only" else None)
            runs[mode]={"numeric":result,"receipt":receipt,"stdout":stdout}
    except Exception as exc:
        manifest.update(status="extraction_compile_or_execution_error",error=f"{type(exc).__name__}: {exc}")
        (output/"fixture.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
        print(manifest["error"],file=sys.stderr);return 2
    manifest["runs"]={m:{"numeric":v["numeric"],"receipt":v["receipt"]} for m,v in runs.items()}
    cand=runs["candidate"];base=runs["baseline"];idc=runs["id_only"]
    candidate_ok=(cand["numeric"] is not None and cand["numeric"][1]==0 and
        cand["receipt"].get("process_exit_code")==0 and cand["receipt"].get("assertion_count")==0)
    baseline_ok=(base["numeric"] is not None and base["numeric"][1]>0 and
        base["receipt"].get("process_exit_code")==1 and
        base["receipt"].get("assertion_count")==0 and
        "EXPECTED_BASELINE_NEGATIVE id=201 size=112 cache201=1 message=128 assertions=0" in base["stdout"])
    idonly_ok=(idc["numeric"] is not None and idc["numeric"][1]>0 and
        idc["receipt"].get("process_exit_code")==1 and
        idc["receipt"].get("assertion_count")==0 and
        "EXPECTED_ID_ONLY_NEGATIVE id=457 size=112 message=116 payload_match=0 assertions=0" in idc["stdout"])
    manifest["expected_negative_control_process_exits"]={"baseline":1,"id_only":1}
    manifest["witnesses"]={"candidate_pass":candidate_ok,"baseline_negative_expected":baseline_ok,"id_only_negative_expected":idonly_ok}
    manifest["status"]="pass" if candidate_ok and baseline_ok and idonly_ok else "numeric_or_execution_failure"
    (output/"fixture.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print("Audio-wire fixture status: "+manifest["status"])
    return 0 if manifest["status"]=="pass" else 1

if __name__ == "__main__":
    raise SystemExit(main())
