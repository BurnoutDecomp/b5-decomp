from pathlib import Path
import sys


here = Path(__file__).resolve().parent
root = Path(sys.argv[1])
source = root / "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp"
body = source.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


anchor = "    void BootLegal::OnEnter()\n    {"
replace(anchor, anchor + '\n        OutputDebugStringA("R102_LEGAL_ENTER\\n");')
anchor = "    void BootLegal::Update()\n    {"
replace(
    anchor,
    anchor + """
        static s32 siLastStage = -1;
        static unsigned int suUpdates = 0;
        static unsigned int suEvents = 0;
        ++suUpdates;
        if (siLastStage != meUpdateStage || (suUpdates & 127u) == 0)
        {
            siLastStage = meUpdateStage;
            char lacProbe[140];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R102_LEGAL_STAGE stage=%d update=%u cache=%08X\\n",
                siLastStage, suUpdates, reinterpret_cast<u32>(mpGuiCache));
            OutputDebugStringA(lacProbe);
        }""",
)
anchor = "            const s32 liSubId   = *reinterpret_cast<const s32*>(lpPayload + 4);"
replace(
    anchor,
    anchor + """
            if (++suEvents <= 24)
            {
                char lacProbe[140];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R102_LEGAL_EVENT id=%d sub=%d size=%d stage=%d\\n",
                    liEventId, liSubId, liSize, static_cast<s32>(meUpdateStage));
                OutputDebugStringA(lacProbe);
            }""",
)
anchor = "                    if (lpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))"
replace(
    anchor,
    """                    const bool lbReady = lpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad);
                    if (suEvents <= 24)
                    {
                        char lacProbe[124];
                        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                            "R102_LEGAL_RESOURCE_GATE count=%u ready=%d\\n",
                            muNumResourcesToLoad, lbReady);
                        OutputDebugStringA(lacProbe);
                    }
                    if (lbReady)""",
)
anchor = "            mpStateInterface->PlayAptMovie(KAC_TITLE_MOVIE, 1);"
replace(
    anchor,
    '            OutputDebugStringA("R102_LEGAL_PLAY_TITLE\\n");\n' + anchor,
)
anchor = "            if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))\n                break;"
replace(
    anchor,
    """            if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
            {
                if (suUpdates <= 24 || (suUpdates & 127u) == 0)
                    OutputDebugStringA("R102_LEGAL_WAIT_COMPONENTS\\n");
                break;
            }""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n' + body)

source = root / "src/GameSource/Gui/BrnGuiCache.cpp"
body = source.read_text(encoding="utf-8")
start = body.index("    void StateLoadingHelper::OnLoadNotification(")
end = body.index("    void StateLoadingHelper::OnUnloadNotification(", start)
before, body, after = body[:start], body[start:end], body[end:]
anchor = "        ResourceInfo& lrInfo = maResources[luResourceId];"
replace(
    anchor,
    anchor + """
        if (luResourceId == 127 || luResourceId == 128)
        {
            char lacProbe[148];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R102_TITLE_NOTIFICATION id=%u old=%d type=%d handle=%08X\\n",
                luResourceId, static_cast<s32>(lrInfo.meState), static_cast<s32>(lrInfo.meType),
                reinterpret_cast<u32>(lpEvent->mResourceHandle.mpResourceMemory));
            OutputDebugStringA(lacProbe);
        }""",
)
body = before + body + after
anchor = "        const EResourceState leOldState     = lrResourceInfo.meState;"
start = body.index("    bool StateLoadingHelper::EnsureResourceIsLoaded(")
end = body.index("    bool StateLoadingHelper::EnsureResourcesAreLoaded(", start)
before, body, after = body[:start], body[start:end], body[end:]
replace(
    anchor,
    anchor + """
        if (lResource.muId == 127 || lResource.muId == 128)
        {
            static unsigned int suTitleChecks = 0;
            if (++suTitleChecks <= 20 || (suTitleChecks & 127u) == 0)
            {
                char lacProbe[150];
                CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                    "R102_TITLE_RESOURCE id=%u state=%d type=%d ptr=%08X\\n",
                    lResource.muId, static_cast<s32>(leOldState),
                    static_cast<s32>(lrResourceInfo.meType), reinterpret_cast<u32>(lrResourceInfo.mpResource));
                OutputDebugStringA(lacProbe);
            }
        }""",
)
body = before + body + after
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write("#include <xtl.h>\n" + body)
