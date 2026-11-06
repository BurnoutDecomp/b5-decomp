from pathlib import Path


here = Path(__file__).resolve().parent
root = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
root /= "src/SDKs/EATech/include/Apt"
prefix = ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
          'extern bool gbTraceAptRuntime;\n')


def replace(body, anchor, replacement):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, replacement)


path = root / "AptDisplayList.cpp"
body = path.read_text(encoding="utf-8")
start = body.index("int AptDisplayList::tick(")
end = body.index("int AptDisplayList::GeneralisedProcess(", start)
before, tick, after = body[:start], body[start:end], body[end:]
tick = replace(tick, "    int nResult = 0;", """    int nResult = 0;
    static unsigned int suProbeLists = 0;
    const bool lbProbeList = gbTraceAptRuntime && suProbeLists++ < 16;
    unsigned int luProbeNodes = 0;
    if (lbProbeList) OutputDebugStringA("R92_LIST_ENTER\\n");""")
tick = replace(
    tick, "        if (bUseDepthLayerMask)", """        const bool lbProbeNode = lbProbeList && luProbeNodes++ < 12;
        if (lbProbeNode)
        {
            char lacProbe[124];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R92_LIST_NODE index=%u list=%08X node=%08X depthMask=%d\\n",
                luProbeNodes, reinterpret_cast<uint32_t>(this),
                reinterpret_cast<uint32_t>(pNode), nDepthLayerMask);
            OutputDebugStringA(lacProbe);
        }
        if (bUseDepthLayerMask)""",
)
tick = replace(
    tick, "            nResult |= pNode->tick();   // AptCIH::tick",
    """        {
            if (lbProbeNode) OutputDebugStringA("R92_LIST_CIH_BEGIN\\n");
            nResult |= pNode->tick();   // AptCIH::tick
            if (lbProbeNode) OutputDebugStringA("R92_LIST_CIH_END\\n");
        }""",
)
tick = replace(tick, "    return nResult;",
               '    if (lbProbeList) OutputDebugStringA("R92_LIST_END\\n");\n    return nResult;')
with (here / path.name).open("x", encoding="utf-8") as output:
    output.write(prefix + before + tick + after)


path = root / "AptCIH.cpp"
body = path.read_text(encoding="utf-8")
start = body.index("int AptCIH::tick()")
end = body.index("// (AptCIH::_gotoAndX", start)
before, tick, after = body[:start], body[start:end], body[end:]
tick = replace(
    tick, "int AptCIH::tick()\n{", """int AptCIH::tick()
{
    static unsigned int suProbeCih = 0;
    const bool lbProbeCih = gbTraceAptRuntime && suProbeCih++ < 16;
    if (lbProbeCih) OutputDebugStringA("R92_CIH_ENTER\\n");""",
)
tick = replace(
    tick, "    const uint32_t nFlags = pInst->mnClipActionFlags;   // v11 = v6[5]",
    """    const uint32_t nFlags = pInst->mnClipActionFlags;   // v11 = v6[5]
    if (lbProbeCih)
    {
        char lacProbe[140];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R92_CIH_SPRITE type=%u clipFlags=%08X goto=%d inst=%08X\\n",
            nType, nFlags, pInst->mnGotoFrame, reinterpret_cast<uint32_t>(pInst));
        OutputDebugStringA(lacProbe);
    }""",
)
for marker, call in (
    ("MOVIE", "    AptMovie* const pClipMovie = AptGetClipMovie(pInst);   // *(v6[1]+4) + embed"),
    ("JUMP", "            jumpToFrame(0);"),
    ("CONTROLS", "        pClipMovie->doFrameControls(&pInst->mDisplayList, this, pInst->mnGotoFrame);"),
    ("ACTIONS", "            pClipMovie->queueFrameActions(this, pInst->mnGotoFrame);"),
    ("ENTER_EVENT", "        queueClipEvents(2, gnAptActionFrameId, 1);"),
    ("CHILD", "    const int nChildTick = pInst->mDisplayList.tick(-1, 0);"),
):
    indent = call[:len(call) - len(call.lstrip())]
    tick = replace(
        tick, call,
        f'{indent}if (lbProbeCih) OutputDebugStringA("R92_CIH_{marker}_BEGIN\\n");\n'
        f'{call}\n'
        f'{indent}if (lbProbeCih) OutputDebugStringA("R92_CIH_{marker}_END\\n");',
    )
with (here / path.name).open("x", encoding="utf-8") as output:
    output.write(prefix + before + tick + after)
