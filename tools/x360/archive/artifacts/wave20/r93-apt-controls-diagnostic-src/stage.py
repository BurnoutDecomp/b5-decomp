from pathlib import Path


here = Path(__file__).resolve().parent
root = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
source = root / "src/SDKs/EATech/include/Apt/AptMovie.cpp"
prefix = ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
          'extern bool gbTraceAptRuntime;\n')
body = source.read_text(encoding="utf-8")
start = body.index("void AptMovie::doFrameControls(")
end = body.index("void AptMovie::DoTemporaryFrameControls(", start)
before, controls, after = body[:start], body[start:end], body[end:]


def replace(anchor, replacement):
    global controls
    assert controls.count(anchor) == 1, anchor
    controls = controls.replace(anchor, replacement)


replace(
    "int nFrame)\n{",
    """int nFrame)
{
    static unsigned int suProbeMovies = 0;
    const bool lbProbe = gbTraceAptRuntime && suProbeMovies++ < 6;
    char lacProbe[180];
    if (lbProbe)
    {
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R93_CONTROLS_ENTER movie=%08X frames=%08X nFrame=%d frameCount=%d\\n",
            reinterpret_cast<uint32_t>(this), reinterpret_cast<uint32_t>(mpFrames),
            nFrame, mnFrameCount);
        OutputDebugStringA(lacProbe);
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R93_CONTROLS_FRAME commandCount=%d commands=%08X\\n",
            mpFrames[nFrame].mnCommandCount,
            reinterpret_cast<uint32_t>(mpFrames[nFrame].mpCommands));
        OutputDebugStringA(lacProbe);
    }""",
)
for type_name, phase in (("AptTimelineInitAction", "INIT"), ("AptTimelineCommand", "PLAY")):
    line = f"        {type_name}* command = static_cast<{type_name}*>(mpFrames[nFrame].mpCommands[i]);"
    replace(
        line,
        line + f"""
        const bool lbCommand = lbProbe && i < 16;
        if (lbCommand)
        {{
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R93_{phase}_COMMAND i=%d ptr=%08X\\n", i, reinterpret_cast<uint32_t>(command));
            OutputDebugStringA(lacProbe);
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe), "R93_{phase}_TAG tag=%d\\n", command->mnTag);
            OutputDebugStringA(lacProbe);
        }}""",
    )
for marker, call in (
    ("ROOT", "            AptCharacterInst* inst = pParent ? pParent->GetRootAnimation()->GetCharacterInst() : 0;"),
    ("STREAM", "            gAptActionInterpreter.runStream(command->mpStream, pParent, -1, inst);"),
    ("CLEANUP", "            gAptActionInterpreter.CleanupAfterExecution(saved);"),
    ("EXECUTE_INIT", "            anim->ExecuteInitActions(pParent, body->mi32CharacterId);"),
    ("PLACE", "            AptDispatchPlaceCommand(pDisplayList, body, pParent);"),
):
    replace(
        call,
        f'            if (lbCommand) OutputDebugStringA("R93_{marker}_BEGIN\\n");\n'
        f'{call}\n'
        f'            if (lbCommand) OutputDebugStringA("R93_{marker}_END\\n");',
    )
replace(
    "        if (command->mnTag == 8 && command->mnIndicator >= 0)",
    """        if (lbCommand && command->mnTag == 8)
        {
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R93_INIT_PAYLOAD indicator=%d stream=%08X\\n",
                command->mnIndicator, reinterpret_cast<uint32_t>(command->mpStream));
            OutputDebugStringA(lacProbe);
        }
        if (command->mnTag == 8 && command->mnIndicator >= 0)""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(prefix + before + controls + after)
