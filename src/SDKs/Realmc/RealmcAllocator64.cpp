#include "SDKs/Realmc/RealmcAllocator64.h"

#include "SDKs/Realmc/RealmcCore.h"  // RealmcCore::allocator (tagged alloc) / g_pRealmcAllocator

// ===========================================================================
// RealmcCore::allocator<,64> -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcAllocator64.h for the per-offset layout map. Every page / page-array
// allocation routes through the shared Realmc backend (g_pRealmcAllocator),
// stamped with the "RealmcCore::allocator" tag, matching the stateless adaptor
// allocator in RealmcCore.cpp.
//
// PAGE-ARRAY POINTER ARITHMETIC NOTE. The X360 computes the centring offset and
// the per-page run length in BYTES over a 4-byte-pointer page array (v10 / 4*v4
// are byte spans of dword slots). On the 64-bit host the array elements are 8
// bytes, so the same logical slot counts are reproduced by dividing those X360
// byte spans by the X360 pointer width (4) to recover the slot index, then
// indexing the char*[] array by NAME. This keeps the front/back slots, page
// count and cursor placement semantically identical without asserting the X360
// pointer width on the host.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// allocator<,64>::DoInit @ 0x82C455C0
//
//   srwi r11, a2, 6 ; addi r30, r11, 1          -> v4 = (a2 >> 6) + 1   (pages)
//   addi r11, r30, 2 ; ... max(., 8)            -> v6 = max((a2>>6)+3, 8) (slots)
//   stw  v6, 4(this)                            -> mnPageSlots = v6
//   backend->Allocate(4*v6, "RealmcCore::allocator", 0) ; stw r3, 0(this)
//                                               -> mppPageArray = array of v6 ptrs
//   r10 = v6 - v4 ; extlwi -> v10 = (2*(v6-v4)) & ~3   (centring byte offset)
//   v11 = mppPageArray + v10 ; v12 = v11 + 4*v4 (front..back slot run, bytes)
//   for (slot = v11; slot < v12; slot += 4)     -> fill each slot with a 256B page
//        *slot = backend->Allocate(0x100, tag, 0)
//   stw  v11, 0x14(this)                        -> mppFrontSlot = v11
//   mpFrontCur = mpFrontBegin = *v11 ; mpFrontEnd = *v11 + 0x100
//   stw  v12-4, 0x24(this)                      -> mppBackSlot = v12 - 4
//   mpBackBegin = *(v12-4) ; mpBackEnd = *(v12-4) + 0x100
//   mpBackCur = ((a2 & 0x3F) << 2) + *(v12-4)
//
// The byte spans v10 (centring) and 4*v4 (page run) are dword-slot spans on the
// X360; here they are converted to slot counts (/4) and applied to the char*[]
// array by name. The last allocation result is returned (X360 returns r3).
// ---------------------------------------------------------------------------
void* Allocator64::DoInit(unsigned int nByteCapacity)
{
    const unsigned int luPages = (nByteCapacity >> KU_ElementLog) + 1u;     // v4
    unsigned int luSlots = (nByteCapacity >> KU_ElementLog) + 3u;            // v17
    if (luSlots <= 8u)                                                        // max(., 8)
        luSlots = 8u;
    mnPageSlots = static_cast<int>(luSlots);

    void* lpLastResult = RealmcCore::allocator::allocate(4u * luSlots, 0);    // page-ptr array
    mppPageArray = static_cast<char**>(lpLastResult);

    // v10 = (2*(slots - pages)) & ~3  -- a byte offset over 4-byte slots; the
    // recovered slot index is v10 / 4 (the X360 pointer width).
    const unsigned int luCentreBytes =
        (2u * (luSlots - luPages)) & 0xFFFFFFFCu;
    const unsigned int luFrontSlotIndex = luCentreBytes / 4u;

    char** lppFront = mppPageArray + luFrontSlotIndex;          // v11
    char** lppEnd   = lppFront + luPages;                       // v12 == v11 + 4*v4 (in slots)

    for (char** lppSlot = lppFront; lppSlot < lppEnd; ++lppSlot)
    {
        lpLastResult = RealmcCore::allocator::allocate(KU_PageBytes, 0);      // 256-byte page
        *lppSlot = static_cast<char*>(lpLastResult);
    }

    mppFrontSlot = lppFront;
    mpFrontCur   = *lppFront;
    mpFrontEnd   = *lppFront + KU_PageBytes;
    mpFrontBegin = *lppFront;

    char** lppBack = lppEnd - 1;                                // v12 - 4 (one slot back)
    mppBackSlot = lppBack;
    mpBackBegin = *lppBack;
    mpBackEnd   = *lppBack + KU_PageBytes;
    mpBackCur   = *lppBack + ((nByteCapacity & 0x3Fu) << 2);    // (a2 & 0x3F) << 2

    return lpLastResult;
}

// ---------------------------------------------------------------------------
// allocator<,64>::DoPopFront @ 0x82C45350
//
//   v2 = mpFrontCur (0xC) ; if (v2) backend->Free(v2, 0x100)   -> free front page
//   v3 = mppFrontSlot (0x14) + 4 ; mppFrontSlot = v3           -> advance one slot
//   v4 = *v3 ; mpFrontCur = v4 ; mpFrontEnd = v4 + 0x100 ; mpFrontBegin = v4
//
// The freed pointer is mpFrontCur (what the X360 reads at +0xC), not the page
// begin; reproduced exactly. Returns the backend free result, or this when there
// was nothing to free (the X360 leaves r3 = this in that path).
// ---------------------------------------------------------------------------
void* Allocator64::DoPopFront()
{
    void* lpResult = this;
    if (mpFrontCur)
    {
        g_pRealmcAllocator->Free(mpFrontCur, KU_PageBytes);
        lpResult = mpFrontCur;
    }

    ++mppFrontSlot;                 // mppFrontSlot += 1 slot (X360: +4 bytes)
    char* lpPage = *mppFrontSlot;
    mpFrontCur   = lpPage;
    mpFrontEnd   = lpPage + KU_PageBytes;
    mpFrontBegin = lpPage;
    return lpResult;
}

// ---------------------------------------------------------------------------
// allocator<,64>::DoPushBack @ 0x82C45A80
//
//   v3 = *a2                                                    -> the dword to push
//   if (((mppBackSlot - mppPageArray) >> 2) + 1 >= mnPageSlots) -> slots exhausted?
//        DoReallocPt(1, 1)                                       -> grow the array
//   page = backend->Allocate(0x100, tag, 0)
//   *(mppBackSlot + 4) = page                                   -> stage page in next slot
//   if (mpBackCur) *mpBackCur = v3                               -> write value at cursor
//   mppBackSlot += 4 ; v7 = *mppBackSlot                         -> advance onto the page
//   mpBackBegin = v7 ; mpBackEnd = v7 + 0x100 ; mpBackCur = v7
//
// The capacity test ((mppBackSlot - mppPageArray)/4 + 1 >= mnPageSlots) is a slot-
// index compare; on the host char*[] pointer subtraction already yields slots, so
// no /4 is needed. Returns the freshly allocated page (X360 returns r3).
// ---------------------------------------------------------------------------
void* Allocator64::DoPushBack(const int* pValue)
{
    const int liValue = *pValue;                       // v3 = *a2

    if ((mppBackSlot - mppPageArray) + 1 >= mnPageSlots)
        DoReallocPt(1, 1);

    void* lpPageRaw = RealmcCore::allocator::allocate(KU_PageBytes, 0);
    char* lpPage = static_cast<char*>(lpPageRaw);

    mppBackSlot[1] = lpPage;                            // *(mppBackSlot + 4) = page

    if (mpBackCur)
        *reinterpret_cast<int*>(mpBackCur) = liValue;   // *mpBackCur = v3

    ++mppBackSlot;                                      // mppBackSlot += 1 slot
    char* lpNew = *mppBackSlot;                         // v7 = *mppBackSlot
    mpBackBegin = lpNew;
    mpBackEnd   = lpNew + KU_PageBytes;
    mpBackCur   = lpNew;
    return lpPageRaw;
}

// ---------------------------------------------------------------------------
// allocator<,64>::DoFreeSubar @ 0x82C453C8
//
//   for (i = a2; i < a3; ++i)                                   -> i is a char**
//        if (*i) backend->Free(*i, 0x100)                       -> free each page
//
// Frees every live page pointer in the half-open slot range [ppBegin, ppEnd).
// ---------------------------------------------------------------------------
void Allocator64::DoFreeSubar(char** ppBegin, char** ppEnd)
{
    for (char** lppSlot = ppBegin; lppSlot < ppEnd; ++lppSlot)
    {
        if (*lppSlot)
            g_pRealmcAllocator->Free(*lppSlot, KU_PageBytes);
    }
}

} // namespace RealmcCore
