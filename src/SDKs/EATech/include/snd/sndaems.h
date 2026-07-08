#ifndef SDKS_EATECH_SND_SNDAEMS_H
#define SDKS_EATECH_SND_SNDAEMS_H

#include <cstddef> // offsetof

#include "types.hpp"

#include "SDKs/EATech/include/snd/sndo.h" // Snd9::IAemsSamplePlayerFactory

// ============================================================================
// SDKs/EATech/include/snd/sndaems.h  (Snd9 AEMS module-bank manager home)
//
// EATech "Snd9" AEMS (Audio Engine Module System) module-bank registry. Sibling
// of the sndo.h sample-player surface. This header is the canonical OWNING home
// for the Snd9::Aems free-function manager reconstructed from
// BURNOUT_X360_ARTIST.XEX (the PowerPC asm is authoritative for every offset,
// width and side-effect; no Feb-2007 source and no DecFIGS DWARF exist for this
// TU):
//     Snd9::Aems::BeginRemoveModuleBank    @ 0x82B72250
//     Snd9::Aems::IsModuleBankRemoved      @ 0x82B71720
//     Snd9::Aems::SetSamplePlayerFactory   @ 0x82B6FCD0
//
// A loaded AEMS bank is registered as a Snd9::Aems::ModuleBank node threaded onto
// a process-global intrusive list (registry head off_832775F0). Each node carries
// its bank handle, a live-instance/module table, and an auxiliary allocation freed
// on teardown. `Snd9`/`Aems` are an EATech vendor boundary, so those identifiers
// are preserved verbatim per the naming convention.
//
// LAYOUT (ModuleBank, from the X360 asm): the intrusive link the registry threads
// through sits at bank +0x50, so the registry head and every mpNext point at the
// NEXT bank's link (not its base) -- classic EA CListDStack threading, so the
// owning bank is recovered with container-of (ModuleBankFromLink). The +0xNN
// annotations below are the X360 (32-bit-pointer) offsets and are documentary
// only; members are declared with x64 widths so only member ORDER is load-bearing
// and every field is accessed by name (no offset asserts on the pointer tail).
// ============================================================================

namespace Snd9
{

// ----------------------------------------------------------------------------
// Snd9::RemoveModuleBankHandler -- the deferred module-bank-remove command's
// replay callback (a free function in namespace Snd9). Its ADDRESS is queued into
// the rw audio System command ring by Aems::BeginRemoveModuleBank and replayed
// later on the audio thread. Its body lives in its own (not-yet-reconstructed) TU;
// declared here so the queue store can take its address. Signature modelled on the
// sibling ring handlers (e.g. rw::audio::core::PlugIn::SetAttributeHandler): it
// receives the command record and returns the consumed command size. FLAGGED
// vendor extern -- declared, not defined, here.
// ----------------------------------------------------------------------------
int RemoveModuleBankHandler(void* apCommand);

namespace Aems
{

// One intrusive list link, embedded in every ModuleBank at +0x50. The registry
// head and each mpNext point at the *next* bank's embedded link.
struct ModuleBankLink
{
    ModuleBankLink* mpNext; // +0x00  (bank +0x50)  0 == end of list
};

// A registered AEMS module bank. Header fields are the ones the remove/query paths
// touch; the packed, variable-stride module-record table lives at a self-relative
// byte offset (miModuleTableOffset) and is walked as compiled AEMS data.
struct ModuleBank
{
    u8    mPad00[0x0A];                 // +0x00
    u16   muModuleCount;                // +0x0A  number of module records in the table
    u8    mPad0C[0x1C - 0x0C];          // +0x0C
    s32   miModuleTableOffset;          // +0x1C  self-relative byte offset to the module table
    u8    mPad20[0x3C - 0x20];          // +0x20
    s32   miBankHandle;                 // +0x3C  bank id matched by the remove/query paths
    u8    mPad40[0x44 - 0x40];          // +0x40
    s32   miRemoved;                    // +0x44  set to 1 by BeginRemoveModuleBank
    void* mpAllocatedBlock;             // +0x48  auxiliary allocation freed on teardown
    ModuleBankLink mLink;               // +0x50  intrusive registry link
};

// Recover the owning bank from its embedded registry link (container-of). The
// registry head and every mpNext point at &bank.mLink, not the bank base.
inline ModuleBank* ModuleBankFromLink(ModuleBankLink* apLink)
{
    return reinterpret_cast<ModuleBank*>(
        reinterpret_cast<char*>(apLink) - offsetof(ModuleBank, mLink));
}

// @ 0x82B72250. Begin removing the bank whose handle is aiBankHandle: unsubscribe
// each module's Csis constructor client, flag+update-destroy every live instance,
// free the bank's auxiliary block, mark it removed, and queue a deferred
// RemoveModuleBankHandler command onto the audio System's command ring. Returns 0
// on success, or -10 when no registered bank matches the handle.
s32 BeginRemoveModuleBank(s32 aiBankHandle);

// @ 0x82B71720. Report whether the bank whose handle is aiBankHandle is no longer
// registered (removed). Returns true when the AEMS system is inactive or no
// registered bank matches; false while the bank is still present.
bool IsModuleBankRemoved(s32 aiBankHandle);

// @ 0x82B6FCD0. Install the process-global AEMS sample-player factory and return
// it (called from CgsSound::Playback::AemsFactory's constructor).
IAemsSamplePlayerFactory* SetSamplePlayerFactory(IAemsSamplePlayerFactory* apFactory);

} // namespace Aems
} // namespace Snd9

#endif // SDKS_EATECH_SND_SNDAEMS_H
