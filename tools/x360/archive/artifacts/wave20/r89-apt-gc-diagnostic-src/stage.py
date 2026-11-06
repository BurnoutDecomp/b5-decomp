from pathlib import Path


here = Path(__file__).resolve().parent
source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
source /= "src/SDKs/EATech/include/Apt/AptGC.cpp"
prefix = ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
          'extern bool gbTraceAptRuntime;\n')
body = source.read_text(encoding="utf-8")
start = body.index("void AptGC::CleanUnreachable()")
end = body.index("void AptFlushInputQueue()", start)
before, clean, after = body[:start], body[start:end], body[end:]


def replace(anchor, replacement):
    global clean
    assert clean.count(anchor) == 1, anchor
    clean = clean.replace(anchor, replacement)


replace(
    "    // 1. Flush anything already queued for deferred release.",
    """    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_INITIAL_RELEASE_BEGIN\\n");
    // 1. Flush anything already queued for deferred release.""",
)
replace(
    "    // 2. Mark every value reachable from a GC root.",
    """    if (gbTraceAptRuntime)
    {
        char lacProbe[120];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R89_GC_MARK_BEGIN pool=%08X sizeOffset=%u\\n",
            reinterpret_cast<uint32_t>(gpGCPoolManager),
            static_cast<unsigned int>(gAptValueGCSizeOffset));
        OutputDebugStringA(lacProbe);
    }
    unsigned int luProbeValues = 0;
    // 2. Mark every value reachable from a GC root.""",
)
replace(
    "        if (pValue->getGCRoot() == 0u || pValue->getGCMark())",
    """        const bool lbProbeValue = gbTraceAptRuntime && luProbeValues++ < 12;
        if (lbProbeValue)
        {
            char lacProbe[120];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R89_GC_MARK_NODE index=%u value=%08X\\n",
                luProbeValues, reinterpret_cast<uint32_t>(pValue));
            OutputDebugStringA(lacProbe);
        }
        if (pValue->getGCRoot() == 0u || pValue->getGCMark())""",
)
replace(
    "        pValue->RegisterReferences();   // vtbl +0x34 -- recurses via the callback",
    """        if (lbProbeValue) OutputDebugStringA("R89_GC_MARK_REFERENCE_BEGIN\\n");
        pValue->RegisterReferences();   // vtbl +0x34 -- recurses via the callback
        if (lbProbeValue) OutputDebugStringA("R89_GC_MARK_REFERENCE_END\\n");""",
)
replace(
    "    AptRegisterGlobalReferences();",
    """    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_GLOBAL_REFERENCES_BEGIN\\n");
    AptRegisterGlobalReferences();
    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_GLOBAL_REFERENCES_END\\n");""",
)
replace(
    "    const bool bWasSuspended = AptValue::sbSuspendRefcountDeletions;",
    """    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_PREDESTROY_BEGIN\\n");
    const bool bWasSuspended = AptValue::sbSuspendRefcountDeletions;""",
)
replace(
    "    AptValue::sbSuspendRefcountDeletions = bWasSuspended;",
    """    AptValue::sbSuspendRefcountDeletions = bWasSuspended;
    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_PREDESTROY_END\\n");""",
)
replace(
    "    // 5. Final flush, then the value free-lists / temporary string pool.",
    """    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_DELETE_END\\n");
    // 5. Final flush, then the value free-lists / temporary string pool.""",
)
replace(
    "    AptInteger::ClearPool();\n    AptFloat::ClearPool();\n    StringPool::ClearTemporaryPool();",
    """    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_FINAL_RELEASE_END\\n");
    AptInteger::ClearPool();
    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_INTEGER_POOL_END\\n");
    AptFloat::ClearPool();
    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_FLOAT_POOL_END\\n");
    StringPool::ClearTemporaryPool();
    if (gbTraceAptRuntime) OutputDebugStringA("R89_GC_STRING_POOL_END\\n");""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write(prefix + before + clean + after)
