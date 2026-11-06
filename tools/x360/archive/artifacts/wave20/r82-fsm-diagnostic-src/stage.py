from pathlib import Path


worktree = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
here = Path(__file__).resolve().parent


def source(path):
    path = worktree / path
    return path.name, ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                       + path.read_text(encoding="utf-8"))


def insert(body, anchor, probe):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, probe + anchor)


name, body = source("src/GameSource/Gui/BrnGuiFsmController.cpp")
body = insert(body, '''        if (mapFlows[liFlow] == 0)
            continue;

        switch (maeFlowLoadState[liFlow])''', '''        static s32 saiLastState[KU_NUM_FLOWS] = { -1, -1, -1 };
        if (saiLastState[liFlow] != maeFlowLoadState[liFlow])
        {
            saiLastState[liFlow] = maeFlowLoadState[liFlow];
            char lacProbe[152];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R82_FSM_STATE flow=%d stage=%d object=%08X notification=%08X\\n",
                liFlow, saiLastState[liFlow], reinterpret_cast<u32>(mapFlows[liFlow]),
                reinterpret_cast<u32>(mapLoadNotification[liFlow]));
            OutputDebugStringA(lacProbe);
        }
''')
body = insert(body, '''                        mapFlows[liFlow]->PrepareLua(lLuaCodeHandle.Get(),''', '''                        char lacProbe[152];
                        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                            "R82_FSM_LUA flow=%d id=%u handle=%08X/%08X\\n",
                            liFlow, lpLoadNotification->muLoadRequestId,
                            reinterpret_cast<u32>(lLuaCodeHandle.mpResourceMemory),
                            reinterpret_cast<u32>(lLuaCodeHandle.mpSourceEntry));
                        OutputDebugStringA(lacProbe);
''')
with (here / name).open("x", encoding="utf-8") as output:
    output.write(body)

name, body = source("src/GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.cpp")
body = insert(body, '''                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    if (!lrBundle.mbParsed &&
                        lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_LOAD &&''', '''                if (lpResponse->miEventId == 14 || lpResponse->miEventId == 15)
                {
                    char lacProbe[164];
                    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                        "R82_FSM_REPLY id=%d bank=%d handle=%08X/%08X\\n",
                        lpResponse->miEventId, lpResponse->miPoolId,
                        reinterpret_cast<u32>(lpResponse->mResourceHandle.mpResourceMemory),
                        reinterpret_cast<u32>(lpResponse->mResourceHandle.mpSourceEntry));
                    OutputDebugStringA(lacProbe);
                }
''')
with (here / name).open("x", encoding="utf-8") as output:
    output.write(body)
