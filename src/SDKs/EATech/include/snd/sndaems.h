#ifndef SDKS_EATECH_SND_SNDAEMS_H
#define SDKS_EATECH_SND_SNDAEMS_H

#include <cstddef> // offsetof

#include "types.hpp"

#include "SDKs/EATech/include/snd/sndo.h" // Snd9::IAemsSamplePlayerFactory
#include "SDKs/Csis/CsisClass.h"

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
// Native geometry: ARTIST 82B717C8/82B72250. Bank link+50, module handle+4,
// constructor node+C, program+28; fixed Module size3C and bank header58.
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
    ModuleBankLink* mpPrev;
};

// A registered AEMS module bank. Header fields are the ones the remove/query paths
// touch; the packed, variable-stride module-record table lives at a self-relative
// byte offset (miModuleTableOffset) and is walked as compiled AEMS data.
struct ModuleBank
{
    u8    maFileHeader[0x0A];            // +0x00, ABKC/version/native-width marker
    u16   muModuleCount;                 // +0x0A
    u8    maPad0C[0x08];                 // +0x0C
    u32   muTotalSize;                   // +0x14
    u32   muResidentSize;                // +0x18
    u32   muModuleTableOffset;           // +0x1C
    u32   muSampleOffset;
    u32   muSampleSize;                  // +0x24
    u8    maPad28[0x08];                 // +0x28
    u32   muFunctionRelocOffset;          // +0x30
    u32   muPointerRelocOffset;           // +0x34
    u32   muCsisRelocOffset;              // +0x38
    s32   miBankHandle;                   // +0x3C
    void* mpSampleBank;
    s32   miRemoved;
    void* mpAllocatedBlock;
    s32   miStreamFileOffset;
    ModuleBankLink mLink;
};

// ARTIST native fixed portion. Signed byte offsets follow at +3C for
// muSamplePlayerCount + muAlternateCount records.

struct ModuleRecord
{
    s32                   miModuleId;        // +0x00
    Csis::ClassHandle     mClassHandle;
    Csis::ClassClientNode mConstructorNode;
    s16                   muCurrentInstances;
    s16                   muMaxInstances;
    u16                   muGlobalCount;
    u16                   muFunctionCount;
    u8                    muSamplePlayerCount;
    u8                    mbHasDestructor;
    u8                    mbHasClassData;
    u8                    muAlternateCount;
    void*                 mpProgram;
    void*                 mpInstanceTemplate;
    u32                   muInstanceSize;
    s32                   miDestroyDataOffset;
    void*                 mpInstanceList;
};

CGS_STATIC_ASSERT((offsetof(ModuleBank, mLink) == 0x50), "native AEMS bank link");
CGS_STATIC_ASSERT((sizeof(ModuleBank) == 0x58), "native AEMS bank header");
CGS_STATIC_ASSERT((offsetof(ModuleRecord, mClassHandle) == 0x04), "native AEMS class handle");
CGS_STATIC_ASSERT((offsetof(ModuleRecord, mConstructorNode) == 0x0C), "native AEMS constructor node");
CGS_STATIC_ASSERT((offsetof(ModuleRecord, mpProgram) == 0x28), "native AEMS program pointer");
CGS_STATIC_ASSERT((sizeof(ModuleRecord) == 0x3C), "native AEMS module fixed record");

inline s32* ModuleRecordOffsets(ModuleRecord* apRecord)
{
    return reinterpret_cast<s32*>(reinterpret_cast<u8*>(apRecord) + sizeof(ModuleRecord));
}

inline ModuleRecord* NextModuleRecord(ModuleRecord* apRecord)
{
    return reinterpret_cast<ModuleRecord*>(
        reinterpret_cast<u8*>(apRecord) + sizeof(ModuleRecord) +
        sizeof(s32) * (apRecord->muSamplePlayerCount + apRecord->muAlternateCount));
}

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

// C-facing AEMS bank entry point used by AemsContent.  The callback may retain
// the input allocation or return a separate resident allocation.
typedef void* (*SNDAEMSModuleBankAllocator)(void* apBank, int aiResidentSize,
                                            int aiTotalSize);
extern "C" s32 SNDAEMS_addmodulebank(void* apBank, s32 aiStreamFileOffset,
                                     s32 aiFlags,
                                     SNDAEMSModuleBankAllocator apAllocator);

#endif // SDKS_EATECH_SND_SNDAEMS_H
