from pathlib import Path
import sys


here = Path(__file__).resolve().parent
root = Path(sys.argv[1])


def write(source, old, new):
    body = source.read_text(encoding="utf-8")
    assert body.count(old) == 1, old
    destination = here / source.name
    result = body.replace(old, new)
    if destination.exists():
        assert destination.read_text(encoding="utf-8") == result
    else:
        with destination.open("x", encoding="utf-8") as output:
            output.write(result)


anchor = """            if (mbSignOutPending ||
                mpGuiCache->GetTime() <= (mfStageStartTime + KF_WAIT_START_DWELL))"""
write(
    here.parent / "r102-legal-diagnostic-src/BrnBootLegal.cpp",
    anchor,
    """            if (suUpdates <= 32 || (suUpdates & 127u) == 0)
            {
                char lacProbe[200];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R109_LEGAL_DWELL now=%f start=%f step=%f signout=%d press=%d armed=%d\\n",
                    mpGuiCache->GetTime(), mfStageStartTime, mpGuiCache->GetTimeStep(),
                    mbSignOutPending, lbStartPressed, mbWaitForStartPressed);
                OutputDebugStringA(lacProbe);
            }
""" + anchor,
)

source = root / "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
body = source.read_text(encoding="utf-8")
start = body.index("void LoadingScriptedState::Update()")
end = body.index("bool LoadingScriptedState::LoadSoundModuleAgain(", start)
before, update, after = body[:start], body[start:end], body[end:]
anchor = "        const CgsSystem::Timer& lrGameTimer = lpGameModule->GetGameTimer();"
assert update.count(anchor) == 1
update = update.replace(
    anchor,
    anchor + """
        static unsigned int suClockProbe = 0;
        if (++suClockProbe <= 8 || (suClockProbe & 127u) == 0)
        {
            char lacProbe[184];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R109_SCRIPTED_CLOCK ticks=%d accum=%f rate=%f scale=%f paused=%d stage=%d\\n",
                static_cast<s32>(lrGameTimer.GetAccumTicks()), lrGameTimer.GetAccumulator(),
                lrGameTimer.GetRate(), lrGameTimer.GetScaleCurrent(), gBrnScriptedLoadPaused,
                static_cast<s32>(gBrnScriptedLoadStage));
            OutputDebugStringA(lacProbe);
        }""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                 + before + update + after)

source = root / "src/GameSource/Gui/BrnGuiCache.cpp"
body = source.read_text(encoding="utf-8")
anchor = "            std::memcpy(&mfTimeNow, reinterpret_cast<const u8*>(lpEvent) + 4, sizeof(mfTimeNow));"
assert body.count(anchor) == 1, anchor
body = body.replace(
    anchor,
    anchor + """
            {
                static unsigned int suGuiClockProbe = 0;
                if (++suGuiClockProbe <= 8 || (suGuiClockProbe & 127u) == 0)
                {
                    char lacProbe[160];
                    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                        "R109_CACHE_CLOCK count=%u step=%f now=%f\\n",
                        suGuiClockProbe, mfTimeStep, mfTimeNow);
                    OutputDebugStringA(lacProbe);
                }
            }""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write("#include <xtl.h>\n" + body)
