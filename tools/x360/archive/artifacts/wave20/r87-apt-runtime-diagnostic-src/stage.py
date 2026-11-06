from pathlib import Path


here = Path(__file__).resolve().parent
root = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")


def put(name, body):
    with (here / name).open("x", encoding="utf-8") as output:
        output.write(body)


def replace(body, anchor, replacement):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, replacement)


def source(path):
    path = root / path
    return path.name, ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                       'extern bool gbTraceAptRuntime;\n' + path.read_text(encoding="utf-8"))


apt = here.parent / "r86-apt-update-diagnostic-src/CgsAptAux.cpp"
body = apt.read_text(encoding="utf-8")
body = replace(body, "static bool sbTraceNextAptUpdate = false;",
               "static bool sbTraceNextAptUpdate = false;\nbool gbTraceAptRuntime = false;")
body = replace(body, "        AptUpdateTarget(mpAptTargetInstance, liDeltaMs, -1, 16);",
               "        gbTraceAptRuntime = lbTrace;\n"
               "        AptUpdateTarget(mpAptTargetInstance, liDeltaMs, -1, 16);\n"
               "        gbTraceAptRuntime = false;")
put(apt.name, body)

name, body = source("src/SDKs/EATech/Apt/AptUpdate.cpp")
anchor = '''    if (nBankedMs >= nMsPerFrame)
    {
        bTicked = 1;
        for (;;)
        {
            pAnim->TickIntervalTimers(nMsPerFrame);
            pAnim->mDisplayList.tick(nDepthLayerMask, 1);
            pAnim->RunActions();
            pAnim->ProcessInputs();
            gpAptTarget->mpLinker->Update();'''
body = replace(body, anchor, '''    if (gbTraceAptRuntime)
    {
        char lacProbe[160];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R87_APT_PACE elapsed=%d bank=%u period=%u root=%08X\\n",
            nElapsedMs, nBankedMs, nMsPerFrame, reinterpret_cast<u32>(pRootInst));
        OutputDebugStringA(lacProbe);
    }
    unsigned int luProbeTicks = 0;
    if (nBankedMs >= nMsPerFrame)
    {
        bTicked = 1;
        for (;;)
        {
            const bool lbProbeTick = gbTraceAptRuntime && luProbeTicks++ < 4;
            if (lbProbeTick) OutputDebugStringA("R87_APT_TIMERS_BEGIN\\n");
            pAnim->TickIntervalTimers(nMsPerFrame);
            if (lbProbeTick) OutputDebugStringA("R87_APT_TICK_BEGIN\\n");
            pAnim->mDisplayList.tick(nDepthLayerMask, 1);
            if (lbProbeTick) OutputDebugStringA("R87_APT_ACTIONS_BEGIN\\n");
            pAnim->RunActions();
            if (lbProbeTick) OutputDebugStringA("R87_APT_INPUT_BEGIN\\n");
            pAnim->ProcessInputs();
            if (lbProbeTick) OutputDebugStringA("R87_APT_FRAME_LINKER_BEGIN\\n");
            gpAptTarget->mpLinker->Update();
            if (lbProbeTick) OutputDebugStringA("R87_APT_FRAME_LINKER_END\\n");''')
anchor = '''    if (gpAptSavedInputStream)
    {
        AptUpdateReplaySavedInputs(nElapsedMs, nDepthLayerMask);'''
body = replace(body, anchor, '''    if (gbTraceAptRuntime)
        OutputDebugStringA(gpAptSavedInputStream ? "R87_APT_MODE_REPLAY\\n" : "R87_APT_MODE_LIVE\\n");
''' + anchor)
anchor = '''            gpAptTarget->mpLinker->Update();
            ++gAptInputRecorderTag;'''
body = replace(body, anchor, '''            if (gbTraceAptRuntime) OutputDebugStringA("R87_APT_FALLBACK_LINKER_BEGIN\\n");
            gpAptTarget->mpLinker->Update();
            if (gbTraceAptRuntime) OutputDebugStringA("R87_APT_FALLBACK_LINKER_END\\n");
            ++gAptInputRecorderTag;''')
put(name, body)

name, body = source("src/SDKs/EATech/include/Apt/AptLoader.cpp")
anchor = '''void AptLoader::Update()
{
    MutexAptLoader.Lock();'''
body = replace(body, anchor, '''void AptLoader::Update()
{
    static unsigned int suProbeUpdates = 0;
    const bool lbProbeLoader = gbTraceAptRuntime && suProbeUpdates++ < 8;
    unsigned int luProbeNodes = 0;
    if (lbProbeLoader) OutputDebugStringA("R87_LOADER_LOCK_BEGIN\\n");
    MutexAptLoader.Lock();
    if (lbProbeLoader) OutputDebugStringA("R87_LOADER_LOCK_END\\n");''')
anchor = '                const int32_t nState = pFile->mnState;   // [c:+0x08]\n'
body = replace(body, anchor, anchor + '''                const bool lbProbeNode = lbProbeLoader && luProbeNodes++ < 12;
                if (lbProbeNode)
                {
                    char lacProbe[140];
                    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                        "R87_LOADER_NODE state=%d file=%08X data=%08X\\n",
                        nState, reinterpret_cast<u32>(pFile), reinterpret_cast<u32>(pFile->mpData));
                    OutputDebugStringA(lacProbe);
                }
''')
anchor = '                    gAptFuncs.pfnLoadAnimation(pFile->mFileName.c_str(), file);'
body = replace(body, anchor, '''                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_CALLBACK_BEGIN\\n");
''' + anchor + '''
                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_CALLBACK_END\\n");''')
anchor = '''                    if (!AllImportsAvailable(file))
                        break;'''
body = replace(body, anchor, '''                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_IMPORTS_BEGIN\\n");
                    if (!AllImportsAvailable(file))
                        break;
                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_IMPORTS_END\\n");''')
anchor = '                    AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(pData))->Link(pData, pDataBlock);'
body = replace(body, anchor, '''                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_LINK_BEGIN\\n");
''' + anchor + '''
                    if (lbProbeNode) OutputDebugStringA("R87_LOADER_LINK_END\\n");''')
put(name, body)
