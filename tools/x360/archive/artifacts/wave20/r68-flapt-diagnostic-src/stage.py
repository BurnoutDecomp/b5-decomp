from pathlib import Path


source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/SharedClasses/Gui/Flapt/BrnFlaptFile.cpp")
body = source.read_text(encoding="utf-8")
anchor = '#include "GameShared/GameClasses/Core/CgsAssert.h"'
assert body.count(anchor) == 1
body = body.replace(anchor, '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n\n' + anchor)
anchor = '    CGS_ASSERT(luFrame < muNumFramesInTimeline,\n'
assert body.count(anchor) == 1
body = body.replace(anchor, '''    char lacProbe[200];
    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R68_FLAPT_FRAME clip=%08X file=%08X frame=%u frames=%u map=%08X\\n",
        reinterpret_cast<u32>(this), reinterpret_cast<u32>(mpFile),
        luFrame, static_cast<u32>(muNumFramesInTimeline),
        reinterpret_cast<u32>(mpauFrameToKeyFrameMap));
    OutputDebugStringA(lacProbe);
''' + anchor)
with Path(__file__).with_name(source.name).open("x", encoding="utf-8") as output:
    output.write(body)
