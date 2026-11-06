from pathlib import Path


here = Path(__file__).resolve().parent
source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
source /= "src/SDKs/EATech/include/Apt/AptActionRun.cpp"
prefix = ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
          'extern bool gbTraceAptRuntime;\n')
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace(
    "    const bool bTopLevel = (nLength == -1);",
    """    const bool bTopLevel = (nLength == -1);
    static unsigned int suProbeRuns = 0;
    const bool lbProbe = gbTraceAptRuntime && suProbeRuns < 6;
    const unsigned int luRun = lbProbe ? ++suProbeRuns : 0;
    unsigned int luDispatch = 0;
    char lacProbe[160];
    if (lbProbe)
    {
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe), "R94_RUN_ENTER id=%u stream=%08X length=%d\\n",
                        luRun, reinterpret_cast<uint32_t>(pStream), nLength);
        OutputDebugStringA(lacProbe);
    }""",
)
replace(
    "        pCIH->AddRef();",
    """        if (lbProbe) OutputDebugStringA("R94_RUN_ADDREF_BEGIN\\n");
        pCIH->AddRef();
        if (lbProbe) OutputDebugStringA("R94_RUN_ADDREF_END\\n");""",
)
line = '    ctx.mpScopeVariable       = getVariable(pCIH, 0, &gAptKeySuper, 1, 1, 0);  // console: the "super" resolve @ctx+0x10'
replace(line,
        '    if (lbProbe) OutputDebugStringA("R94_RUN_SUPER_BEGIN\\n");\n' + line +
        '\n    if (lbProbe) OutputDebugStringA("R94_RUN_SUPER_END\\n");')
replace(
    "            sGlobalTable[opcode](this, &ctx);",
    """            const bool lbDispatch = lbProbe && luDispatch++ < 40;
            if (lbDispatch)
            {
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R94_RUN_OP_BEGIN id=%u i=%u pc=%08X op=%02X handler=%08X top=%d base=%d\\n",
                    luRun, luDispatch, reinterpret_cast<uint32_t>(ctx.mpProgramCounter - 1),
                    static_cast<unsigned int>(opcode), reinterpret_cast<uint32_t>(sGlobalTable[opcode]),
                    mnStackTop, mnStackBase);
                OutputDebugStringA(lacProbe);
            }
            sGlobalTable[opcode](this, &ctx);
            if (lbDispatch)
            {
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R94_RUN_OP_END id=%u i=%u next=%08X top=%d\\n",
                    luRun, luDispatch, reinterpret_cast<uint32_t>(ctx.mpProgramCounter), mnStackTop);
                OutputDebugStringA(lacProbe);
            }""",
)
replace(
    "    return ctx.mpProgramCounter;",
    """    if (lbProbe)
    {
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe), "R94_RUN_RETURN id=%u pc=%08X\\n",
                        luRun, reinterpret_cast<uint32_t>(ctx.mpProgramCounter));
        OutputDebugStringA(lacProbe);
    }
    return ctx.mpProgramCounter;""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(prefix + body)
