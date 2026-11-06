from pathlib import Path
import sys


here = Path(__file__).resolve().parent
worktree = Path(sys.argv[1])
prefix = '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'


def write(relative, anchor, addition, *, after):
    path = worktree / relative
    body = path.read_text(encoding="utf-8")
    assert body.count(anchor) == 1, anchor
    replacement = anchor + addition if after else addition + anchor
    with (here / path.name).open("x", encoding="utf-8") as output:
        output.write(prefix + body.replace(anchor, replacement))


write(
    "src/GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.cpp",
    "    *lrSpec.mppvBuffer = lrBuffer.mpBuffer;",
    """
    char lacProbe[230];
    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R100_AUDIO_REQUEST slot=%d wrapper=%08X voice=%08X valid=%d file=%.96s\\n",
        liIndex, reinterpret_cast<u32>(&lrBuffer.mReadStream),
        reinterpret_cast<u32>(lhVoice.GetObject()), lrBuffer.mReadStream.IsValid(),
        lrSpec.mpFilename);
    OutputDebugStringA(lacProbe);""",
    after=True,
)
write(
    "src/GameShared/GameClasses/System/FileSystem/CgsReadStream.cpp",
    "    return mpStreamDevice->Read(auAmount, apDestination);",
    """    static unsigned int suSmallReads = 0;
    if (auAmount <= 32 && suSmallReads++ < 32)
    {
        char lacProbe[140];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R100_AUDIO_READ wrapper=%08X device=%08X amount=%u\\n",
            reinterpret_cast<u32>(this), reinterpret_cast<u32>(mpStreamDevice), auAmount);
        OutputDebugStringA(lacProbe);
    }
""",
    after=False,
)
