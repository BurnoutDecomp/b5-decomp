#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsCsisCommandQueue.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "rw/audio/core/Voice.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // the constructor's local allocator (DWARF h:301)

#include "rw/math/fpu/scalar_operation.h"  // rw::math::fpu::Min / Max / Clamp
#include <cstring>
#include <new>

namespace CgsSound
{
namespace Playback
{

// This header's / file's paths as ARTIST's asserts spell them (strings 0x820B0338 / 0x820AD8A8).
#define KAC_AEMS_PLAYER_VOICE_H_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/AEMS/CgsAemsPlayerVoice.h"
#define KAC_AEMS_PLAYER_VOICE_CPP_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/AEMS/CgsAemsPlayerVoice.cpp"

// ARTIST 0x826A2B58 (DWARF CgsAemsPlayerVoice.h:242, locals lpCsis :244, lu32ParamCount :248): the
// voice's CSIS class from the factory registry (the voice schema's name), then the object plus one
// word per AEMS parameter (the class's user parameters and the AEMS feature's parameters).
size_t AemsPlayerVoice::GetClientAllocationSize(
    Factory& lFactory, const VoiceSpec& lVoiceSpec)
{
    const AemsVoiceCsisClass* lpCsis = static_cast<AemsFactory*>(&lFactory)->GetRegistry().
        GetEntity<AemsVoiceCsisClass>(lVoiceSpec.GetVoiceSchema().GetName());
    CGS_ASSERT_AT(lpCsis, "lpCsis", KAC_AEMS_PLAYER_VOICE_H_FILE, 246);
    uint32_t lu32ParamCount = lpCsis->GetUserParameterStart() +
        lVoiceSpec.GetVoiceSchema().GetFeatureSchema(0).GetParameterSchemaCount();
    return sizeof(AemsPlayerVoice) + sizeof(uint32_t) * lu32ParamCount;
}

// ARTIST 0x826DA1E8 (DWARF CgsAemsPlayerVoice.h:283 with the base Factory&; locals lAemsFactory :295,
// lLocalBuffer / lAllocator :300/:301, lpvMem :308, lpVoiceSpec :310, lu32FeatureCount :315,
// lpVoiceSchema :316, i :318, lpCsis :331, lCommand :348, lSlot :354, lpSf :358, lu32I :367). The RWAC
// half is created from a stack copy of the spec whose schema drops the AEMS feature (feature 0) and
// whose voice type is E_SUBMIX_VOICE; the CSIS class handle is requested; the AEMS content slot is
// installed; the parameter block starts with the voice's ident, the rest cleared.
AemsPlayerVoice::AemsPlayerVoice(Factory& lFactory,
                                 const VoiceSpec& lVoiceSpec,
                                 u32 luiIdent)
    : PlayerVoice(GetClientAllocationSize(lFactory, lVoiceSpec),
                  lFactory, lVoiceSpec, luiIdent),
      GenericRwacVoice(),
      mpFirstPlayer(0),
      mpInternalSubmix(0),
      mCsisHandle(),
      mpCsisClass(0),
      mbRemoving(false)
{
    mu32AemsParamCount = lVoiceSpec.GetVoiceSchema().GetFeatureSchema(0).GetParameterSchemaCount();
    const AemsFactory& lAemsFactory = *static_cast<const AemsFactory*>(&lFactory);
    CGS_ASSERT_AT(0 == lVoiceSpec.GetVoiceSchema().GetFeatureSchema(0).GetOutputParamCount(),
                  "0 == lVoiceSpec.GetVoiceSchema().GetFeatureSchema(0).GetOutputParamCount()",
                  KAC_AEMS_PLAYER_VOICE_H_FILE, 296);

    const int32_t KI_LOCAL_ALLOCATOR = 1024;
    char lLocalBuffer[KI_LOCAL_ALLOCATOR];
    CgsMemory::LinearMalloc lAllocator;
    lAllocator.Construct();
    lAllocator.Create(lLocalBuffer, KI_LOCAL_ALLOCATOR);
    lAllocator.SetAlignment(4);

    void* lpvMem = lAllocator.Malloc(lVoiceSpec.GetAllocatedSize());
    CGS_ASSERT_AT(lpvMem, "lpvMem", KAC_AEMS_PLAYER_VOICE_H_FILE, 309);
    VoiceSpec* lpVoiceSpec = static_cast<VoiceSpec*>(memcpy(lpvMem, &lVoiceSpec, lVoiceSpec.GetAllocatedSize()));

    lpvMem = lAllocator.Malloc(lVoiceSpec.GetVoiceSchema().GetAllocatedSize());
    CGS_ASSERT_AT(lpvMem, "lpvMem", KAC_AEMS_PLAYER_VOICE_H_FILE, 314);
    uint32_t lu32FeatureCount = lVoiceSpec.GetVoiceSchema().GetFeatureSchemaCount();
    VoiceSchema* lpVoiceSchema = new (lpvMem) VoiceSchema(lu32FeatureCount - 1);
    for (uint32_t i = 1; i < lu32FeatureCount; ++i)
    {
        lpVoiceSchema->SetFeatureSchema(i - 1, lVoiceSpec.GetVoiceSchema().GetFeatureSchema(i));
    }
    lpVoiceSpec->SetVoiceSchema(*lpVoiceSchema);
    lpVoiceSpec->SetVoiceType(E_SUBMIX_VOICE);

    CGS_ASSERT_AT(CreateVoiceInstance(*lpVoiceSpec, *this,
                                      const_cast<AemsFactory&>(lAemsFactory).GetRwacFactory(),
                                      &mpInternalSubmix),
                  "NOT CREATE VOICE", KAC_AEMS_PLAYER_VOICE_H_FILE, 328);

    const AemsVoiceCsisClass* lpCsis = const_cast<AemsFactory&>(lAemsFactory).GetRegistry().
        GetEntity<AemsVoiceCsisClass>(lVoiceSpec.GetVoiceSchema().GetName());
    if (lpCsis)
    {
        mu32AemsFirstUserParam = lpCsis->GetUserParameterStart();
        CsisSetClassHandleCommand lCommand(
            reinterpret_cast<uintptr_t>(&mCsisHandle), lpCsis->GetClassName(),
            lpCsis->GetSystemCrc(), lpCsis->GetClassCrc());
        const_cast<AemsFactory&>(lAemsFactory).GetCommandQueue() << lCommand;
    }

    Slot& lSlot = GetSlot(0);
    CGS_ASSERT_AT(0 == lSlot.GetImplementation(), "0 == lSlot.GetImplementation()",
                  KAC_AEMS_PLAYER_VOICE_H_FILE, 356);
    const ISlotFactory* lpSf = ISlotFactory::GetFactory(Name("~AemsContentSlot~"));
    CGS_ASSERT_AT(lpSf, "lpSf", KAC_AEMS_PLAYER_VOICE_H_FILE, 359);
    lSlot.SetImplementation(lpSf->DoCreateSlot(*this));

    GetAemsParameter(0) = GetIdent();
    for (uint32_t lu32I = 1; lu32I < mu32AemsParamCount; ++lu32I)
    {
        GetAemsParameter(lu32I) = 0;
    }

    lAllocator.Destruct();
}

// ARTIST 0x826DA668: the sample players' rw voice ticks accumulated in muAemsTicks, plus the RWAC
// half's own.
f32 AemsPlayerVoice::GetCpuTicks()
{
    muAemsTicks = 0;
    for (AemsRWSamplePlayer* lpPlayer = mpFirstPlayer; lpPlayer; lpPlayer = lpPlayer->mpNext)
    {
        muAemsTicks += lpPlayer->mpVoice->miLastFrameCpuTicks;
    }
    return static_cast<f32>(muAemsTicks) + GenericRwacVoice::GetCpuTicks();
}

Voice::EProfileVoiceType AemsPlayerVoice::GetProfileVoiceType()
{
    return E_VOICETYPE_AEMS;
}

void AemsPlayerVoice::DoUpdate(System* apSystem, f32 /*af32DeltaTime*/)
{
    GenericRwacVoice::Update(apSystem, *this);
}

bool AemsPlayerVoice::DoConnectSend(u32 au32Index, SubmixVoice* apSubmix)
{
    return GenericRwacVoice::ConnectSend(au32Index, apSubmix);
}

bool AemsPlayerVoice::DoRemove()
{
    if (!mbRemoving)
    {
        Stop();
        mbRemoving = true;
    }
    return 0 == mpFirstPlayer;   // constant first: ARTIST's addi rX,rX,0 before cntlzw (0x826DAFC8)
}

// ARTIST 0x826DAE38 (DWARF CgsAemsPlayerVoice.cpp:141): no class yet, so copy the clamped AEMS
// inputs into the parameter block after the class's user parameters (as unsigned words: fctidz +
// stfiwx) and post the CSIS create.
bool AemsPlayerVoice::Play(u32 /*au32Param*/)
{
    if (!mpCsisClass)
    {
        for (uint32_t lu32I = 0; lu32I < mu32AemsParamCount; ++lu32I)
        {
            const InputParameter& lParameter = GetInputParameter(lu32I);
            // rw::math::fpu::Clamp: ARTIST fsel(min - value) then fsel(max - v') (0x826DAE9C / 0x826DAEA4).
            GetAemsParameter(mu32AemsFirstUserParam + lu32I) = static_cast<uint32_t>(
                rw::math::fpu::Clamp(lParameter.GetValueRaw(), lParameter.GetMin(), lParameter.GetMax()));
        }

        CsisCreateCommand lCommand(
            reinterpret_cast<uintptr_t>(&mCsisHandle),
            reinterpret_cast<uintptr_t>(&GetAemsParameter(0)),
            reinterpret_cast<uintptr_t>(&mpCsisClass));
        GetAemsFactory().GetCommandQueue() << lCommand;
        return true;
    }
    return false;
}

// ARTIST 0x826DAD00 (DWARF CgsAemsPlayerVoice.cpp:97): refresh the changed inputs; while the CSIS
// class lives, report it finished once only the voice holds it, else post the new parameters.
bool AemsPlayerVoice::Update(f32 /*af32Dt*/)
{
    bool lbChanged = false;
    for (uint32_t lu32I = 0; lu32I < mu32AemsParamCount; ++lu32I)
    {
        const InputParameter& lParameter = GetInputParameter(lu32I);
        if (lParameter.HasChanged())
        {
            lbChanged = true;
            // rw::math::fpu::Clamp: ARTIST fsel(min - value) then fsel(max - v') (0x826DAD64 / 0x826DAD6C).
            GetAemsParameter(mu32AemsFirstUserParam + lu32I) = static_cast<uint32_t>(
                rw::math::fpu::Clamp(lParameter.GetValueRaw(), lParameter.GetMin(), lParameter.GetMax()));
        }
    }

    if (mpCsisClass)
    {
        s32 liRefCount = 0;
        CGS_ASSERT_AT(0 == mpCsisClass->GetRefCount(&liRefCount),
                      "Failed to GetRefCount on Csis Pointer", KAC_AEMS_PLAYER_VOICE_CPP_FILE, 118);
        if (liRefCount > 2)
        {
            if (lbChanged)
            {
                CsisUpdateCommand lCommand(
                    reinterpret_cast<uintptr_t>(mpCsisClass),
                    reinterpret_cast<uintptr_t>(&GetAemsParameter(0)));
                GetAemsFactory().GetCommandQueue() << lCommand;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

// ARTIST 0x826DAF10 (DWARF CgsAemsPlayerVoice.cpp:163): release the CSIS class, if any.
bool AemsPlayerVoice::Stop()
{
    if (mpCsisClass)
    {
        CsisReleaseCommand lCommand(reinterpret_cast<uintptr_t>(mpCsisClass));
        GetAemsFactory().GetCommandQueue() << lCommand;
        mpCsisClass = 0;
        return true;
    }
    return false;
}

void AemsPlayerVoice::AddSamplePlayer(AemsRWSamplePlayer* apPlayer)
{
    apPlayer->mpNext = mpFirstPlayer;
    mpFirstPlayer = apPlayer;
}

void AemsPlayerVoice::RemoveSamplePlayer(AemsRWSamplePlayer* apPlayer)
{
    AemsRWSamplePlayer** lppLink = &mpFirstPlayer;
    while (*lppLink && *lppLink != apPlayer)
        lppLink = &(*lppLink)->mpNext;
    if (*lppLink == apPlayer)
        *lppLink = apPlayer->mpNext;
}

#undef KAC_AEMS_PLAYER_VOICE_H_FILE
#undef KAC_AEMS_PLAYER_VOICE_CPP_FILE

} // namespace Playback
} // namespace CgsSound
