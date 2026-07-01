#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H

#include "types.hpp"

#include <cstddef> // size_t

namespace rw
{
    struct ResourceDescriptor;
    struct Resource;
    class  IResourceAllocator;
}

// ============================================================================
// CgsSplicerPlayerVoice.h  (MINIMAL home for the splicer player-voice TUs).
//
// The two reconstructed functions:
//   SplicerPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&) @ 0x826AFC48
//   SplicerPlayerVoice::~SplicerPlayerVoice()  (scalar-deleting dtor)    @ 0x826E1838
//
// SplicerPlayerVoice derives (like every playback voice) from Playback::Voice + a
// GenericRwacVoice subobject; those bases are un-homed keystones. This MINIMAL home
// models only the allocation surface and the two own-member nulls the dtor writes.
// Factory / Environment / VoiceSpec are self-contained minimal slices carrying ONLY
// the methods these TUs call BY NAME (this header is the only game header these TUs
// include besides rwcore/resource -- no ODR clash with the committed homes). Host-
// width FLAG: pointer members widen; pinned BY NAME only.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// The serialised voice spec slice -- the count forwarders drive the allocation math.
struct VoiceSpec
{
    u32 GetSlotCount() const;
    u32 GetParameterCount() const;
    u32 GetOutputParameterCount() const;
    u32 GetSendCount() const { return mu8SendCount; }

    u8 mu8SendCount;   // (+0xC) send-unit count
};

// The environment slice -- only the RenderWare allocator it holds is read.
struct Environment
{
    rw::IResourceAllocator* GetAllocator() const;
};

// The splicer factory slice -- exposes its owning environment BY NAME.
class Factory
{
public:
    Environment& GetEnvironment();
};

// ---------------------------------------------------------------------------
// CgsSplicerPlayerVoice.h. The splicer player voice (allocation + dtor surface).
// ---------------------------------------------------------------------------
struct SplicerPlayerVoice
{
    // @ 0x826AFC48 (placement new). size = 20*(slots + inputParams) +
    // 12*(sends + outputParams) + 140; allocated through the Factory's Environment.
    void* operator new(size_t auSize, Factory& arFactory, const VoiceSpec& arVoiceSpec);

    // @ 0x826E1838. Null the two own members, then run the base dtor chain (implicit).
    virtual ~SplicerPlayerVoice();

    void* mpInternalSubmix;   // X360 +0x84 (a1[33]) -- nulled by the dtor
    void* mpSplice;           // X360 +0x88 (a1[34]) -- nulled by the dtor
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H
