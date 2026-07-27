#include "SDKs/XGraphics/XGraphicsCompilerToSSM.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring> // memcpy

// ===========================================================================
// XGRAPHICS CompilerToSSM -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every numeric immediate (AS table index,
// render-state id, message index, packed-register bit). No reference source and
// no DWARF exist for this TU; see the header for the AS_Object layout notes.
//
// EXTERNAL-BY-ATTESTED-NAME dependencies (un-homed, declared so the asm-grounded
// bodies reproduce verbatim; the compile gate is `cl /c` so these need no
// definition here):
//   * AS_GetStateI is the integer sibling accessor -- bodied below from the
//     same +0x44/+0x08 reads the float accessor attests.
//   * sub_82C22190(value, buffer) -- the SSM value->text trace formatter; the
//     CompileGet* getters call it to fill the 80-byte trace buffer.
//   * the off_82FA5xxx state-name pointers -- the per-state debug-name strings
//     the optional trace callback prints; their CONTENTS live in un-recovered
//     X360 rodata, so they are referenced through extern symbols (the pointer is
//     external; nothing is fabricated). FLAG.
//   * D3D::SetTextureHeader / D3D::BlockSizeOfGpuFormat -- the Xenos texture
//     header builders SetBaseTextureHeader forwards to.
// ===========================================================================

// ---- un-homed externals referenced verbatim from the attested asm -----------
extern "C" int  sub_82C22190(int aiValue, void* apTextBuffer); // SSM value->text
extern "C" int  sub_82C217C8(int aiSentinel, int aiItem);      // list splice
extern "C" int* sub_82C21750(int aiPoolHead);                  // pool new-node

namespace D3D
{
    // FLAG: external Xenos texture builders (declared, not homed here).
    unsigned int BlockSizeOfGpuFormat(unsigned int auFormat, int* apBlockOut, unsigned int* apSizeOut);
    int SetTextureHeader(int a1, int a2, int a3, int a4, int a5, unsigned int a6, short a7, int a8);
}

namespace XGRAPHICS
{

// The optional verbose-trace path prints a per-state debug-name string that
// lives in un-recovered X360 rodata (the off_82FA5xxx tables). Since that name
// table cannot be grounded, the trace helpers below route through a single
// stand-in name pointer (kpcUnknownStateName) -- FLAG: the per-state name string
// is the ONLY un-grounded datum; every message index, stage and value the
// callbacks print IS attested in the asm.

// ===========================================================================
// infra core
// ===========================================================================

// @ 0x82C21DA0
f32 AS_GetStateF(AS_Object* apObject, u32 auState)
{
    CGS_ASSERT(auState < apObject->muNumStates,
               "(SSM_UINT32)eState < pAS_Object->ASMapInfo.uNumStates");
    // return *(float*)(*(a1+8) + 4*a2)
    u32 luBits = apObject->mpStateFloats[auState];
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

// @ integer sibling of AS_GetStateF (see header note). FLAG: reconstructed from
// the float accessor's attested +0x44/+0x08 reads, not a dedicated postmortem.
s32 AS_GetStateI(AS_Object* apObject, u32 auState)
{
    CGS_ASSERT(auState < apObject->muNumStates,
               "(SSM_UINT32)eState < pAS_Object->ASMapInfo.uNumStates");
    return static_cast<s32>(apObject->mpStateFloats[auState]);
}

// @ 0x82C21E10
f32 AS_GetArrayStateF(AS_Object* apObject, u32 auArrayState, u32 auIndex)
{
    CGS_ASSERT(auArrayState < apObject->muNumArrayStates,
               "(SSM_UINT32)eArrayState < pAS_Object->ASMapInfo.uNumArrayStates");
    // v10 = 12 * a2 ; map = *(a1+84) ; check uIndex < map[v10+8].uMaxCount
    ArrayStateMapEntry& lrEntry = apObject->mpArrayStateMap[auArrayState];
    CGS_ASSERT(auIndex < lrEntry.muMaxCount,
               "uIndex < pAS_Object->ASMapInfo.pArrayStateMap[eArrayState].uMaxCount");
    // AS_GetStateF(a1, stride*uIndex + base)
    return AS_GetStateF(apObject, lrEntry.muStride * auIndex + lrEntry.muBaseStateIndex);
}

// @ 0x82C21A78
s32 AS_GetArrayStateI(AS_Object* apObject, u32 auArrayState, u32 auIndex)
{
    CGS_ASSERT(auArrayState < apObject->muNumArrayStates,
               "(SSM_UINT32)eArrayState < pAS_Object->ASMapInfo.uNumArrayStates");
    ArrayStateMapEntry& lrEntry = apObject->mpArrayStateMap[auArrayState];
    CGS_ASSERT(auIndex < lrEntry.muMaxCount,
               "uIndex < pAS_Object->ASMapInfo.pArrayStateMap[eArrayState].uMaxCount");
    return AS_GetStateI(apObject, lrEntry.muStride * auIndex + lrEntry.muBaseStateIndex);
}

// @ 0x82C21B28 -- allocate a 16-byte Required-Renderstate node via the
// compiled-shader's allocator (*(a1+4))(*a1, 16), fill {0, id, stage, value},
// append it to the object-list at *(a1+2436). The compiled-shader handle is the
// Hex-Rays `int` (its full type is owned elsewhere); the indexed reads/writes
// are reproduced as attested raw accesses. FLAG: opaque compiled-shader handle.
s32 CS_SetRequiredRenderState(int aiCompiledShader, int aiRenderStateId,
                             int aiStage, int aiValue)
{
    CGS_ASSERT(aiCompiledShader, "pCompiledShader");
    int* lpBase = reinterpret_cast<int*>(aiCompiledShader);
    typedef int* (*AllocFn)(int, int);
    AllocFn lpfnAlloc = reinterpret_cast<AllocFn>(lpBase[1]); // *(a1+4)
    int* lpNode = lpfnAlloc(lpBase[0], 16);                   // (*a1, 16)
    if (lpNode)
    {
        lpNode[1] = aiRenderStateId; // +4
        lpNode[2] = aiStage;         // +8
        lpNode[3] = aiValue;         // +12
        lpNode[0] = 0;               // +0  type
        OBJLIST_AppendNewItem(reinterpret_cast<u32*>(lpBase[609]) /* *(a1+2436) */, 0);
        return 0;
    }
    CGS_ASSERT(false, "!\"Could not allocate memory for the Required Renderstate Struct\"");
    return 16;
}

// @ 0x82C218B0
int OBJLIST_AppendItem(int aiObjList, int aiItem)
{
    CGS_ASSERT(aiObjList, "pObjList");
    CGS_ASSERT(aiItem, "pItem");
    int* lpList = reinterpret_cast<int*>(aiObjList);
    CGS_ASSERT(aiItem != lpList[4] /* *(a1+16) pSentinel */, "pItem != pObjList->pSentinel");
    int liResult = sub_82C217C8(lpList[4], aiItem); // splice before sentinel
    if (!lpList[5] /* *(a1+20) count */)
        *reinterpret_cast<int*>(lpList[4] + 8) = aiItem; // *(*(a1+16)+8) = pItem
    ++lpList[5];
    return liResult;
}

// @ 0x82C21978
int OBJLIST_AppendNewItem(u32* apObjList, int aiPayload)
{
    CGS_ASSERT(apObjList, "pObjList");
    int* lpNewItem = sub_82C21750(static_cast<int>(apObjList[0])); // *a1 = pool head
    CGS_ASSERT(lpNewItem, "pNewItem");
    lpNewItem[3] = aiPayload; // *(pNewItem+12) = payload
    OBJLIST_AppendItem(reinterpret_cast<int>(apObjList), reinterpret_cast<int>(lpNewItem));
    return reinterpret_cast<int>(lpNewItem);
}

// @ 0x82C21BE0 -- build a fixed-count free-list pool. Node[0]=alloc, [1]=free,
// [2]=user, [4]=rounded node size, [3]=block base, [5]=[6]=count, [7]=next; the
// block is threaded into a singly-linked free list of `aiCount` nodes.
int (**MemoryInit(int aiCount, int aiNodeSize,
                  int (*apfnAlloc)(int, int), int (*apfnFree)(int, int),
                  int (*apfnUser)(int, int)))(int, int)
{
    typedef int (*Fn)(int, int);
    u32 luNodeSize = (static_cast<u32>(aiNodeSize) + 7) & 0xFFFFFFF8u; // round to 8
    Fn* lpHead = reinterpret_cast<Fn*>(apfnAlloc(reinterpret_cast<int>(apfnUser), 32));
    Fn* lpPool = lpHead;
    if (lpHead)
    {
        lpHead[0] = apfnAlloc;
        lpHead[1] = apfnFree;
        lpHead[2] = apfnUser;
        reinterpret_cast<u32*>(lpHead)[4] = luNodeSize;
        Fn lpBlock = reinterpret_cast<Fn>(apfnAlloc(reinterpret_cast<int>(apfnUser),
                                                    (aiCount + 1) * static_cast<int>(luNodeSize)));
        if (lpBlock)
        {
            reinterpret_cast<void**>(lpPool)[3] = reinterpret_cast<void*>(lpBlock);
            char* lpCursor = reinterpret_cast<char*>(lpBlock);
            reinterpret_cast<int*>(lpPool)[5] = aiCount;
            reinterpret_cast<int*>(lpPool)[6] = aiCount;
            reinterpret_cast<int*>(lpPool)[7] = 0;
            int liRemaining = aiCount;
            void** lpWrite = reinterpret_cast<void**>(lpBlock);
            while (liRemaining > 0)
            {
                lpCursor += luNodeSize;
                --liRemaining;
                *lpWrite = lpCursor;                 // *node = next
                lpWrite = reinterpret_cast<void**>(lpCursor);
            }
        }
        else
        {
            apfnFree(reinterpret_cast<int>(apfnUser), reinterpret_cast<int>(lpPool));
            lpPool = nullptr;
        }
    }
    return lpPool;
}

// @ 0x82C21CA0 -- pop a free node from the pool chain; grow it (MemoryInit) when
// the current segment is exhausted; assert if no node could be produced.
u32* MemoryNewNode(u32* apHead, int aiTag)
{
    (void)aiTag;
    CGS_ASSERT(apHead, "pHead");
    u32* lpSeg = apHead;
    u32* lpNode = nullptr;
    while (lpSeg)
    {
        if (lpSeg[6]) // free count in this segment
        {
            u32** lpFreeListPtr = reinterpret_cast<u32**>(lpSeg[3]); // block free-list head holder
            lpNode = *lpFreeListPtr;
            *lpFreeListPtr = reinterpret_cast<u32*>(**reinterpret_cast<u32***>(lpSeg[3]));
            if (lpNode)
            {
                --lpSeg[6];
                return lpNode;
            }
        }
        else
        {
            u32* lpScan = lpSeg;
            while (lpScan[7] && !reinterpret_cast<u32*>(lpScan[7])[6])
                lpScan = reinterpret_cast<u32*>(lpScan[7]);
            lpSeg = lpScan;
            if (!lpSeg[6])
            {
                typedef int (*Fn)(int, int);
                int (**lpNew)(int, int) = MemoryInit(static_cast<int>(lpSeg[5]),
                                                     static_cast<int>(lpSeg[4]),
                                                     reinterpret_cast<Fn>(lpSeg[0]),
                                                     reinterpret_cast<Fn>(lpSeg[1]),
                                                     reinterpret_cast<Fn>(lpSeg[2]));
                lpSeg[7] = reinterpret_cast<u32>(lpNew);
                lpSeg = reinterpret_cast<u32*>(lpNew);
                continue; // re-enter the free-pop path (LABEL_10)
            }
        }
    }
    CGS_ASSERT(apHead, "pHead");
    return nullptr;
}

// @ 0x82C21108 -- build a base-texture GPU header. Every store immediate
// (0x3F format mask, the 6-DWORD zero init, the 0x3B4-style fields, the
// 0xFFFFF000 page masks, 0x400000 / 0x200000 caps, -65536 / 0x4B... seeds)
// is attested in the asm. D3D::BlockSizeOfGpuFormat / D3D::SetTextureHeader are
// external (declared). FLAG: a4..a8 dim-validation branches and the v60[]
// header word layout are modelled from the asm displacements only (no DWARF).
int SetBaseTextureHeader(int a1, int a2, int a3, int a4, int a5, int a6,
                         int a7, int a8, int a9, int a10, int a11, int a12,
                         int a13, int a14, int a15, int a16, int a17, int a18,
                         int a19, int a20, int a21, int a22, int a23, int a24,
                         int a25, int a26, int a27, int a28, int a29, int a30,
                         int a31, int a32, int a33, int a34, int a35, int a36,
                         int a37, int a38, int a39, u32* a40, int a41,
                         u32* a42)
{
    (void)a9; (void)a10; (void)a11; (void)a12; (void)a13; (void)a14;
    (void)a15; (void)a16; (void)a17; (void)a18; (void)a19; (void)a20;
    (void)a21; (void)a22; (void)a23; (void)a24; (void)a25; (void)a26;
    (void)a27; (void)a28; (void)a29; (void)a31; (void)a33; (void)a34;
    (void)a35; (void)a37; (void)a39; (void)a41;

    int  laBlock[3];   // v59
    u32  luSurfaceSize = 0; // v58
    D3D::BlockSizeOfGpuFormat(static_cast<unsigned int>(a6) & 0x3F, laBlock, &luSurfaceSize);

    // dimension-consistency validation (a1..a4,a8) -> __twllei trap when a36==3
    bool lbTrap = true;
    if (a4 != 1)
    {
        if (a4 == 0)
        {
            if (a1 == 1)
            {
                if (a2 == 1 && a3 == 1 && a8 == 0)
                    lbTrap = (a1 == 3 && a2 == 3 && a3 == 3 && a8 != 0);
            }
            else
            {
                lbTrap = (a1 == 3 && a2 == 3 && a3 == 3 && a8 != 0);
            }
        }
    }
    if (lbTrap && a36 == 3 && luSurfaceSize == 0)
    {
        // __twllei(v58,0): trap if v58 <= 0 (here v58==0 already)
        CGS_ASSERT(luSurfaceSize != 0, "v58 != 0");
    }

    u32 lau60[32];
    for (int i = 0; i < 6; ++i)
        lau60[i] = 0;
    lau60[6] = 0;

    D3D::SetTextureHeader(a36, a1, a2, a3, a4, static_cast<unsigned int>(a5),
                          static_cast<short>(a6), a7);

    int liValue0 = 3;
    lau60[1] = 1;
    lau60[0] = 3;
    lau60[8] = (static_cast<u32>(a30) & 0xFFFFF000u) | (lau60[8] & 0xFFFu);
    lau60[5] = static_cast<u32>(-65536);
    lau60[6] = static_cast<u32>(-65536);
    if ((a5 & 4) != 0)
    {
        liValue0 = 2097155;
        lau60[0] = 2097155;
    }
    if ((a5 & 0x200) != 0)
        lau60[0] = liValue0 | 0x400000;

    int liBlock0 = laBlock[0]; // v54
    u32 luBaseSize;            // v55
    if (laBlock[0])
    {
        if (a32 != -1)
        {
            luBaseSize = luSurfaceSize;
            lau60[12] = (static_cast<u32>(a32) & 0xFFFFF000u) | (lau60[12] & 0xFFFu);
        }
        else
        {
            luBaseSize = (luSurfaceSize + 4095) & 0xFFFFF000u;
            lau60[12] = (lau60[12] & 0xFFFu) | ((luBaseSize + a30) & 0xFFFFF000u);
        }
    }
    else
    {
        luBaseSize = luSurfaceSize;
        lau60[12] = (lau60[12] & 0xFFFu);
    }

    if (a40)
        *a40 = luBaseSize;
    if (a42)
        *a42 = static_cast<u32>(liBlock0);
    if (a38)
        std::memcpy(reinterpret_cast<void*>(a38), lau60, 52);
    return static_cast<int>(luBaseSize) + liBlock0;
}

// @ 0x8295E070 -- multi-stage cache-line-aware memcpy. The VMX in the asm is the
// pure WIDE-COPY idiom, not vector arithmetic (lvx128 = aligned 16-byte load,
// stvlx+stvrx = unaligned 16-byte store pair, lvlx+lvrx+vor = unaligned 16-byte
// load merge, dcbz128 = destination cache-line pre-zero), so the documented
// lvx-as-memcpy exception applies and the faithful reconstruction is the same
// staged copy with plain fixed-width moves. The stage structure, the guards and
// the branch polarity are reproduced exactly -- including the tail stages, which
// really are still conditioned on the SOURCE alignment, so a 16-byte-aligned
// source with a non-multiple-of-16 length leaves the last bytes uncopied. That
// is the binary's behaviour; the sole caller (XGUntileSurface) never hits it, and
// it is deliberately NOT "fixed" here.
u8* CopyMemoryCachedDst(u8* apDst, u8* apSrc, u32 auBytes)
{
    u8* lpSrc = apSrc; // r11
    u8* lpDst = apDst; // r10

    // ---- stage 1: single bytes until the SOURCE is 4-byte aligned (lbz/stb) ----
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 3u) != 0 && auBytes >= 1u)
    {
        *lpDst = *lpSrc;
        ++lpSrc;
        ++lpDst;
        auBytes -= 1u;
    }

    // ---- stage 2: 4-byte steps until the source is 8-byte aligned (lwz/stw) ----
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 7u) != 0 && auBytes >= 4u)
    {
        std::memcpy(lpDst, lpSrc, 4);
        lpSrc += 4;
        lpDst += 4;
        auBytes -= 4u;
    }

    // ---- stage 3: 8-byte steps until the source is 16-byte aligned (ld/std) ----
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 15u) != 0 && auBytes >= 8u)
    {
        std::memcpy(lpDst, lpSrc, 8);
        lpSrc += 8;
        lpDst += 8;
        auBytes -= 8u;
    }

    // ---- stage 4: 16-byte blocks until (dst + 15) sits in the low quadword of a
    // 128-byte cache line, i.e. until the next dcbz128 line is the one about to be
    // written. The asm carries r8 = dst + 15 incrementally and adds 0x10 per pass;
    // recomputing it from lpDst each pass is the same value. --------------------
    while (((reinterpret_cast<uintptr_t>(lpDst) + 15u) & 0x7Fu) >= 0x10u && auBytes >= 0x10u)
    {
        std::memcpy(lpDst, lpSrc, 16);
        lpSrc += 0x10;
        lpDst += 0x10;
        auBytes -= 0x10u;
    }

    // ---- main loop: one 128-byte cache line per outer pass ---------------------
    while (auBytes >= 0x8Fu)
    {
        // X360 issues dcbz128 at EA (lpDst + 15) here, pre-zeroing the destination
        // cache line it is about to write. The zeroed line lies inside
        // [lpDst, lpDst + 143), which is why the guard is 0x8F (128 + 15) and not
        // 0x80. Those bytes are covered by this loop plus the stages below EXCEPT
        // in the same src-16-aligned / auBytes % 16 != 0 corner this file already
        // documents for the tails (e.g. auBytes == 0x8F with an aligned source
        // leaves lpDst+128..lpDst+142 zeroed on X360 but untouched here). Emitting
        // no code is still the right call off X360 -- a cache hint is not portable
        // and the caller never reads that tail -- but the equivalence is not total.
        u32 luHalves = 2u; // the asm's r6 counter: 2 x 64 bytes = one 128-byte line
        do
        {
            // four 16-byte moves at +0x00/+0x10/+0x20/+0x30 (lvlx+lvrx+vor loads,
            // stvlx+stvrx stores -- wide copy only, no vector arithmetic)
            for (u32 luBlock = 0u; luBlock < 4u; ++luBlock)
                std::memcpy(lpDst + 0x10 * luBlock, lpSrc + 0x10 * luBlock, 16);
            lpSrc += 0x40;
            lpDst += 0x40;
            auBytes -= 0x40u;
        }
        while (--luHalves);
    }

    // ---- post loop: whole 16-byte blocks (lvx128 aligned load + unaligned store) --
    if (auBytes >= 0x10u)
    {
        u32 luBlocks = auBytes >> 4;
        do
        {
            std::memcpy(lpDst, lpSrc, 16);
            auBytes -= 0x10u;
            lpSrc += 0x10;
            lpDst += 0x10;
        }
        while (--luBlocks);
    }

    // ---- tails: stages 3/2/1 mirrored, still keyed off the SOURCE alignment -----
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 15u) != 0 && auBytes >= 8u)
    {
        std::memcpy(lpDst, lpSrc, 8);
        lpSrc += 8;
        lpDst += 8;
        auBytes -= 8u;
    }
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 7u) != 0 && auBytes >= 4u)
    {
        std::memcpy(lpDst, lpSrc, 4);
        lpSrc += 4;
        lpDst += 4;
        auBytes -= 4u;
    }
    while ((reinterpret_cast<uintptr_t>(lpSrc) & 3u) != 0 && auBytes >= 1u)
    {
        *lpDst = *lpSrc;
        ++lpSrc;
        ++lpDst;
        auBytes -= 1u;
    }

    // The binary leaves r3 clobbered (the main loop reuses it as a scratch address)
    // and its one caller ignores the result; the u8* return follows the Hex-Rays
    // param-seed, so the destination pointer is returned. FLAG.
    return apDst;
}

// ===========================================================================
// helper: emit the optional verbose trace for an integer-valued state.
// The state-name string is un-homed rodata (FLAG); only the format and the
// attested message index / stage / value are reproduced.
// ===========================================================================
namespace
{
    // Stand-in for the un-recovered per-state rodata name pointer.
    const char* const kpcUnknownStateName = "?"; // FLAG: un-homed off_82FA5xxx

    inline void TraceVal(CompilePrintFn apfn, int aiChannel,
                         int aiMsgIndex, int aiStage, int aiValue)
    {
        if (apfn && aiChannel)
            apfn(aiChannel, "%s (%d) %d : %d\n", kpcUnknownStateName, aiMsgIndex, aiStage, aiValue);
    }
    inline void TraceValNoStage(CompilePrintFn apfn, int aiChannel,
                               int aiMsgIndex, int aiValue)
    {
        if (apfn && aiChannel)
            apfn(aiChannel, "%s (%d) : %d\n", kpcUnknownStateName, aiMsgIndex, aiValue);
    }
    inline void TraceText(CompilePrintFn apfn, int aiChannel,
                         int aiMsgIndex, int aiStage, int aiValueForFmt)
    {
        if (apfn && aiChannel)
        {
            char laText[80];
            sub_82C22190(aiValueForFmt, laText);
            apfn(aiChannel, "%s (%d) %d : %s\n", kpcUnknownStateName, aiMsgIndex, aiStage, laText);
        }
    }
    inline void TraceTextNoStage(CompilePrintFn apfn, int aiChannel,
                                int aiMsgIndex, int aiValueForFmt)
    {
        if (apfn && aiChannel)
        {
            char laText[80];
            sub_82C22190(aiValueForFmt, laText);
            apfn(aiChannel, "%s (%d) : %s\n", kpcUnknownStateName, aiMsgIndex, laText);
        }
    }
}

// ===========================================================================
// CompileGet* getters
// ===========================================================================

// @ 0x82C25028  AS_GetArrayStateF table 16, msg 71
int CompileGetBorderColorR(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 16, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 71, aiStage, 0);
    return 1;
}

// @ 0x82C250D8  AS_GetArrayStateF table 17, msg 72
int CompileGetBorderColorG(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 17, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 72, aiStage, 0);
    return 1;
}

// @ 0x82C25188  AS_GetArrayStateF table 18, msg 73
int CompileGetBorderColorB(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 18, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 73, aiStage, 0);
    return 1;
}

// @ 0x82C25238  AS_GetArrayStateF table 19, msg 74
int CompileGetBorderColorA(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 19, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 74, aiStage, 0);
    return 1;
}

// @ 0x82C252E8  FogBias: AS_TABLE_FOG_BLEND(165)==3 -> AS_GetStateF(0xA6) else 0;
//               then assert AS_TABLE_FOG_INDEX(170)==1 (W-fog).
int CompileGetFogBias(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (AS_GetStateI(apObject, 165) == 3)
        *apOut = AS_GetStateF(apObject, 0xA6u);
    else
        *apOut = 0.0f;
    CGS_ASSERT(AS_GetStateI(apObject, 170) == 1,
               "CompileGetFogBias -- why are we calculating a bias for Z-fog?");
    TraceText(apfnPrint, aiChannel, 26, aiStage, 0);
    return 1;
}

// @ 0x82C24E48  FogColorR: AS_GetStateF(1000), msg 9
int CompileGetFogColorR(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 1000);
    TraceText(apfnPrint, aiChannel, 9, aiStage, 0);
    return 1;
}

// @ 0x82C24EE8  FogColorG: AS_GetStateF(1001)
int CompileGetFogColorG(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 1001);
    TraceText(apfnPrint, aiChannel, 10, aiStage, 0);
    return 1;
}

// @ 0x82C258F0  FogDensity: AS_GetStateF(168)
int CompileGetFogDensity(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 168);
    TraceText(apfnPrint, aiChannel, 65, aiStage, 0);
    return 1;
}

// @ 0x82C25850  FogEnd: AS_GetStateF(167)
int CompileGetFogEnd(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 167);
    TraceText(apfnPrint, aiChannel, 66, aiStage, 0);
    return 1;
}

// @ 0x82C253F0  FogScale: linear (165==3) -> 1/(end-start) (or FLT_MAX when
//               end==start); else exp -> 1/(5.5413637/density). Asserts W-fog.
int CompileGetFogScale(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (AS_GetStateI(apObject, 165) == 3)
    {
        f32 lfStart = AS_GetStateF(apObject, 166);
        f32 lfEnd   = AS_GetStateF(apObject, 167);
        if ((lfEnd - lfStart) == 0.0f)
        {
            *apOut = 3.4028235e38f; // FLT_MAX
            CGS_ASSERT(AS_GetStateI(apObject, 170) == 1,
                       "CompileGetFogScale -- why are we calculating a scale for Z-fog?");
            TraceText(apfnPrint, aiChannel, 27, aiStage, 0);
            return 1;
        }
        *apOut = 1.0f / (lfEnd - lfStart);
    }
    else
    {
        *apOut = 1.0f / (5.5413637f / AS_GetStateF(apObject, 168));
    }
    CGS_ASSERT(AS_GetStateI(apObject, 170) == 1,
               "CompileGetFogScale -- why are we calculating a scale for Z-fog?");
    TraceText(apfnPrint, aiChannel, 27, aiStage, 0);
    return 1;
}

// @ 0x82C257B0  FogStart: AS_GetStateF(166)
int CompileGetFogStart(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 166);
    TraceText(apfnPrint, aiChannel, 67, aiStage, 0);
    return 1;
}

// @ 0x82C24A10  HOSMode: AS_GetStateI(567)/(974) -> {2,5,8,11}; assert otherwise.
int CompileGetHOSMode(AS_Object* apObject, int* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    int liStateI = AS_GetStateI(apObject, 567);
    int liMode   = AS_GetStateI(apObject, 974);
    int liValue;
    if (liStateI == 2)
    {
        switch (liMode)
        {
        case 0: liValue = 2;  *apOut = liValue; break;
        case 1: liValue = 5;  *apOut = liValue; break;
        case 3: liValue = 8;  *apOut = liValue; break;
        case 2: liValue = 11; *apOut = liValue; break;
        default:
            CGS_ASSERT(false, "0");
            TraceValNoStage(apfnPrint, aiChannel, 61, *apOut);
            return 1;
        }
    }
    else
    {
        CGS_ASSERT(false, "0");
        TraceValNoStage(apfnPrint, aiChannel, 61, *apOut);
        return 1;
    }
    TraceValNoStage(apfnPrint, aiChannel, 61, *apOut);
    return 1;
}

// @ 0x82C25990  LodBias: always 0.0 (msg 64)
int CompileGetLodBias(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = 0.0f;
    TraceText(apfnPrint, aiChannel, 64, aiStage, 0);
    return 1;
}

// @ 0x82C25A30  LodClampMax: AS_GetArrayStateF table 26
int CompileGetLodClampMax(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 26, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 65, aiStage, 0);
    return 1;
}

// @ 0x82C25B90  MemExportConstant0: AS_GetArrayStateI(0x59) >> 2 | 0x40000000
int CompileGetMemExportConstant0(AS_Object* apObject, u32 auStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    u32 luState = static_cast<u32>(AS_GetArrayStateI(apObject, 0x59u, auStage));
    *apOut = (luState >> 2) | 0x40000000u;
    TraceText(apfnPrint, aiChannel, 62, static_cast<int>(auStage), static_cast<int>(luState));
    return 1;
}

// @ 0x82C25C48  MemExportConstant1: constant 0x4B000000 (1258291200), msg 88
int CompileGetMemExportConstant1(AS_Object* apObject, int aiStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    (void)apObject;
    *apOut = 1258291200u;
    TraceText(apfnPrint, aiChannel, 88, aiStage, 0);
    return 1;
}

// The Xenos memory-export surface-format codes CompileGetMemExportConstant2
// indexes. RECOVERED from X360 rodata 0x82FA5220 -- 16 big-endian dwords bounded
// by pointer data on both sides (0x82FA521C = 0 below, 0x82FA5260 = a pointer
// above); see scratchpad/waveD/XGRAPHICS.rodata.txt. The asm's effective address
// is `unk_82FA5220 + 4*(4*format + count) - 4`, i.e. table[4*format + count - 1]
// over format 0..3 x count 1..4.
static const s32 kaiMemExportConstant2Format[16] =
{
    36, 37, 38, 38, // format 0 (FLOAT32): k_32_FLOAT/k_32_32_FLOAT/k_32_32_32_32_FLOAT, count 1..4
    30, 31, 32, 32, // format 1 (FLOAT16): k_16_FLOAT/k_16_16_FLOAT/k_16_16_16_16_FLOAT, count 1..4
    24, 25, 26, 26, // format 2 (16-bit fixed): k_16/k_16_16/k_16_16_16_16,               count 1..4
     2, 10,  6,  6  // format 3 (8-bit fixed):  k_8/k_8_8/k_8_8_8_8,                      count 1..4
};

// @ 0x82C25CC0  MemExportConstant2: format = AS_GetArrayStateI(91, stage),
// count = AS_GetArrayStateI(92, stage); packs ((sel << 8) | 0x4B0000 | entry) << 8
// with sel = 2 for the NON-float formats (2 and 3) and 7 for the float ones.
// The assert names FLOAT16/FLOAT32, which are formats 0 and 1: only those two
// bypass SSMDebugPrint at 0x82C25D64-0x82C25D8C. Msg index 89.
int CompileGetMemExportConstant2(AS_Object* apObject, int aiStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    u32 luFormat = static_cast<u32>(AS_GetArrayStateI(apObject, 91, static_cast<u32>(aiStage)));
    s32 liCount  = AS_GetArrayStateI(apObject, 92, static_cast<u32>(aiStage));
    // the asm loads the table entry before the format branch (slwi/add/lwz -4(r11))
    s32 liEntry  = kaiMemExportConstant2Format[4u * luFormat + static_cast<u32>(liCount) - 1u];

    s32 liSel;
    if (luFormat == 2 || luFormat == 3)
    {
        liSel = 2;
    }
    else
    {
        // formats 0 and 1 branch past the assert silently; it fires only for >= 4.
        CGS_ASSERT(luFormat < 2,
                   "format == AS_VS_EXPORT_STREAM_FORMAT_N__FLOAT16 || format == AS_VS_EXPORT_STREAM_FORMAT_N__FLOAT32");
        liSel = 7;
    }
    *apOut = static_cast<u32>(((liSel << 8) | 0x4B0000 | liEntry) << 8);

    if (apfnPrint && aiChannel)
    {
        char laText[112]; // frame-attested buffer (sp+0x50 .. sp+0xC0 of a 0xC0 frame)
        // the binary passes *(float*)apOut to the formatter in f1; the un-homed
        // sub_82C22190 shim is declared (int, void*), so 0 goes through here exactly
        // as every committed sibling does. FLAG.
        sub_82C22190(0, laText);
        // name-table entry 89 @ 0x82FA51C4 -> "CompileGetMemExportConstant2"
        // (RECOVERED rodata, so the real name prints instead of kpcUnknownStateName)
        apfnPrint(aiChannel, "%s (%d) %d : %s\n", "CompileGetMemExportConstant2", 89, aiStage, laText);
    }
    return 1;
}

// @ 0x82C25E08  MemExportConstant3: AS_GetArrayStateI(90) | 0x4B000000, msg 90
int CompileGetMemExportConstant3(AS_Object* apObject, int aiStage, int* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liState = AS_GetArrayStateI(apObject, 90, static_cast<u32>(aiStage));
    *apOut = liState | 0x4B000000;
    TraceText(apfnPrint, aiChannel, 90, aiStage, liState);
    return 1;
}

// @ 0x82C24BE8  PointSizeMax: AS_GetStateF(0x82), msg 103
int CompileGetPointSizeMax(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 0x82u);
    if (apfnPrint && aiChannel)
    {
        char laText[80];
        sub_82C22190(0, laText);
        apfnPrint(aiChannel, "%s (%d) : %s\n", kpcUnknownStateName, 103, laText);
    }
    (void)aiStage;
    return 1;
}

// @ 0x82C24C88  PointSizeMin: AS_GetStateF(129)
int CompileGetPointSizeMin(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetStateF(apObject, 129);
    if (apfnPrint && aiChannel)
    {
        char laText[80];
        sub_82C22190(0, laText);
        apfnPrint(aiChannel, "%s (%d) : %s\n", kpcUnknownStateName, 104, laText);
    }
    (void)aiStage;
    return 1;
}

// @ 0x82C25640  ShadowFailThreshold: AS_GetArrayStateF table 86, msg 51
int CompileGetShadowFailThreshold(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    *apOut = AS_GetArrayStateF(apObject, 86, static_cast<u32>(aiStage));
    TraceText(apfnPrint, aiChannel, 51, aiStage, 0);
    return 1;
}

// @ 0x82C24AF8  TextureFormatData: stage>=16 -> 0 else AS_GetArrayStateI(15);
//               then fill the 10-DWORD format descriptor with attested consts.
int CompileGetTextureFormatData(AS_Object* apObject, u32 auStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liState = (auStage >= 0x10) ? 0 : AS_GetArrayStateI(apObject, 15, auStage);
    apOut[1] = static_cast<u32>(liState);
    apOut[0] = 0;
    apOut[2] = 3;
    apOut[3] = 3;
    apOut[4] = 3;
    apOut[5] = 7;
    apOut[6] = 7;
    apOut[7] = 3;
    apOut[8] = 3;
    apOut[9] = 1;
    TraceVal(apfnPrint, aiChannel, 31, static_cast<int>(auStage), 0);
    return 1;
}

// @ 0x82C256F0  VertexStreamFrequencyDivideInterval: AS_GetArrayStateI(48) -> float
int CompileGetVertexStreamFrequencyDivideInterval(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liState = AS_GetArrayStateI(apObject, 48, static_cast<u32>(aiStage));
    *apOut = static_cast<f32>(liState);
    if (apfnPrint && aiChannel)
    {
        char laText[80];
        sub_82C22190(liState, laText);
        apfnPrint(aiChannel, "%s (%d) : %s\n", kpcUnknownStateName, 54, laText);
    }
    (void)aiStage;
    return 1;
}

// @ 0x82C24D28  WincoordOffsetsForPolyStippleX: constant 4194303.5
int CompileGetWincoordOffsetsForPolyStippleX(AS_Object* apObject, int aiStage, f32* apOut)
{
    (void)apObject; (void)aiStage;
    *apOut = 4194303.5f;
    return 1;
}

// @ 0x82C24D40  WincoordOffsetsForPolyStippleY: AS_GetStateI(580)/(581) ->
//               (581==1) ? (state580-1)+4194304.5 : 4194303.5
int CompileGetWincoordOffsetsForPolyStippleY(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liState = AS_GetStateI(apObject, 580);
    int liMode  = AS_GetStateI(apObject, 581);
    if (liMode == 1)
        *apOut = static_cast<f32>(static_cast<f32>(liState - 1) + 4194304.5f);
    else
        *apOut = 4194303.5f;
    if (apfnPrint && aiChannel)
    {
        char laText[80];
        sub_82C22190(liMode, laText);
        apfnPrint(aiChannel, "%s (%d) : %s\n", kpcUnknownStateName, 67, laText);
    }
    (void)aiStage;
    return 1;
}

// @ 0x82C24E30  WincoordOffsetsForWincoordX: constant 4194304.0
int CompileGetWincoordOffsetsForWincoordX(AS_Object* apObject, int aiStage, f32* apOut)
{
    (void)apObject; (void)aiStage;
    *apOut = 4194304.0f;
    return 1;
}

// @ 0x82C25550..0x82C25628  YUVConstants: hard-coded color-conversion matrix.
int CompileGetYUVConstantsC00(AS_Object*, int, f32* apOut) { *apOut = -0.87420243f;   return 1; }
int CompileGetYUVConstantsC01(AS_Object*, int, f32* apOut) { *apOut = 0.53166765f;    return 1; }
int CompileGetYUVConstantsC02(AS_Object*, int, f32* apOut) { *apOut = -1.0856302f;    return 1; }
int CompileGetYUVConstantsC03(AS_Object*, int, f32* apOut) { *apOut = 1.0f;           return 1; }
int CompileGetYUVConstantsC10(AS_Object*, int, f32* apOut) { *apOut = 0.0045662099f;  return 1; }
int CompileGetYUVConstantsC22(AS_Object*, int, f32* apOut) { *apOut = 0.0079107098f;  return 1; }
int CompileGetYUVConstantsC30(AS_Object*, int, f32* apOut) { *apOut = 0.0062589301f;  return 1; }
int CompileGetYUVConstantsC31(AS_Object*, int, f32* apOut) { *apOut = -0.00318811f;   return 1; }
int CompileGetYUVConstantsC33(AS_Object*, int, f32* apOut) { *apOut = 0.0f;           return 1; }

// ===========================================================================
// CompileWith* setters
// ===========================================================================

// @ 0x82C22570  BorderColor: always emit RS 7 (stage,0), msg 10
int CompileWithBorderColor(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    (void)apObject;
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 7, aiStage, 0);
    TraceVal(apfnPrint, aiChannel, 10, aiStage, 0);
    return 0;
}

// @ 0x82C24678  CXVUTextureFormat: always emit RS 32, msg 52
int CompileWithCXVUTextureFormat(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 32, 0, 0);
    TraceVal(apfnPrint, aiChannel, 52, aiStage, 0);
    return 0;
}

// @ 0x82C24228  ColorClamp01: AS_GetArrayStateI(37)==1 -> RS 35 value 1/0, msg 58
int CompileWithColorClamp01(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 37, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 35, 0, 1);
        TraceVal(apfnPrint, aiChannel, 58, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 35, 0, 0);
        TraceVal(apfnPrint, aiChannel, 58, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C24350  ColorClampNeg11: AS_GetArrayStateI(37)==2 -> RS 36, msg 59
int CompileWithColorClampNeg11(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 37, static_cast<u32>(aiStage)) == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 36, 0, 1);
        TraceVal(apfnPrint, aiChannel, 59, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 36, 0, 0);
        TraceVal(apfnPrint, aiChannel, 59, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C24108  ColorClampNone: AS_GetArrayStateI(37) nonzero -> RS 34 value 0
//               else value 1, msg 57
int CompileWithColorClampNone(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 37, static_cast<u32>(aiStage)))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 34, 0, 0);
        TraceVal(apfnPrint, aiChannel, 57, aiStage, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 34, 0, 1);
        TraceVal(apfnPrint, aiChannel, 57, aiStage, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C24560  EdgeFlag: AS_GetStateI(926)==1 -> RS 22 value 1/0, msg 36
int CompileWithEdgeFlag(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 926) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 22, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 36, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 22, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 36, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C224E0  FogBlendInRB: always emit RS 6, msg 8 (no pAS_Object assert)
int CompileWithFogBlendInRB(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    (void)apObject; (void)aiStage;
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 6, 0, 0);
    TraceValNoStage(apfnPrint, aiChannel, 8, 0);
    return 0;
}

// @ 0x82C22AE0  FogIndexOFog: AS_GetStateI(170)==2 -> RS 12 value 1/0, msg 18
int CompileWithFogIndexOFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 170) == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 12, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 18, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 12, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 18, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C22BF8  FogIndexZ: AS_GetStateI(170) nonzero -> RS 13 value 0 else 1, msg 19
int CompileWithFogIndexZ(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 170))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 13, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 19, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 13, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 19, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C24478  HOSMode: AS_GetStateI(971)==1 -> RS 37 value 1/0, msg 60 (no assert)
int CompileWithHOSMode(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 971) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 37, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 60, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 37, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 60, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C229D0  LineAA: AS_GetStateI(935) nonzero -> RS 11 value 1/0, msg 17
int CompileWithLineAA(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 935))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 11, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 17, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 11, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 17, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C225F0  LodClampEnable: always emit RS 38, msg 63
int CompileWithLodClampEnable(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 38, 0, 0);
    TraceVal(apfnPrint, aiChannel, 63, aiStage, 0);
    return 0;
}

// @ 0x82C22790  PixelFog: AS_GetStateI(163) && AS_GetStateI(165) -> RS 9 1/0, msg 15
int CompileWithPixelFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    int liFogEnable = AS_GetStateI(apObject, 163);
    int liFogBlend  = AS_GetStateI(apObject, 165);
    if (liFogEnable && liFogBlend)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 9, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 15, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 9, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 15, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C228C0  PointAA: AS_GetStateI(934) nonzero -> RS 10 value 1/0, msg 16
int CompileWithPointAA(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 934))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 10, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 16, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 10, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 16, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C238D0  PointModeShadowBuffering: AS_GetArrayStateI(23)==1 &&
//               AS_GetArrayStateI(20)==1 -> RS 25 value 1/0, msg 44
int CompileWithPointModeShadowBuffering(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liA = AS_GetArrayStateI(apObject, 20, static_cast<u32>(aiStage));
    int liResult;
    if (AS_GetArrayStateI(apObject, 23, static_cast<u32>(aiStage)) == 1 && liA == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 25, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 44, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 25, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 44, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C25EC0  PointSizeClamp: AS_GetArrayStateI(7)==1 -> RS 43 value 1
//               else RS 4 value 0, msg 105
int CompileWithPointSizeClamp(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 7, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 43, 0, 1);
        TraceVal(apfnPrint, aiChannel, 105, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 4, 0, 0);
        TraceVal(apfnPrint, aiChannel, 105, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C25FE8  PointSpriteReplaceModeNone: stage bounds; AS_GetArrayStateI(12)==1
//               -> RS 0 value 0 else value 1, msg 0
int CompileWithPointSpriteReplaceModeNone(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    CGS_ASSERT(aiStage < 16, "stage < 16");
    CGS_ASSERT(aiStage >= 0, "stage >= 0");
    int liResult;
    if (AS_GetArrayStateI(apObject, 12, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 0, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 0, aiStage, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 0, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 0, aiStage, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C26478  PointSpriteReplaceModeR: stage bounds; AS_GetStateI(135);
//               AS_GetArrayStateI(12)==1 && state135==2 -> RS 3 value 1/0, msg 3
int CompileWithPointSpriteReplaceModeR(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    CGS_ASSERT(aiStage < 16, "stage < 16");
    CGS_ASSERT(aiStage >= 0, "stage >= 0");
    int liMode = AS_GetStateI(apObject, 135);
    int liResult;
    if (AS_GetArrayStateI(apObject, 12, static_cast<u32>(aiStage)) == 1 && liMode == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 3, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 3, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 3, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 3, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C262E8  PointSpriteReplaceModeS: stage bounds; AS_GetStateI(135);
//               AS_GetArrayStateI(12)==1 && state135==1 -> RS 2 value 1/0, msg 2
int CompileWithPointSpriteReplaceModeS(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    CGS_ASSERT(aiStage < 16, "stage < 16");
    CGS_ASSERT(aiStage >= 0, "stage >= 0");
    int liMode = AS_GetStateI(apObject, 135);
    int liResult;
    if (AS_GetArrayStateI(apObject, 12, static_cast<u32>(aiStage)) == 1 && liMode == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 2, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 2, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 2, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 2, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C26158  PointSpriteReplaceModeZero: stage bounds; AS_GetStateI(135);
//               (AS_GetArrayStateI(12)!=1 || state135) -> RS 1 value 0 else 1, msg 1
int CompileWithPointSpriteReplaceModeZero(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    CGS_ASSERT(aiStage < 16, "stage < 16");
    CGS_ASSERT(aiStage >= 0, "stage >= 0");
    int liMode = AS_GetStateI(apObject, 135);
    int liResult;
    if (AS_GetArrayStateI(apObject, 12, static_cast<u32>(aiStage)) != 1 || liMode)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 1, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 1, aiStage, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 1, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 1, aiStage, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C22298  PointSprites: AS_GetStateI(127)==1 -> RS 4 value 1/0, msg 4
int CompileWithPointSprites(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 127) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 4, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 4, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 4, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 4, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23710  PolyStipple: AS_GetStateI(582)==1 -> RS 23 value 1/0, msg 38
int CompileWithPolyStipple(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 582) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 23, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 38, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 23, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 38, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C24968  ShadowBuffering: always emit RS 41 (stage 0,0), msg 101
int CompileWithShadowBuffering(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 41, 0, 0);
    TraceVal(apfnPrint, aiChannel, 101, aiStage, 0);
    return 0;
}

// @ 0x82C23B50  ShadowGEqualCompareFunc: AS_GetArrayStateI(0x17) nonzero ->
//               RS 27 value 0 else 1, msg 46
int CompileWithShadowGEqualCompareFunc(AS_Object* apObject, u32 auStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 0x17u, auStage))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 27, 0, 0);
        TraceVal(apfnPrint, aiChannel, 46, static_cast<int>(auStage), 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 27, 0, 1);
        TraceVal(apfnPrint, aiChannel, 46, static_cast<int>(auStage), 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C23C70  ShadowLEqualCompareFunc: AS_GetArrayStateI(23)==1 -> RS 28 1/0, msg 47
int CompileWithShadowLEqualCompareFunc(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 23, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 28, 0, 1);
        TraceVal(apfnPrint, aiChannel, 47, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 28, 0, 0);
        TraceVal(apfnPrint, aiChannel, 47, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23FE8  ShadowUsageAlpha: AS_GetArrayStateI(22) nonzero -> RS 30 0 else 1, msg 49
int CompileWithShadowUsageAlpha(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 22, static_cast<u32>(aiStage)))
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 30, 0, 0);
        TraceVal(apfnPrint, aiChannel, 49, aiStage, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 30, 0, 1);
        TraceVal(apfnPrint, aiChannel, 49, aiStage, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C23EC0  ShadowUsageIntensity: AS_GetArrayStateI(22)==2 -> RS 30 1/0, msg 49
int CompileWithShadowUsageIntensity(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 22, static_cast<u32>(aiStage)) == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 30, 0, 1);
        TraceVal(apfnPrint, aiChannel, 49, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 30, 0, 0);
        TraceVal(apfnPrint, aiChannel, 49, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23D98  ShadowUsageLuminance: AS_GetArrayStateI(22)==1 -> RS 29 1/0, msg 48
int CompileWithShadowUsageLuminance(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 22, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 29, 0, 1);
        TraceVal(apfnPrint, aiChannel, 48, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 29, 0, 0);
        TraceVal(apfnPrint, aiChannel, 48, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C22E20  TableFogALU: AS_GetStateI(171)==1 -> RS 40 value 1/0, msg 92
int CompileWithTableFogALU(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 171) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 40, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 92, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 40, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 92, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23050  TableFogExp: AS_GetStateI(165)==1 -> RS 16 value 1/0, msg 22
int CompileWithTableFogExp(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 165) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 16, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 22, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 16, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 22, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23168  TableFogExp2: AS_GetStateI(165)==2 -> RS 17 value 1/0, msg 23
int CompileWithTableFogExp2(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 165) == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 17, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 23, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 17, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 23, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C22F38  TableFogLinear: AS_GetStateI(165)==3 -> RS 15 value 1/0, msg 21
int CompileWithTableFogLinear(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 165) == 3)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 15, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 21, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 15, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 21, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C235E8  TexProjectedW: AS_GetArrayStateI(14)==3 -> RS 21 value 1/0, msg 35
int CompileWithTexProjectedW(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 14, static_cast<u32>(aiStage)) == 3)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 21, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 35, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 21, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 35, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C23398  TexProjectedY: AS_GetArrayStateI(14)==1 -> RS 19 value 1/0, msg 33
int CompileWithTexProjectedY(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 14, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 19, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 33, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 19, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 33, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C234C0  TexProjectedZ: AS_GetArrayStateI(14)==2 -> RS 20 value 1/0, msg 34
int CompileWithTexProjectedZ(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetArrayStateI(apObject, 14, static_cast<u32>(aiStage)) == 2)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 20, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 34, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 20, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 34, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C22698  TextureDimensionCube: AS_GetArrayStateI(27)==1 -> RS 8 value 1/0, msg 14
int CompileWithTextureDimensionCube(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    int liResult;
    if (AS_GetArrayStateI(apObject, 27, static_cast<u32>(aiStage)) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 8, aiStage, 1);
        TraceVal(apfnPrint, aiChannel, 14, aiStage, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 8, aiStage, 0);
        TraceVal(apfnPrint, aiChannel, 14, aiStage, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C24848  TransformedVertices: AS_GetStateI(157)==1 -> RS 39 value 0 else 1, msg 91
int CompileWithTransformedVertices(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    int liResult;
    if (AS_GetStateI(apObject, 157) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 39, 0, 0);
        TraceVal(apfnPrint, aiChannel, 91, aiStage, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 39, 0, 1);
        TraceVal(apfnPrint, aiChannel, 91, aiStage, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C23280  TwoSidedLighting: AS_GetStateI(615)==1 -> RS 18 value 1/0, msg 32
int CompileWithTwoSidedLighting(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liResult;
    if (AS_GetStateI(apObject, 615) == 1)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 18, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 32, 1);
        liResult = 1;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 18, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 32, 0);
        liResult = 0;
    }
    return liResult;
}

// @ 0x82C223B0  VertexFog: (!AS_GetStateI(163) || AS_GetStateI(165)) -> RS 5
//               value 0 else 1, msg 7
int CompileWithVertexFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    (void)aiStage;
    int liFogEnable = AS_GetStateI(apObject, 163);
    int liFogBlend  = AS_GetStateI(apObject, 165);
    int liResult;
    if (!liFogEnable || liFogBlend)
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 5, 0, 0);
        TraceValNoStage(apfnPrint, aiChannel, 7, 0);
        liResult = 0;
    }
    else
    {
        if (aiCompiledShader) CS_SetRequiredRenderState(aiCompiledShader, 5, 0, 1);
        TraceValNoStage(apfnPrint, aiChannel, 7, 1);
        liResult = 1;
    }
    return liResult;
}

// @ 0x82C23828  YUVConversion: always emit RS 24 (stage,0), msg 39
int CompileWithYUVConversion(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel)
{
    CGS_ASSERT(apObject, "pAS_Object");
    if (aiCompiledShader)
        CS_SetRequiredRenderState(aiCompiledShader, 24, aiStage, 0);
    TraceVal(apfnPrint, aiChannel, 39, aiStage, 0);
    return 0;
}

} // namespace XGRAPHICS
