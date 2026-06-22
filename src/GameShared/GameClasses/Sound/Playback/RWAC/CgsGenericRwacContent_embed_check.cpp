#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"

#include "rw/rwcore_structs.h"  // rw types for the VoiceConfig operator new exercise

#include <cstddef>  // offsetof

// Embed-check for the CgsSound-RWAC-content group:
//   GenericRwacReverbIRContent::`scalar deleting destructor'  @ 0x826D7EE0
//   GenericRwacWaveContent::`scalar deleting destructor'      @ 0x826C7A48
//   GenericRwacVoiceConfig::operator new                      @ 0x826AEEE8
//
// Proves the layout the two destructors depend on and forces emission of the
// scalar-deleting destructors (dtor + operator delete) plus the operator new body.

using namespace CgsSound::Playback;

// --- Layout proofs -----------------------------------------------------------
//
// NOTE: absolute X360 offsets (Content == 0x20, links at object +44/+48) are NOT
// static-asserted here -- they depend on 32-bit X360 pointers and would be wrong on
// the host's 64-bit ABI (project rule: never assert absolute offsets across pointer
// members). The X360 offsets are documented in CgsGenericRwacContent.h. Instead we
// assert ABI-stable structural invariants that still pin down the dtor's layout
// assumption: both content classes contain the Content base, and mLoader follows it
// contiguously (the embedded ResourcePtr's alias links are what the dtor unlinks).

// Both content classes derive from Content (object begins with the Content base;
// mLoader sits immediately after it -- the embedded ResourcePtr alias links the
// dtor unlinks live inside mLoader).
static_assert(static_cast<Content*>(static_cast<GenericRwacReverbIRContent*>(nullptr)) == nullptr,
              "ReverbIRContent must derive from Content");
static_assert(static_cast<Content*>(static_cast<GenericRwacWaveContent*>(nullptr)) == nullptr,
              "WaveContent must derive from Content");
static_assert(sizeof(GenericRwacReverbIRContent) > sizeof(Content),
              "ReverbIRContent must add mLoader past the Content base");
static_assert(sizeof(GenericRwacWaveContent) > sizeof(Content),
              "WaveContent must add mLoader past the Content base");

// rw::ResourceDescriptor / rw::Resource shapes the VoiceConfig new path uses (these
// are fixed 4-entry arrays of 32-bit fields -- ABI-stable on the host).
static_assert(sizeof(rw::ResourceDescriptor) == 32, "ResourceDescriptor 4x{size,align}");
static_assert(sizeof(rw::Resource) == 32, "Resource 4x base ptr");

// --- Force emission of the three bodied symbols -----------------------------

// delete-through-pointer forces the compiler to emit the scalar-deleting
// destructor (member/base dtor unlink + operator delete) for each class.
void embed_delete_reverb(GenericRwacReverbIRContent* lpContent)
{
    delete lpContent;
}

void embed_delete_wave(GenericRwacWaveContent* lpContent)
{
    delete lpContent;
}

// Exercise the placement operator new.
void* embed_voiceconfig_new(size_t auSize, Environment& arEnvironment)
{
    return GenericRwacVoiceConfig::operator new(auSize, arEnvironment);
}
