#include "SDKs/XAudio/XAudioEffectManager.h"

#include <string.h> // memcpy (registered-table growth copy)

#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager

// ===========================================================================
// XAUDIO::CEffectManager -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioEffectManager.h for the layout, the factory-table structures and
// the asm-grounded member offsets. No reference source and no DWARF exist for
// this TU; the PowerPC asm is authoritative.
// ===========================================================================

namespace XAUDIO
{

// The COM-style status the dispatchers return when no factory is registered for
// a requested effect id (X360 `lis r3,-0x8000 ; ori r3,r3,0x4001` -> 0x80004001
// == E_NOTIMPL).
static const s32 KI_E_NOTIMPL = static_cast<s32>(0x80004001);

// The status RegisterEffects returns when the table would exceed 128 slots or
// the growth allocation fails (X360 `lis r3,-0x7FF9 ; ori r3,r3,0xE` ->
// 0x8007000E == E_OUTOFMEMORY).
static const s32 KI_E_OUTOFMEMORY = static_cast<s32>(0x8007000E);

// The XMem allocate/free attribute word the registered-table growth path bakes
// in (`lis r5,0x6182 ; ori r5,r5,4` -> 0x61820004).
static const u32 KU_XMEM_ATTRIBUTES = 0x61820004u;

// The high-id base RegisterEffects assigns (registered ids start at 128; the id
// dispatchers subtract it before indexing the registered table).
static const u32 KU_REGISTERED_ID_BASE = 0x80u;

// @ 0x82C2F420 (also inlined into the ctor) -- scan a registration table for the
// highest effect id it uses; the builtin table is indexed directly by id, so it
// needs (highest id + 1) slots (0 for an empty table).
u32 CEffectManager::ComputeSlotCount(const SEffectRegistrationTable* apTable)
{
    u32 luSlotCount = 0;
    u32 luRemaining = apTable->muCount;
    if (luRemaining != 0)
    {
        const SEffectRegistration* lpEntry = apTable->mpEntries;
        do
        {
            u32 luNeeded = static_cast<u32>(lpEntry->muEffectId) + 1;
            if (luNeeded > luSlotCount)
                luSlotCount = luNeeded;
            ++lpEntry;
            --luRemaining;
        } while (luRemaining != 0);
    }
    return luSlotCount;
}

// @ 0x82C2F480
CEffectManager::CEffectManager(SEffectRegistrationTable* const* appTable,
                               IEffectManagerAllocator* apAllocator)
{
    miRefCount = 1;

    const SEffectRegistrationTable* lpTable = *appTable;

    // Size the builtin table so it can be indexed directly by effect id, and
    // allocate it through the supplied allocator (only when non-empty; the asm
    // leaves mpBuiltinEffects untouched otherwise).
    muBuiltinCount = ComputeSlotCount(lpTable);
    if (muBuiltinCount != 0)
    {
        // X360 strides these at 8 bytes (two 4-byte pointers); on x64 the entry
        // widens, so the allocation is sized by sizeof to match the by-index
        // stride below.
        mpBuiltinEffects = static_cast<SEffectFactory*>(apAllocator->mpVTable->mpAllocate(
            apAllocator, sizeof(SEffectFactory) * muBuiltinCount));
    }

    // Populate builtin[id] = { querySize, create } for each registration entry.
    if (lpTable->muCount != 0)
    {
        u32 luIndex = 0;
        do
        {
            const SEffectRegistration& lrEntry = lpTable->mpEntries[luIndex];
            SEffectFactory& lrSlot = mpBuiltinEffects[lrEntry.muEffectId];
            lrSlot.mpfnQuerySize = lrEntry.mpfnQuerySize;
            lrSlot.mpfnCreate    = lrEntry.mpfnCreate;
            ++luIndex;
        } while (luIndex < lpTable->muCount);
    }
}

// @ 0x82C2F598 -- the X360 `vector deleting destructor' entry point; the asm
// reseats CEffectManager's vtable (off_821B553C), frees the registered table,
// then reseats the shared base vtable (off_82108FF4) and returns this. The two
// vtable reseats are the compiler's base/derived destructor sequence; the only
// hand-written work is releasing the registered table.
CEffectManager::~CEffectManager()
{
    if (mpRegisteredEffects != nullptr)
    {
        gXMemMemoryManager.XMemFree(mpRegisteredEffects, KU_XMEM_ATTRIBUTES);
        mpRegisteredEffects = nullptr;
    }
}

// @ 0x82C2F078
void* CEffectManager::QueryInterface(void** appInterface)
{
    // The requested interface is the object's effect-table view, which begins at
    // byte +8 (past the vptr and refcount); the asm guards against a null object
    // before forming the pointer (COM sub-interface aggregation, not a member
    // poke). Returns the object unchanged.
    void* lpInterface = nullptr;
    if (this != nullptr)
        lpInterface = reinterpret_cast<u8*>(this) + 8;
    *appInterface = lpInterface;
    return this;
}

// @ 0x82C2F090
s32 CEffectManager::QueryEffectSize(const SEffectRequest* apRequest, void* apArg)
{
    const SEffectFactory* lpFactory = nullptr;

    u32 luId = apRequest->muEffectId;
    if (luId > 0x7F)
    {
        luId -= KU_REGISTERED_ID_BASE;
        if (luId < muRegisteredCount)
            lpFactory = &mpRegisteredEffects[luId];
    }
    else if (luId < muBuiltinCount)
    {
        lpFactory = &mpBuiltinEffects[luId];
    }

    // A usable slot has both functions non-zero; otherwise no factory is
    // registered for the id.
    if (lpFactory == nullptr ||
        lpFactory->mpfnCreate == nullptr || lpFactory->mpfnQuerySize == nullptr)
    {
        return KI_E_NOTIMPL;
    }

    return lpFactory->mpfnQuerySize(apRequest, apArg);
}

// @ 0x82C2F118
s32 CEffectManager::CreateEffect(const SEffectRequest* apRequest,
                                 void* apArg1, void* apArg2)
{
    const SEffectFactory* lpFactory = nullptr;

    u32 luId = apRequest->muEffectId;
    if (luId > 0x7F)
    {
        luId -= KU_REGISTERED_ID_BASE;
        if (luId < muRegisteredCount)
            lpFactory = &mpRegisteredEffects[luId];
    }
    else if (luId < muBuiltinCount)
    {
        lpFactory = &mpBuiltinEffects[luId];
    }

    if (lpFactory == nullptr ||
        lpFactory->mpfnCreate == nullptr || lpFactory->mpfnQuerySize == nullptr)
    {
        return KI_E_NOTIMPL;
    }

    return lpFactory->mpfnCreate(apRequest, apArg1, apArg2);
}

// @ 0x82C2F1A0
s32 CEffectManager::RegisterEffects(const SEffectRegistrationTable* apTable)
{
    const u32 luCount = apTable->muCount;

    // Pass 1: place as many effects as possible into free holes in the existing
    // registered table (a slot is free when both its words are zero). The scan
    // cursor advances continuously across the batch.
    u32 luPlaced = 0;
    if (luCount != 0)
    {
        SEffectFactory* lpBase = mpRegisteredEffects;
        SEffectFactory* lpEnd  = lpBase + muRegisteredCount;
        SEffectFactory* lpSlot = nullptr;
        do
        {
            lpSlot = (lpSlot == nullptr) ? lpBase : (lpSlot + 1);
            if (lpSlot >= lpEnd)
                break;

            bool lbRanOff = false;
            while (lpSlot->mpfnCreate != nullptr || lpSlot->mpfnQuerySize != nullptr)
            {
                ++lpSlot;
                if (lpSlot >= lpEnd)
                {
                    lbRanOff = true;
                    break;
                }
            }
            if (lbRanOff)
                break;

            SEffectRegistration& lrEntry = apTable->mpEntries[luPlaced];
            lrEntry.muEffectId = static_cast<u8>((lpSlot - lpBase) + KU_REGISTERED_ID_BASE);
            lpSlot->mpfnQuerySize = lrEntry.mpfnQuerySize;
            lpSlot->mpfnCreate    = lrEntry.mpfnCreate;
            ++luPlaced;
        } while (luPlaced < luCount);
    }

    // Pass 2: grow the table for the entries that did not fit in an existing
    // hole.
    const u32 luRemaining = luCount - luPlaced;
    if (luRemaining == 0)
        return 0;

    const u32 luNewCount = muRegisteredCount + luRemaining;
    if (luNewCount > 0x80)
        return KI_E_OUTOFMEMORY;

    SEffectFactory* lpNewTable = static_cast<SEffectFactory*>(
        gXMemMemoryManager.XMemAlloc(sizeof(SEffectFactory) * luNewCount, KU_XMEM_ATTRIBUTES));
    if (lpNewTable == nullptr)
        return KI_E_OUTOFMEMORY;

    const u32 luOldCount = muRegisteredCount;
    if (luOldCount != 0)
    {
        memcpy(lpNewTable, mpRegisteredEffects, sizeof(SEffectFactory) * luOldCount);
        if (mpRegisteredEffects != nullptr)
        {
            gXMemMemoryManager.XMemFree(mpRegisteredEffects, KU_XMEM_ATTRIBUTES);
            mpRegisteredEffects = nullptr;
        }
    }

    mpRegisteredEffects = lpNewTable;
    muRegisteredCount   = luOldCount + luRemaining;

    // Append the remaining entries after the copied-over slots.
    for (u32 luK = 0; luK < luRemaining; ++luK)
    {
        SEffectRegistration& lrEntry = apTable->mpEntries[luPlaced + luK];
        SEffectFactory* lpSlot = &lpNewTable[luOldCount + luK];
        lrEntry.muEffectId = static_cast<u8>(
            (lpSlot - mpRegisteredEffects) + KU_REGISTERED_ID_BASE);
        lpSlot->mpfnQuerySize = lrEntry.mpfnQuerySize;
        lpSlot->mpfnCreate    = lrEntry.mpfnCreate;
    }

    return 0;
}

// @ 0x82C2F3A8
s32 CEffectManager::UnregisterEffects(const SEffectIdList* apIds)
{
    for (u32 luI = 0; luI < apIds->muCount; ++luI)
    {
        u32 luId = static_cast<u32>(apIds->mpIds[luI]) - KU_REGISTERED_ID_BASE;
        if (luId < muRegisteredCount)
        {
            SEffectFactory* lpSlot = &mpRegisteredEffects[luId];
            // Only clear a fully-occupied slot (the asm also re-checks the slot
            // pointer, a defensive guard preserved here).
            if (lpSlot->mpfnCreate != nullptr &&
                lpSlot->mpfnQuerySize != nullptr &&
                lpSlot != nullptr)
            {
                lpSlot->mpfnQuerySize = nullptr;
                lpSlot->mpfnCreate    = nullptr;
            }
        }
    }
    return 0;
}

// @ 0x82C2F420
s32 CEffectManager::GetObjectAdditionalSize(const SEffectRegistrationTable* const* appTable)
{
    if (appTable == nullptr)
        return 0;

    const SEffectRegistrationTable* lpTable = *appTable;
    if (lpTable == nullptr)
        return 0;

    return static_cast<s32>(8 * ComputeSlotCount(lpTable));
}

} // namespace XAUDIO
