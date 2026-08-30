#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

#include "rw/rwcore_structs.h"                                       // rw::ResourceDescriptor, rw::Resource, rw::IResourceAllocator
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>)

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826C2270
//   CgsSound::Playback::AemsPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&)
//   (DWARF CgsAemsPlayerVoice.h:261). called by AemsFactory::DoCreateVoice.
//
// Placement new for the AEMS player voice: size the client block (fixed part from
// GetClientAllocationSize + a per-parameter/-slot tail), then allocate it through the
// Factory's Environment RenderWare allocator.
//
// X360 behaviour (asm):
//   op   = VoiceSpec::GetOutputParameterCount(spec)
//   pc   = VoiceSpec::GetParameterCount(spec)
//   v7   = lbz *(spec+0xC)                 == GetTailUnitCount()
//   in   = pc - op                          (input parameters)
//   sc   = VoiceSpec::GetSlotCount(spec)
//   tail = 20*(sc + in) + 12*(v7 + op)
//   cas  = GetClientAllocationSize(Factory, spec)
//   total = cas + tail
//   descriptor[0] = { total, 4 }; descriptor[1..4] = { 0, 1 }   (5-entry serialised)
//   return allocator->DoAllocate(descriptor, "AemsPlayerVoice").m_baseResources[0]

namespace CgsSound
{
namespace Playback
{
    void* AemsPlayerVoice::operator new(size_t /*auSize*/, Factory& arFactory, const VoiceSpec& arVoiceSpec)
    {
        const u32 lu32OutputParameterCount = arVoiceSpec.GetOutputParameterCount();
        const u32 lu32ParameterCount       = arVoiceSpec.GetParameterCount();
        const u32 lu32InputParameterCount  = lu32ParameterCount - lu32OutputParameterCount;
        const u32 lu32SlotCount            = arVoiceSpec.GetSlotCount();
        const u32 lu32SendCount            = arVoiceSpec.GetSendCount();

        // Native counterpart of ARTIST's 20*(slots+inputs)+12*(sends+outputs).
        // Slot contains three pointers on x64, so the console stride must not be
        // carried into the host allocation while Voice::Voice uses sizeof(Slot).
        const size_t luTailSize =
            sizeof(Slot) * lu32SlotCount +
            sizeof(InputParameter) * lu32InputParameterCount +
            sizeof(Send) * lu32SendCount +
            sizeof(OutputParameter) * lu32OutputParameterCount;

        Environment& lrEnvironment          = arFactory.GetEnvironment();
        const u32 lu32ClientSize            = static_cast<u32>(GetClientAllocationSize(arFactory, arVoiceSpec));
        rw::IResourceAllocator* lpAllocator = lrEnvironment.GetAllocator();
        const size_t luTotalSize             = lu32ClientSize + luTailSize;

        // X360 stack build: a FIVE-entry serialised descriptor (40B). Reuse the committed
        // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>) BY NAME.
        // desc[0] = { total, 4 }; desc[1..4] = { 0, 1 }.
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luTotalSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }
        static_assert(sizeof(CgsResource::ResourceDescriptor) == 40,
                      "X360 serialised descriptor is 5x{size,align}=40B");

        // The PC rwcore DoAllocate ABI takes the narrower rw::ResourceDescriptor (<4>);
        // the inert 5th {0,1} entry does not affect the leading-4 allocation view.
        const rw::ResourceDescriptor& lAbiDescriptor =
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor);
        rw::Resource lResource = lpAllocator->DoAllocate(lAbiDescriptor, "AemsPlayerVoice");
        return lResource.m_baseResources[0];
    }
}
}
