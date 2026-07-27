// ============================================================================
// SDKs/EATech/include/snd/sndaems.cpp
//
// Snd9::Aems AEMS module-bank manager bodies. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every offset,
// width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU.
//   Snd9::Aems::BeginRemoveModuleBank    @ 0x82B72250
//   Snd9::Aems::IsModuleBankRemoved      @ 0x82B71720
//   Snd9::Aems::SetSamplePlayerFactory   @ 0x82B6FCD0
//
// The `_savegprlr_23` / `_restgprlr_23` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so
// they are dropped. off_83271928 is the shared rwaudio System singleton (same
// global reached by Decoder.cpp / PlugIn.cpp), whose Lock/Unlock guard the registry
// walks and whose deferred-command ring receives the queued remove command.
// ============================================================================

#include "SDKs/EATech/include/snd/sndaems.h"

#include "SDKs/Csis/CsisClass.h"   // Csis::ClassHandle, Csis::ClassClientNode, Csis::Class::UnsubscribeConstructorFast
#include "rw/audio/core/PlugIn.h"  // rw::audio::core::System (Lock / Unlock / Free + command ring)

// The shared rwaudio System singleton (off_83271928). Defined/owned by the System
// sub-system TU; declared here as the target of Lock/Unlock/Free and the command
// ring. Same declaration form Decoder.cpp uses.
extern "C" rw::audio::core::System* off_83271928;

// The C-facing AEMS instance layer's per-subscriber teardown (SNDAEMSI_updatedestroy,
// paired with Csis::Function::UnsubscribeFast). Its body lives in its own TU;
// declared here so BeginRemoveModuleBank's instance walk can call it. FLAGGED extern.
extern "C" int SNDAEMSI_updatedestroy(void* apSubscriber);

namespace Snd9
{
namespace Aems
{

// Result of BeginRemoveModuleBank when no registered bank matches the handle
// (X360: `li r31, -0xA`).
static const s32 KI_MODULE_BANK_NOT_FOUND = -10;

// The process-global AEMS module-bank registry head (off_832775F0). Holds the
// address of the first bank's embedded link (bank +0x50), not the bank base. Banks
// are pushed onto it by the SNDAEMSI create path (its own TU); the remove/query
// paths here only read it. Null until the first bank registers.
ModuleBankLink* gpModuleBankListHead = 0;

// Whether the AEMS module system is active (byte_83277864). While false the query
// path short-circuits (every bank reports removed). Set by AEMS init (its own TU).
bool gbAemsActive = false;

// The process-global AEMS sample-player factory (off_82F87DBC), installed by
// SetSamplePlayerFactory.
IAemsSamplePlayerFactory* gpSamplePlayerFactory = 0;

// One record slot queued into the audio System's deferred-command ring by
// BeginRemoveModuleBank; replayed on the audio thread by Snd9::RemoveModuleBankHandler.
// The X360 lays it out in 12 bytes (4-byte handler pointer); on the host it is 16.
// Offsets in the member comments are the console ones the asm encodes.
struct RemoveModuleBankCommand
{
    int (*mpfnHandler)(void* apCommand); // +0x00  &Snd9::RemoveModuleBankHandler
    u32   muReserved;                    // +0x04  always 0
    s32   miBankHandle;                  // +0x08  the removed bank's handle
};

// ----------------------------------------------------------------------------
// Snd9::Aems::BeginRemoveModuleBank @ 0x82B72250
// ----------------------------------------------------------------------------
s32 BeginRemoveModuleBank(s32 aiBankHandle)
{
    rw::audio::core::System* lpSystem = off_83271928;
    rw::audio::core::System::Lock(lpSystem);

    // Locate the registered bank whose handle matches (registry is threaded through
    // each bank's embedded +0x50 link, so recover the owning bank with container-of).
    ModuleBank* lpBank = 0;
    for (ModuleBankLink* lpLink = gpModuleBankListHead; lpLink; lpLink = lpLink->mpNext)
    {
        ModuleBank* lpCandidate = ModuleBankFromLink(lpLink);
        if (lpCandidate->miBankHandle == aiBankHandle)
        {
            lpBank = lpCandidate;
            break;
        }
    }

    s32 liResult;
    if (!lpBank)
    {
        liResult = KI_MODULE_BANK_NOT_FOUND;
    }
    else
    {
        // Walk the bank's packed, variable-stride module-record table (compiled AEMS
        // bank data at a self-relative byte offset from the bank base).
        char*     lpcRecord     = reinterpret_cast<char*>(lpBank) + lpBank->miModuleTableOffset;
        const u32 luModuleCount = lpBank->muModuleCount;
        for (u32 luModule = 0; luModule < luModuleCount; ++luModule)
        {
            // Unsubscribe this module's Csis constructor client (record +0x04 =
            // Csis::ClassHandle* passed by address; +0x0C = the embedded client node).
            Csis::ClassHandle**    lppHandle = reinterpret_cast<Csis::ClassHandle**>(lpcRecord + 0x04);
            Csis::ClassClientNode* lpNode    = reinterpret_cast<Csis::ClassClientNode*>(lpcRecord + 0x0C);
            Csis::Class::UnsubscribeConstructorFast(lppHandle, lpNode);

            // Flag + update-destroy every live instance on this module's instance list
            // (record +0x38 head; each instance carries a subscriber sub-object at the
            // record-driven byte offset record +0x34).
            const s32 liInstanceOffset = *reinterpret_cast<const s32*>(lpcRecord + 0x34); // serialized AEMS record field
            char* lpcInstance = *reinterpret_cast<char**>(lpcRecord + 0x38);              // serialized AEMS instance-list head
            while (lpcInstance)
            {
                char* lpcNext       = *reinterpret_cast<char**>(lpcInstance); // instance +0x00 next link
                char* lpcSubscriber = lpcInstance + liInstanceOffset;
                *reinterpret_cast<u32*>(lpcSubscriber + 0x0C) = 1;            // serialized: mark subscriber destroy-pending
                SNDAEMSI_updatedestroy(lpcSubscriber);
                lpcInstance = lpcNext;
            }

            // Advance to the next record: fixed 60-byte header plus this module's
            // packed input/output tables (byte counts at record +0x24 and +0x27),
            // i.e. (inputs + outputs + 15) * 4 bytes.
            const u32 luInputCount  = static_cast<u8>(lpcRecord[0x24]);
            const u32 luOutputCount = static_cast<u8>(lpcRecord[0x27]);
            lpcRecord += (luInputCount + luOutputCount + 15u) * 4u;
        }

        // Free the bank's auxiliary allocation, mark it removed, and queue a deferred
        // RemoveModuleBankHandler command onto the System's command ring (one
        // RemoveModuleBankCommand slot -- 12 bytes on the X360).
        if (lpBank->mpAllocatedBlock)
        {
            rw::audio::core::System::Free(lpSystem, lpBank->mpAllocatedBlock, 0);
        }
        lpBank->miRemoved = 1;

        const u32 luCursor = lpSystem->muDeferredRingCursor;
        RemoveModuleBankCommand* lpCmd = reinterpret_cast<RemoveModuleBankCommand*>(
            lpSystem->mpDeferredRingBase + luCursor);
        // RECORD STRIDE (X360-literal trap): the asm's `addi r10,r10,0xC` @0x82B72358 IS
        // the console sizeof(RemoveModuleBankCommand) ({int(*)(void*), u32, s32} = 4+4+4).
        // The host record is 16 (the handler pointer widened), and the ring consumer
        // advances by the handler's return, so the enqueue stride must be the HOST sizeof
        // rather than the literal 12. Snd9::RemoveModuleBankHandler is declared in
        // sndaems.h but not yet bodied anywhere in the tree; when its TU is reconstructed
        // it must return this same sizeof, or producer and consumer will disagree.
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(RemoveModuleBankCommand)); // X360: +0xC
        lpCmd->mpfnHandler  = &Snd9::RemoveModuleBankHandler;
        lpCmd->muReserved   = 0;
        lpCmd->miBankHandle = aiBankHandle;
        liResult = 0;
    }

    rw::audio::core::System::Unlock(lpSystem);
    return liResult;
}

// ----------------------------------------------------------------------------
// Snd9::Aems::IsModuleBankRemoved @ 0x82B71720
// ----------------------------------------------------------------------------
bool IsModuleBankRemoved(s32 aiBankHandle)
{
    // While the AEMS system is inactive there are no live banks: report removed
    // without touching the registry lock.
    if (!gbAemsActive)
    {
        return true;
    }

    rw::audio::core::System* lpSystem = off_83271928;
    rw::audio::core::System::Lock(lpSystem);

    bool lbRemoved = true;
    for (ModuleBankLink* lpLink = gpModuleBankListHead; lpLink; lpLink = lpLink->mpNext)
    {
        if (ModuleBankFromLink(lpLink)->miBankHandle == aiBankHandle)
        {
            lbRemoved = false;
            break;
        }
    }

    rw::audio::core::System::Unlock(lpSystem);
    return lbRemoved;
}

// ----------------------------------------------------------------------------
// Snd9::Aems::SetSamplePlayerFactory @ 0x82B6FCD0
// ----------------------------------------------------------------------------
IAemsSamplePlayerFactory* SetSamplePlayerFactory(IAemsSamplePlayerFactory* apFactory)
{
    gpSamplePlayerFactory = apFactory;
    return apFactory;
}

} // namespace Aems
} // namespace Snd9
