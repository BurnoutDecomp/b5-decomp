from pathlib import Path


source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp")
body = source.read_text(encoding="utf-8")
body = '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n' + body
anchor = '''void MainGameFlowStateInitialLoadingScreen::Update()
{
    BrnGame::BrnGameModule* const lpGameModule = gpBurnoutGame;
'''
assert body.count(anchor) == 1
body = body.replace(anchor, anchor + '''    static s32 siLastStage = -1;
    static bool sbLastGuiDone = false;
    static u32 suUpdates = 0;
    ++suUpdates;
    if (siLastStage != meLoadingScreenStage || sbLastGuiDone != mbGuiPreloadDone ||
        (suUpdates & 255u) == 0)
    {
        siLastStage = meLoadingScreenStage;
        sbLastGuiDone = mbGuiPreloadDone;
        char lacProbe[144];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R79_LOADING update=%u stage=%d guiPreload=%d guiPhase=%d\\n",
            suUpdates, siLastStage, static_cast<s32>(mbGuiPreloadDone),
            static_cast<s32>(lpGameModule->IsGuiPhaseComplete()));
        OutputDebugStringA(lacProbe);
    }
''')
with Path(__file__).with_name(source.name).open("x", encoding="utf-8") as output:
    output.write(body)
