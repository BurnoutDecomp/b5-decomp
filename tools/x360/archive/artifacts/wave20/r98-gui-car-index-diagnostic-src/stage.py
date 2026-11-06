from pathlib import Path
import sys


here = Path(__file__).resolve().parent
worktree, name = sys.argv[1:]
source = Path(worktree) / "src/GameSource/Gui/BrnGuiCache.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace(
    "        miSatNavZoomLevel = 0; // ARTIST 0x825062A8\n    }",
    """        miSatNavZoomLevel = 0; // ARTIST 0x825062A8
        char lacProbe[128];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R98_CAR_CONSTRUCT cache=%08X active=%d global=%d\\n",
            reinterpret_cast<u32>(this), static_cast<s32>(mePlayerActiveRaceCarIndex),
            static_cast<s32>(mePlayerGlobalRaceCarIndex));
        OutputDebugStringA(lacProbe);
    }""",
)
anchor = """                CGS_ASSERT(lpRaceCarIdEvent->mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,"""
replace(
    anchor,
    """                static unsigned int suRaceCarIdProbe = 0;
                if (suRaceCarIdProbe++ < 8)
                {
                    char lacProbe[148];
                    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                        "R98_CAR_ID cache=%08X incomingActive=%d incomingGlobal=%d\\n",
                        reinterpret_cast<u32>(this),
                        static_cast<s32>(lpRaceCarIdEvent->mePlayerActiveRaceCarIndex),
                        static_cast<s32>(lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex));
                    OutputDebugStringA(lacProbe);
                }
""" + anchor,
)
anchor = "            std::memcpy(maEventPositionValid, positions->mabValid, sizeof(maEventPositionValid));"
replace(
    anchor,
    anchor + """
            static unsigned int suRaceStandingsProbe = 0;
            if (suRaceStandingsProbe++ < 8)
            {
                char lacProbe[140];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R98_CAR_STANDINGS cache=%08X active=%d global=%d\\n",
                    reinterpret_cast<u32>(this), static_cast<s32>(mePlayerActiveRaceCarIndex),
                    static_cast<s32>(mePlayerGlobalRaceCarIndex));
                OutputDebugStringA(lacProbe);
            }""",
)
with (here / name).open("x", encoding="utf-8") as output:
    output.write("#include <xtl.h>\n" + body)
