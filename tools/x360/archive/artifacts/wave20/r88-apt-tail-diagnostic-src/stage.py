from pathlib import Path


here = Path(__file__).resolve().parent
prior = here.parent / "r87-apt-runtime-diagnostic-src/AptUpdate.cpp"
body = prior.read_text(encoding="utf-8")


def replace(anchor, replacement):
    global body
    assert body.count(anchor) == 1, anchor
    body = body.replace(anchor, replacement)


replace(
    """            ++gAptInputRecorderTag;
            pAnim->mnQueuedInputsCount = 0;""",
    """            ++gAptInputRecorderTag;
            pAnim->mnQueuedInputsCount = 0;
            if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_FALLBACK_END\\n");""",
)
replace(
    """    if (gpValuesToRelease != 0)
        gpValuesToRelease->ReleaseValues();
    if (gbAptZombiesDirty)
    {
        AptGC::CleanUnreachable();
        gbAptZombiesDirty = false;
    }""",
    """    if (gpValuesToRelease != 0)
    {
        if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_RELEASE_BEGIN\\n");
        gpValuesToRelease->ReleaseValues();
        if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_RELEASE_END\\n");
    }
    if (gbAptZombiesDirty)
    {
        if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_GC_BEGIN\\n");
        AptGC::CleanUnreachable();
        if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_GC_END\\n");
        gbAptZombiesDirty = false;
    }
    if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_UPDATE_END\\n");""",
)
replace(
    """    AptUpdate(nElapsedMs, nDepthLayerMask, nMaxBankedFrames);

    gpAptTarget = pPrevTarget;
    gAptTargetTls.SetValue(pPrevTarget);""",
    """    AptUpdate(nElapsedMs, nDepthLayerMask, nMaxBankedFrames);
    if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_RESTORE_BEGIN\\n");

    gpAptTarget = pPrevTarget;
    gAptTargetTls.SetValue(pPrevTarget);
    if (gbTraceAptRuntime) OutputDebugStringA("R88_APT_RESTORE_END\\n");""",
)
with (here / prior.name).open("x", encoding="utf-8") as output:
    output.write(body)
