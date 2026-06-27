#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"
#include <cstddef> // offsetof

// Compile-gate the serialised/runtime mixer records. Each "proc" is a {shared*,unique*}
// pair; the serialised headers are int-only blobs. (X360 sizes are 8/12/16/.. with
// 4-byte pointers; on x64 pointers widen -- field ORDER is the faithful invariant.)
static void NFSMixRecords_embed_check()
{
    (void)sizeof(stMixCtlProc); (void)sizeof(stMixMapHeader);
    static_assert(sizeof(stMixCtlProc) == 2 * sizeof(void*), "proc = {shared*, unique*}");
    static_assert(sizeof(st3DMixCtlProc) == 2 * sizeof(void*), "3d proc pair");
    static_assert(sizeof(stEvtMixCtlProc) == 2 * sizeof(void*), "evt proc pair");
    static_assert(sizeof(stSubMixChProc) == 2 * sizeof(void*), "sub proc pair");
    static_assert(sizeof(stMasterMixChProc) == 2 * sizeof(void*), "master proc pair");
    static_assert(sizeof(stMixMapHeader) == 16, "MixMap header = 4 ints");
    static_assert(sizeof(stMixMapStateHdr) == 32, "MixMap state header = 8 ints");
    static_assert(offsetof(st3DMixCtlParams, StateParams) == 4, "3D params: nINPUTID then StateParams");
}
