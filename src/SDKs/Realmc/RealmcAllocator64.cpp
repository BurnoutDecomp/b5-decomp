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
// allocator<,64>::allocator @ 0x82C45B48  (export-truncated symbol `Rea`;
// ledger home class:<global>) -- the deque constructor.
//
// MEASURED, raw asm @ 0x82C45B48:
//   mr   r31, r3 ; li r11, 0
//   stw  r11, 0/4/8/0xC/0x10/0x14/0x18/0x1C/0x20/0x24(r31)   ; zero all 10 members
//   bl   RealmcCore::allocator<,64>::DoInit                   ; r3 = this, r4 = a2
//   mr   r3, r31 ; blr                                        ; return this
//
// PARAMETER COUNT: the X360 call site passes a third argument (r5 = the address
// of a 1-byte never-written stack temporary -- an empty/stateless allocator
// object by reference); neither this function nor DoInit reads it, so it is
// dropped on the host. The full measurement is in RealmcAllocator64.h on the
// declaration -- do NOT re-derive it as "the ctor takes one parameter".
//
// INFERENCE: the ten zero-stores are the member init-list; DoInit immediately
// rewrites all ten fields, so the zeroing is the compiler's belt-and-braces.
// Reproduced as an init-list in declaration order (== the X360 store order).
// ---------------------------------------------------------------------------
Allocator64::Allocator64(unsigned int nByteCapacity)
    : mppPageArray(nullptr), mnPageSlots(0),
      mpFrontCur(nullptr), mpFrontBegin(nullptr), mpFrontEnd(nullptr),
      mppFrontSlot(nullptr), mpBackCur(nullptr), mpBackBegin(nullptr),
      mpBackEnd(nullptr), mppBackSlot(nullptr)
{
    DoInit(nByteCapacity);
}

// ---------------------------------------------------------------------------
// allocator<,64>::~allocator @ 0x82C45A10  (export-truncated symbol `Re`;
// ledger home class:<global>) -- the deque destructor.
//
// MEASURED, raw asm @ 0x82C45A10:
//   r11 = [this+0x00] (mppPageArray) ; beq -> exit          ; nothing to tear down
//   r11 = [this+0x24] (mppBackSlot)  ; r4 = [this+0x14] (mppFrontSlot)
//   r5  = r11 + 4                                           ; ONE console slot
//   bl  RealmcCore::allocator<,64>::DoFreeSubar             ; (r3 = this, unused)
//   r4  = [this+0x00] ; beq -> exit                         ; redundant re-check
//   r3  = *off_832BE204 (g_pRealmcAllocator) ; r11 = [r3] ; r11 = [r11+0xC]
//   r10 = [this+0x04] (mnPageSlots) ; slwi r5, r10, 2 ; bctrl
//     == g_pRealmcAllocator->Free(mppPageArray, 4 * mnPageSlots)   [vtable +12]
//
// HOST-WIDTH, AND PAIRED WITH DoInit'S ALLOCATION. The `+ 4` on the back slot is
// ONE page-array slot -> `+ 1` in host `char**` arithmetic; the `slwi ..,2` size
// is `4 * mnPageSlots` only because a console slot is 4 bytes. Both console
// literals stay in comments. The sized free below MUST keep the same element
// width as `allocate(sizeof(char*) * luSlots, 0)` in DoInit -- host `char*` is 8
// bytes, so a console-width literal on either side alone is a live heap
// mismatch (MemcardState's capacity-0 deque floors at 8 slots: 32 bytes
// allocated vs 64 freed). They were landed together and must be changed together.
//
// INFERENCE: the second `if (mppPageArray)` is the inner guard of the original
// source's two-step teardown -- nothing between the two loads can write the
// field (DoFreeSubar @ 0x82C453C8 only reads the range and calls Free), so the
// two guards fold into one with identical behaviour.
//
// DECLARATION-SHAPE NOTE: DoFreeSubar's true X360 shape is a non-static member
// (r3 = this, unused; begin = r4, end = r5). The committed `static`
// two-parameter declaration binds the same (begin, end) pair, so this call is
// semantically exact.
// ---------------------------------------------------------------------------
Allocator64::~Allocator64()
{
    if (mppPageArray)
    {
        DoFreeSubar(mppFrontSlot, mppBackSlot + 1);   // X360: end = mppBackSlot + 4 bytes
        g_pRealmcAllocator->Free(
            mppPageArray,
            sizeof(char*) * static_cast<std::size_t>(mnPageSlots));  // X360: 4 * mnPageSlots
    }
}

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
//   mpFrontBegin (+0xC) = *v11 ; mpFrontEnd (+0x10) = *v11 + 0x100 ;
//   mpFrontCur (+0x8) = *v11                    -- the X360 store order exactly
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

    // HOST WIDTH (was a live under-allocation): the X360 sizes this array with
    // `slwi r4, r11, 2` @ 0x82C45608 -- 4 bytes per slot, the CONSOLE pointer
    // width. mppPageArray is `char**`, so on the LLP64 host a slot is 8 bytes and
    // `4u * luSlots` half-sized the block that DoInit then fills with luPages
    // pointers (and DoPushBack grows into). Sized by sizeof(char*) instead; the
    // console literal survives only in this comment.
    // PAIRED: ~Allocator64 above frees this exact block with
    // `sizeof(char*) * mnPageSlots`. The two widths must move together -- either
    // one alone reverting to the console 4 is a live heap mismatch.
    void* lpLastResult =
        RealmcCore::allocator::allocate(sizeof(char*) * luSlots, 0);  // X360: 4 * luSlots
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
    mpFrontBegin = *lppFront;                                   // +0x0C page begin
    mpFrontEnd   = *lppFront + KU_PageBytes;                    // +0x10 page end
    mpFrontCur   = *lppFront;                                   // +0x08 read cursor

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
//   v2 = mpFrontBegin (0xC) ; if (v2) backend->Free(v2, 0x100)   -> free front page
//   v3 = mppFrontSlot (0x14) + 4 ; mppFrontSlot = v3           -> advance one slot
//   v4 = *v3 ; mpFrontBegin = v4 ; mpFrontEnd = v4 + 0x100 ; mpFrontCur = v4
//
// CORRECTED (wave L): the comment here previously claimed the freed pointer was
// "not the page begin". It IS the page begin -- +0x0C is the deque iterator's
// `first` and +0x08 the moving read cursor (see the layout map + proof in
// RealmcAllocator64.h; the code was always offset-faithful, only the two member
// NAMES and the comments were inverted). Freeing +0x0C is what the X360 does and
// what is correct: you release a page, never a cursor.
// Returns the backend free result, or this when there was nothing to free (the
// X360 leaves r3 = this in that path).
// ---------------------------------------------------------------------------
void* Allocator64::DoPopFront()
{
    void* lpResult = this;
    if (mpFrontBegin)
    {
        g_pRealmcAllocator->Free(mpFrontBegin, KU_PageBytes);
        lpResult = mpFrontBegin;
    }

    ++mppFrontSlot;                 // mppFrontSlot += 1 slot (X360: +4 bytes)
    char* lpPage = *mppFrontSlot;
    mpFrontBegin = lpPage;                      // +0x0C page begin
    mpFrontEnd   = lpPage + KU_PageBytes;       // +0x10 page end
    mpFrontCur   = lpPage;                      // +0x08 read cursor
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

// ---------------------------------------------------------------------------
// allocator<,64>::PopFront -- the deque front-dequeue folded inline into
// RealmcCore::MemcardState::GetWaitingToStartTask @ 0x82C46528:
//
//   if (mpFrontCur == mpBackCur) return 0;         -> empty
//   value = *(int*)mpFrontCur;
//   next  = mpFrontCur + 4;
//   if (next == mpFrontEnd) DoPopFront();             -> page exhausted
//   else mpFrontCur = next;
//   return value;
//
// mpFrontCur (X360 +0x08) is the live front read cursor; mpFrontEnd (+0x10) is
// one past the current front page; mpBackCur (+0x18) is the back write cursor,
// so front == back means the deque is empty. The value/advance width is a 4-byte
// int -- the same element model DoPushBack writes.
// ---------------------------------------------------------------------------
int Allocator64::PopFront()
{
    if (mpFrontCur == mpBackCur)
        return 0;

    const int iValue = *reinterpret_cast<const int*>(mpFrontCur);
    char* lpNext = mpFrontCur + sizeof(int);
    if (lpNext == mpFrontEnd)
        DoPopFront();
    else
        mpFrontCur = lpNext;
    return iValue;
}

// ---------------------------------------------------------------------------
// allocator<,64>::PushBack -- the deque back-enqueue folded inline into
// RealmcCore::MemcardState::PutInStartWaitingQueue @ 0x82C468B8:
//
//   cur = mpBackCur;
//   if (cur + 4 == mpBackEnd) DoPushBack(&iValue);    -> page full
//   else { mpBackCur = cur + 4; if (cur) *(int*)cur = iValue; }
//
// mpBackCur (X360 +0x18) is the back write cursor and mpBackEnd (+0x20) one past
// the current back page; when the write would reach the page end the value is
// handed to DoPushBack (which stages a fresh page), otherwise the cursor advances
// and the value is written at the old slot (matching the X360 store order).
// ---------------------------------------------------------------------------
void Allocator64::PushBack(int iValue)
{
    char* lpCur = mpBackCur;
    if (lpCur + sizeof(int) == mpBackEnd)
    {
        DoPushBack(&iValue);
    }
    else
    {
        mpBackCur = lpCur + sizeof(int);
        if (lpCur)
            *reinterpret_cast<int*>(lpCur) = iValue;
    }
}

} // namespace RealmcCore
