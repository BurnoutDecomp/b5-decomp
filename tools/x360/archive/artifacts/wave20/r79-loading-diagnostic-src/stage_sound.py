from pathlib import Path


source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/GameSource/Sound/Module/BrnRootSoundModule.cpp")
body = source.read_text(encoding="utf-8")
body = '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n' + body
anchor = '''        // X360 BrnRootSoundModule.cpp:256-259.
'''
assert body.count(anchor) == 1
body = body.replace(anchor, '''        static s32 siLastPrepare = -1;
        static s32 siLastResource = -1;
        static s32 siLastRegistryGroup = -1;
        static u32 suLastRegistry = 0;
        static s32 siLastReplies = -1;
        const s32 liReplies = mReceiverQueue.GetLength();
        if (siLastPrepare != mePrepareStage || siLastResource != meResourceStage ||
            siLastRegistryGroup != meCurrentRegistry || suLastRegistry != mu32CurrentRegistry ||
            siLastReplies != liReplies)
        {
            siLastPrepare = mePrepareStage;
            siLastResource = meResourceStage;
            siLastRegistryGroup = meCurrentRegistry;
            suLastRegistry = mu32CurrentRegistry;
            siLastReplies = liReplies;
            char lacProbe[176];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R79_SOUND prepare=%d resource=%d group=%d registry=%u replies=%d\\n",
                siLastPrepare, siLastResource, siLastRegistryGroup, suLastRegistry, siLastReplies);
            OutputDebugStringA(lacProbe);
        }
''' + anchor)
with Path(__file__).with_name(source.name).open("x", encoding="utf-8") as output:
    output.write(body)
