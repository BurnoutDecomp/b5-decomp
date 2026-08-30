// ============================================================================
// CgsSplicerPlayerVoice.cpp -- CgsSound::Playback::SplicerPlayerVoice allocation +
// destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SplicerPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&) @ 0x826AFC48
//   SplicerPlayerVoice::~SplicerPlayerVoice()  (scalar-deleting dtor)    @ 0x826E1838
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerPlayerVoice.h"

#include "rw/rwcore_structs.h"                                       // rw::ResourceDescriptor, rw::Resource, rw::IResourceAllocator
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>)

namespace CgsSound
{
namespace Playback
{

// @ 0x826AFC48. Client allocation size (asm-exact):
//   oc  = GetOutputParameterCount; pc = GetParameterCount; sendCount = GetSendCount;
//   ipc = pc - oc; sc = GetSlotCount;
//   size = 20*(sc + ipc) + 12*(sendCount + oc) + 140
// Allocated through the Factory's Environment RenderWare IResourceAllocator (same
// shape as the committed GenericRwacVoiceConfig::operator new).
void* SplicerPlayerVoice::operator new(size_t auSize, Factory& arFactory,
                                       const VoiceSpec& arVoiceSpec)
{
    const u32 lu32OutputParameterCount = arVoiceSpec.GetOutputParameterCount();
    const u32 lu32ParameterCount       = arVoiceSpec.GetParameterCount();
    const u32 lu32SendCount            = arVoiceSpec.GetSendCount();
    const u32 lu32InputParameterCount  = lu32ParameterCount - lu32OutputParameterCount;
    const u32 lu32SlotCount            = arVoiceSpec.GetSlotCount();

    // ARTIST's fixed 140-byte client and 20/12-byte tail records become their
    // native host sizes. The compiler-supplied auSize is sizeof the widened
    // SplicerPlayerVoice and the Voice constructor uses the same four sizeofs.
    const size_t luSize = auSize +
        sizeof(Slot) * lu32SlotCount +
        sizeof(InputParameter) * lu32InputParameterCount +
        sizeof(Send) * lu32SendCount +
        sizeof(OutputParameter) * lu32OutputParameterCount;

    rw::IResourceAllocator* lpAllocator = arFactory.GetEnvironment().GetAllocator();

    // X360 stack build: a FIVE-entry serialised descriptor (40B). desc[0] = { size, 4 };
    // desc[1..4] = { 0, 1 }.
    CgsResource::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    for (u32 luIndex = 1; luIndex < 5; ++luIndex)
    {
        lDescriptor.m_baseResourceDescriptors[luIndex].m_size = 0;
        lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
    }
    static_assert(sizeof(CgsResource::ResourceDescriptor) == 40,
                  "X360 serialised descriptor is 5x{size,align}=40B");

    const rw::ResourceDescriptor& lAbiDescriptor =
        reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor);
    rw::Resource lResource = lpAllocator->DoAllocate(lAbiDescriptor, "SplicerPlayerVoice");
    return lResource.m_baseResources[0];
}

// @ 0x826E1838. Null the two own members, then run the base dtor chain implicitly.
//   a1[33] = 0;  (mpInternalSubmix)   a1[34] = 0;  (mpSplice)
//   GenericRwacVoice::~GenericRwacVoice(a1+0x2C); Voice::~Voice(a1); (a2&1) delete
SplicerPlayerVoice::~SplicerPlayerVoice()
{
    mpInternalSubmix = nullptr; // stw r10,0x84(r31)
    mpSplice = nullptr;         // stw r10,0x88(r31)

    // The GenericRwacVoice base (subobject @+0x2C) and the Voice primary base (@+0)
    // destructors, plus the intervening vtable rewrites, are emitted implicitly here.
}

} // namespace Playback
} // namespace CgsSound
