#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstddef> // size_t

namespace rw
{
    struct ResourceDescriptor;
    struct Resource;
    class  IResourceAllocator;
}

// ============================================================================
// CgsAemsPlayerVoice.h  (MINIMAL home for the AEMS player-voice allocation TUs).
//
// The three reconstructed functions:
//   AemsPlayerVoice::GetClientAllocationSize(Factory&, const VoiceSpec&) @ 0x826A2B58
//   AemsPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&)    @ 0x826C2270
//   AemsPlayerVoice::~AemsPlayerVoice()  (scalar-deleting dtor)          @ 0x826DAA50
//
// AemsPlayerVoice derives (like every playback voice) from Playback::Voice + a
// GenericRwacVoice subobject; those bases are un-homed keystones, so this MINIMAL
// home models only the allocation surface the three TUs touch. Every collaborator
// (Factory, Environment, VoiceSpec, VoiceSchema, FeatureSchema, Registry,
// AemsVoiceCsisClass) is a self-contained minimal slice carrying ONLY the methods
// these TUs call BY NAME. These TUs include ONLY this header (plus rwcore/resource
// for operator new), so there is no ODR clash with the committed CgsDataStructures /
// CgsFactory homes -- no single TU sees both. Host-width FLAG: pointer members
// widen; pinned BY NAME only.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

class Factory;
struct VoiceSpec;

// The interned name key GetClientAllocationSize hashes to look up the CSIS class.
struct Name
{
    uintptr_t muValue;
    uintptr_t GetValue() const { return muValue; }
};

// The CSIS voice class entity (looked up out of the AEMS registry by voice-schema
// name). GetClientAllocationSize reads its parameter count (lhz *(csis+0xC)).
struct AemsVoiceCsisClass
{
    u32 GetParameterCount() const { return mu32ParameterCount; }
    u32 mu32ParameterCount;
};

// Type-checked registry lookup (Registry::GetEntity<AemsVoiceCsisClass>).
struct Registry
{
    template <typename T>
    const T* GetEntity(const Name& akrName) const;
};

// One feature sub-schema; GetClientAllocationSize reads its parameter-schema count.
struct FeatureSchema
{
    u32 GetParameterSchemaCount() const { return mu32ParameterSchemaCount; }
    u32 mu32ParameterSchemaCount;
};

// The resolved voice schema. GetClientAllocationSize reads its interned Name and its
// feature sub-schemas.
struct VoiceSchema
{
    Name GetName() const { return mName; }
    const FeatureSchema& GetFeatureSchema(u32 au32Index) const;
    Name mName;
};

// The serialised voice spec. Its resolved-schema count forwarders + the send/tail
// unit count byte drive the allocation-size math.
struct VoiceSpec
{
    const VoiceSchema* GetVoiceSchema() const { return mpVoiceSchema; }
    u32 GetSlotCount() const;
    u32 GetParameterCount() const;
    u32 GetOutputParameterCount() const;
    u32 GetTailUnitCount() const { return mu8TailUnitCount; }

    const VoiceSchema* mpVoiceSchema;
    u8                 mu8TailUnitCount;
};

// The AEMS environment slice -- only the RenderWare allocator it holds is read.
struct Environment
{
    rw::IResourceAllocator* GetAllocator() const;
};

// The AEMS factory slice -- exposes its registry and owning environment BY NAME
// (X360 Factory-4/+0x60 registry recovery and +0xC environment read).
class Factory
{
public:
    const Registry* GetAemsRegistry() const;
    Environment&    GetEnvironment();
};

// ---------------------------------------------------------------------------
// CgsAemsPlayerVoice.h. The AEMS player voice (allocation surface only).
// ---------------------------------------------------------------------------
struct AemsPlayerVoice
{
    // @ 0x826A2B58 (static). Client block size = 4*(featureParamSchemaCount +
    // csisParamCount + 42).
    static size_t GetClientAllocationSize(Factory& arFactory, const VoiceSpec& arVoiceSpec);

    // @ 0x826C2270 (placement new). Allocate the client block (fixed part +
    // per-parameter/-slot tail) through the Factory's Environment RenderWare allocator.
    void* operator new(size_t auSize, Factory& arFactory, const VoiceSpec& arVoiceSpec);

    // @ 0x826DAA50. Empty out-of-line class dtor (base dtors run implicitly).
    virtual ~AemsPlayerVoice();
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H
