#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsCsisCommandQueue.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "rw/audio/core/Voice.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace CgsSound
{
namespace Playback
{

size_t AemsPlayerVoice::GetClientAllocationSize(
    Factory& arFactory, const VoiceSpec& arVoiceSpec)
{
    Registry* lpRegistry = GetAemsFactoryRegistry(&arFactory);
    CGS_ASSERT(lpRegistry, "mpRegistry");

    const VoiceSchema& lrSchema = arVoiceSpec.GetVoiceSchema();
    Name lSchemaName = lrSchema.GetName();
    const AemsVoiceCsisClass* lpCsis =
        lpRegistry->GetEntity<AemsVoiceCsisClass>(lSchemaName);
    CGS_ASSERT(lpCsis, "lpCsis");
    const FeatureSchema& lrAemsFeature = lrSchema.GetFeatureSchema(0);

    return sizeof(AemsPlayerVoice) + sizeof(s32) *
        (lpCsis->GetUserParameterStart() +
         lrAemsFeature.GetParameterSchemaCount());
}

AemsPlayerVoice::AemsPlayerVoice(AemsFactory& arFactory,
                                 const VoiceSpec& arVoiceSpec,
                                 u32 au32Ident)
    : PlayerVoice(GetClientAllocationSize(arFactory, arVoiceSpec),
                  arFactory, arVoiceSpec, au32Ident),
      GenericRwacVoice(),
      mfSamplePlayerCpuTicks(0.0f),
      mpFirstPlayer(0),
      mpInternalSubmix(0),
      mhClass(),
      mpRequest(0),
      mu32UserParameterStart(0),
      mu32AemsInputParameterCount(0),
      mbRemoving(false),
      mbCreated(false),
      mpAemsParameters(reinterpret_cast<s32*>(
          reinterpret_cast<u8*>(this) + sizeof(AemsPlayerVoice)))
{
    const VoiceSchema& lrSourceSchema = arVoiceSpec.GetVoiceSchema();
    CGS_ASSERT(lrSourceSchema.GetFeatureSchemaCount() != 0,
               "AEMS voice has an AEMS feature");

    Name lSchemaName = lrSourceSchema.GetName();
    const AemsVoiceCsisClass* lpCsis = arFactory.GetRegistry()->
        GetEntity<AemsVoiceCsisClass>(lSchemaName);
    CGS_ASSERT(lpCsis, "lpCsis");
    const FeatureSchema& lrAemsFeature = lrSourceSchema.GetFeatureSchema(0);
    CGS_ASSERT(lrAemsFeature.GetOutputParamCount() == 0,
               "AEMS feature has no output parameters");

    mu32UserParameterStart = lpCsis->GetUserParameterStart();
    mu32AemsInputParameterCount = lrAemsFeature.GetParameterSchemaCount();
    const u32 luParameterWords =
        mu32UserParameterStart + mu32AemsInputParameterCount;
    std::memset(mpAemsParameters, 0, sizeof(s32) * luParameterWords);
    if (luParameterWords)
        mpAemsParameters[0] = static_cast<s32>(au32Ident);

    // Feature zero is the AEMS/CSIS control feature. The RWAC graph receives a
    // temporary schema containing only the ordinary audio features, exactly as
    // ARTIST's constructor does before posting GenericRwacVoice creation.
    const u32 luRwacFeatureCount =
        lrSourceSchema.GetFeatureSchemaCount() - 1u;
    const size_t luSchemaBytes = sizeof(VoiceSchema) +
        (luRwacFeatureCount > 0 ?
            sizeof(FeatureSchema*) * (luRwacFeatureCount - 1u) : 0u);
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
    // ARTIST @ 0x826DA3FC-0x826DA404 writes 1 to VoiceSpec +0x0F: the
    // voice-type byte, not processing stage (+0x0D).  The RWAC half is an
    // internal submix voice, matching the SplicerPlayerVoice construction.
    lpRwacSpec->mu8VoiceType = E_SUBMIX_VOICE;

    mbCreated = CreateVoiceInstance(*lpRwacSpec, *this,
                                    arFactory.GetRwacFactory(),
                                    &mpInternalSubmix);

    CsisSetClassHandleCommand lSetClass(
        reinterpret_cast<uintptr_t>(&mhClass), lpCsis->GetClassName(),
        lpCsis->GetSystemCrc(), lpCsis->GetClassCrc());
    arFactory.GetCommandQueue() << lSetClass;

    if (GetSlotCount() != 0)
    {
        void* lpSlotMemory = arFactory.GetEnvironment().Allocate(
            static_cast<u32>(sizeof(AemsContentSlot)),
            static_cast<u32>(alignof(AemsContentSlot)), "AemsContentSlot");
        CGS_ASSERT(lpSlotMemory, "lpSlotMemory");
        if (lpSlotMemory)
            GetSlot(0).SetImplementation(::new (lpSlotMemory) AemsContentSlot());
    }
    AcknowledgePlaybackStateChange();
}

f32 AemsPlayerVoice::GetCpuTicks()
{
    mfSamplePlayerCpuTicks = 0.0f;
    for (AemsRWSamplePlayer* lpPlayer = mpFirstPlayer; lpPlayer;
         lpPlayer = lpPlayer->mpNext)
    {
        if (lpPlayer->mpVoice)
            mfSamplePlayerCpuTicks +=
                static_cast<f32>(lpPlayer->mpVoice->miLastFrameCpuTicks);
        for (u32 luPanner = 0; luPanner < lpPlayer->mNumPannerVoices; ++luPanner)
        {
            if (lpPlayer->mpPannerVoice[luPanner])
                mfSamplePlayerCpuTicks += static_cast<f32>(
                    lpPlayer->mpPannerVoice[luPanner]->miLastFrameCpuTicks);
        }
    }
    return mfSamplePlayerCpuTicks + GenericRwacVoice::GetCpuTicks();
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
    return mpFirstPlayer == 0;
}

bool AemsPlayerVoice::Play(u32 /*au32Param*/)
{
    if (mpRequest != 0)
        return false;

    for (u32 luIndex = 0; luIndex < mu32AemsInputParameterCount; ++luIndex)
    {
        const InputParameter& lrInput = GetInputParameter(luIndex);
        const f32 lfClamped = std::min(
            std::max(lrInput.GetValueRaw(), lrInput.GetMin()), lrInput.GetMax());
        mpAemsParameters[mu32UserParameterStart + luIndex] =
            static_cast<s32>(lfClamped);
    }

    AemsFactory& lrFactory =
        const_cast<AemsFactory&>(static_cast<const AemsFactory&>(GetFactory()));
    CsisCreateCommand lCommand(
        reinterpret_cast<uintptr_t>(&mhClass),
        reinterpret_cast<uintptr_t>(mpAemsParameters),
        reinterpret_cast<uintptr_t>(&mpRequest));
    lrFactory.GetCommandQueue() << lCommand;
    return true;
}

bool AemsPlayerVoice::Update(f32 /*af32Dt*/)
{
    bool lbChanged = false;
    for (u32 luIndex = 0; luIndex < mu32AemsInputParameterCount; ++luIndex)
    {
        const InputParameter& lrInput = GetInputParameter(luIndex);
        if (!lrInput.HasChanged())
            continue;
        lbChanged = true;
        const f32 lfClamped = std::min(
            std::max(lrInput.GetValueRaw(), lrInput.GetMin()), lrInput.GetMax());
        mpAemsParameters[mu32UserParameterStart + luIndex] =
            static_cast<s32>(lfClamped);
    }

    if (mpRequest)
    {
        s32 liRefCount = 0;
        const int liResult = mpRequest->GetRefCount(&liRefCount);
        CGS_ASSERT(liResult == 0, "Failed to GetRefCount on Csis Pointer");
        if (liRefCount <= 2)
            return false;
        if (lbChanged)
        {
            AemsFactory& lrFactory = const_cast<AemsFactory&>(
                static_cast<const AemsFactory&>(GetFactory()));
            CsisUpdateCommand lCommand(
                reinterpret_cast<uintptr_t>(mpRequest),
                reinterpret_cast<uintptr_t>(mpAemsParameters));
            lrFactory.GetCommandQueue() << lCommand;
        }
    }
    return true;
}

bool AemsPlayerVoice::Stop()
{
    if (!mpRequest)
        return false;
    AemsFactory& lrFactory =
        const_cast<AemsFactory&>(static_cast<const AemsFactory&>(GetFactory()));
    CsisReleaseCommand lCommand(reinterpret_cast<uintptr_t>(mpRequest));
    lrFactory.GetCommandQueue() << lCommand;
    mpRequest = 0;
    return true;
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

} // namespace Playback
} // namespace CgsSound
