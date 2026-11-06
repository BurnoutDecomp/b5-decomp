from pathlib import Path


worktree = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
here = Path(__file__).resolve().parent


def source(path):
    path = worktree / path
    return path.name, ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                       + path.read_text(encoding="utf-8"))


def replace(body, anchor, probe):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, probe)


name, body = source("src/GameShared/GameClasses/Fsm/CgsScriptedFsm.cpp")
body = replace(body, '''    if (lInitialStateId == 0)
        SetState(mLuaState.GetCurrentState());
    else
        SetState(lInitialStateId);''', '''    const CgsID lState = lInitialStateId == 0 ? mLuaState.GetCurrentState() : lInitialStateId;
    char lacState[KI_CGSID_STRING_LEN + 1] = {};
    CgsIDUnCompress(lState, lacState);
    char lacProbe[180];
    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R83_FSM_PREPARE fsm=%08X source=%08X size=%u state=%s\\n",
        reinterpret_cast<u32>(this), reinterpret_cast<u32>(lpLuaCodeResource),
        lpLuaCodeResource->GetSourceSize(), lacState);
    OutputDebugStringA(lacProbe);
    SetState(lState);
    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R83_FSM_CURRENT fsm=%08X state=%08X count=%d\\n",
        reinterpret_cast<u32>(this), reinterpret_cast<u32>(mpCurrentState), GetStateCount());
    OutputDebugStringA(lacProbe);''')
with (here / name).open("x", encoding="utf-8") as output:
    output.write(body)

name, body = source("src/GameSource/Gui/Flow/HUD/States/BrnBootPreload.cpp")
anchor = '        meUpdateStage = E_PRELOAD_WAIT_CACHE;'
body = replace(body, anchor, '''        OutputDebugStringA("R83_PRELOAD_ENTER\\n");
''' + anchor)
anchor = '''        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)'''
body = replace(body, anchor, '''        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        static s32 siLastStage = -1;
        static GuiCache* spLastCache = 0;
        static u32 suUpdates = 0;
        ++suUpdates;
        if (siLastStage != meUpdateStage || spLastCache != mpGuiCache || (suUpdates & 255u) == 0)
        {
            siLastStage = meUpdateStage;
            spLastCache = mpGuiCache;
            char lacProbe[164];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R83_PRELOAD_UPDATE update=%u stage=%d cache=%08X queue=%08X\\n",
                suUpdates, siLastStage, reinterpret_cast<u32>(mpGuiCache),
                reinterpret_cast<u32>(lpInQueue));
            OutputDebugStringA(lacProbe);
        }
        if (lpInQueue == 0)''')
anchor = '            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);'
body = replace(body, anchor, '            OutputDebugStringA("R83_PRELOAD_COMPLETE\\n");\n' + anchor)
with (here / name).open("x", encoding="utf-8") as output:
    output.write(body)
