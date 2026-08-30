// ============================================================================
// CgsSplicerPlayerVoice.cpp -- CgsSound::Playback::SplicerPlayerVoice allocation +
// destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SplicerPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&) @ 0x826AFC48
//   SplicerPlayerVoice::~SplicerPlayerVoice()  (scalar-deleting dtor)    @ 0x826E1838
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"

#include "rw/rwcore_structs.h"                                       // rw::ResourceDescriptor, rw::Resource, rw::IResourceAllocator
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>)

#include <cstring>
#include <new>
#include <vector>

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

// @ 0x826E10D0. Feature zero is the Splicer control feature. Voice owns the
// authored tables, while the GenericRwacVoice half receives a temporary spec
// containing only features 1..N and voice type E_SUBMIX_VOICE.
SplicerPlayerVoice::SplicerPlayerVoice(Factory& arFactory,
                                       const VoiceSpec& arVoiceSpec,
                                       u32 au32Ident)
    : PlayerVoice(sizeof(SplicerPlayerVoice), arFactory, arVoiceSpec, au32Ident)
    , GenericRwacVoice()
    , mpInternalSubmix(0)
    , mpSplice(0)
{
    const VoiceSchema& lrSourceSchema = arVoiceSpec.GetVoiceSchema();
    CGS_ASSERT(lrSourceSchema.GetFeatureSchema(0).GetOutputParamCount() == 0,
               "0 == lVoiceSpec.GetVoiceSchema().GetFeatureSchema(0).GetOutputParamCount()");

    const u32 luRwacFeatureCount = lrSourceSchema.GetFeatureSchemaCount() - 1u;
    const size_t luSchemaBytes = sizeof(VoiceSchema) +
        (luRwacFeatureCount > 0
            ? sizeof(FeatureSchema*) * (luRwacFeatureCount - 1u) : 0u);
    std::vector<u8> laSchemaStorage(luSchemaBytes);
    VoiceSchema* lpRwacSchema = ::new (laSchemaStorage.data())
        VoiceSchema(luRwacFeatureCount);
    lpRwacSchema->mName = lrSourceSchema.mName;
    lpRwacSchema->mTypeName = lrSourceSchema.mTypeName;
    for (u32 luFeature = 0; luFeature < luRwacFeatureCount; ++luFeature)
        lpRwacSchema->SetFeatureSchema(
            luFeature, lrSourceSchema.GetFeatureSchema(luFeature + 1u));

    const u32 luSendCount = arVoiceSpec.GetSendCount();
    const size_t luSpecBytes = sizeof(VoiceSpec) +
        (luSendCount > 0 ? sizeof(Name) * (luSendCount - 1u) : 0u);
    std::vector<u8> laSpecStorage(luSpecBytes);
    VoiceSpec* lpRwacSpec = reinterpret_cast<VoiceSpec*>(laSpecStorage.data());
    std::memcpy(lpRwacSpec, &arVoiceSpec, luSpecBytes);
    lpRwacSpec->mpVoiceSchema = lpRwacSchema;
    // ARTIST @ 0x826E1294-0x826E12A4 stores byte 1 at VoiceSpec +0x0F.
    // +0x0F is mu8VoiceType (the preceding +0x0D byte is processing stage), so
    // the featureless RWAC half is an internal submix voice.  This creates its
    // SubMix plug-in at stage zero and makes the first send stage index one.
    lpRwacSpec->mu8VoiceType = E_SUBMIX_VOICE;

    SplicerFactory& lrFactory = static_cast<SplicerFactory&>(arFactory);
    const bool lbCreated = CreateVoiceInstance(*lpRwacSpec, *this,
        lrFactory.GetRwacFactory(), &mpInternalSubmix);
    CGS_ASSERT(lbCreated, "NOT CREATE VOICE");

    if (GetSlotCount() != 0)
    {
        void* lpSlotMemory = arFactory.GetEnvironment().Allocate(
            static_cast<u32>(sizeof(SplicerContentSlot)),
            static_cast<u32>(alignof(SplicerContentSlot)), "SplicerContentSlot");
        CGS_ASSERT(lpSlotMemory != 0, "lpSf");
        if (lpSlotMemory)
            GetSlot(0).SetImplementation(
                ::new (lpSlotMemory) SplicerContentSlot());
    }
    AcknowledgePlaybackStateChange();
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

f32 SplicerPlayerVoice::GetCpuTicks()
{
    const f32 lfSpliceTicks = mpSplice
        ? static_cast<f32>(mpSplice->GetCpuTicks()) : 0.0f;
    return lfSpliceTicks + GenericRwacVoice::GetCpuTicks();
}

Voice::EProfileVoiceType SplicerPlayerVoice::GetProfileVoiceType()
{
    return E_VOICETYPE_SPLICER;
}

void SplicerPlayerVoice::DoUpdate(System* apSystem, f32 /*af32DeltaTime*/)
{
    GenericRwacVoice::Update(apSystem, *this);
}

bool SplicerPlayerVoice::DoConnectSend(u32 au32Index, SubmixVoice* apSubmix)
{
    return GenericRwacVoice::ConnectSend(au32Index, apSubmix);
}

} // namespace Playback
} // namespace CgsSound
