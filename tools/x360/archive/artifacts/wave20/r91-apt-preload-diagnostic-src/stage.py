from pathlib import Path


here = Path(__file__).resolve().parent


def substitute(body, anchor, replacement):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, replacement)


apt = here.parent / "r87-apt-runtime-diagnostic-src/CgsAptAux.cpp"
body = apt.read_text(encoding="utf-8")
body = substitute(body, "static bool sbTraceNextAptUpdate = false;",
                  "static unsigned int suTraceAptUpdates = 0;")
body = substitute(body, "        sbTraceNextAptUpdate = true;",
                  "        suTraceAptUpdates = 8;")
body = substitute(
    body,
    """        const bool lbTrace = sbTraceNextAptUpdate;
        sbTraceNextAptUpdate = false;
        if (lbTrace)
            OutputDebugStringA("R86_APT_UPDATE_BEGIN\\n");""",
    """        const bool lbTrace = suTraceAptUpdates != 0;
        if (lbTrace)
        {
            char lacProbe[120];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R91_APT_UPDATE_BEGIN index=%u elapsed=%d\\n",
                9u - suTraceAptUpdates, liDeltaMs);
            --suTraceAptUpdates;
            OutputDebugStringA(lacProbe);
        }""",
)
body = substitute(
    body,
    "        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateTarget);",
    """        CgsDev::PerfMonCpu::StopMonitor(BrnGui::GuiPerfmons::miAptAuxUpdateTarget);
        if (lbTrace) OutputDebugStringA("R91_APT_AUX_END\\n");""",
)
with (here / apt.name).open("x", encoding="utf-8") as output:
    output.write(body)


preload = here.parent / "r85-apt-diagnostic-src/BrnBootPreload.cpp"
body = preload.read_text(encoding="utf-8")
body = substitute(
    body,
    """            if (!PreloadResourcesReady(mpGuiCache, KA_PRELOAD_FLAPTHUD, 1))
                break;
            mpStateInterface->PlayLoadingScreen();
            PreloadResourcesReady(mpGuiCache, maSecondPhaseResourcesToLoad,
                                  muSecondPhaseNumResourcesToLoad);""",
    """            if (suUpdates <= 8) OutputDebugStringA("R91_PRELOAD_FLAPTHUD_CHECK\\n");
            if (!PreloadResourcesReady(mpGuiCache, KA_PRELOAD_FLAPTHUD, 1))
                break;
            OutputDebugStringA("R91_PRELOAD_PLAY_LOADING_BEGIN\\n");
            mpStateInterface->PlayLoadingScreen();
            OutputDebugStringA("R91_PRELOAD_PLAY_LOADING_END\\n");
            PreloadResourcesReady(mpGuiCache, maSecondPhaseResourcesToLoad,
                                  muSecondPhaseNumResourcesToLoad);
            OutputDebugStringA("R91_PRELOAD_SECOND_PHASE_PREFETCH_END\\n");""",
)
body = substitute(
    body,
    """            if (!PreloadResourcesReady(mpGuiCache, maSecondPhaseResourcesToLoad,
                                       muSecondPhaseNumResourcesToLoad))
                break;""",
    """            if (suUpdates <= 8 || (suUpdates & 255u) == 0)
                OutputDebugStringA("R91_PRELOAD_SECOND_PHASE_CHECK\\n");
            if (!PreloadResourcesReady(mpGuiCache, maSecondPhaseResourcesToLoad,
                                       muSecondPhaseNumResourcesToLoad))
                break;""",
)
with (here / preload.name).open("x", encoding="utf-8") as output:
    output.write(body)
