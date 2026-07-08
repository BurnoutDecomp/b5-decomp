#include "SDKs/XAudio/XAudioSourceEffect.h"

// ===========================================================================
// XAUDIO::CSourceEffect -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioSourceEffect.h for the layout and asm notes. No reference source
// and no DWARF exist for this TU; the PowerPC asm is authoritative.
// ===========================================================================

namespace XAUDIO
{

// The platform status the param dispatchers return for an unrecognised selector
// (X360 `lis r3,-0x7FF9 ; ori r3,r3,0x57` -> 0x80070057 == E_INVALIDARG).
static const s32 KI_E_INVALIDARG = static_cast<s32>(0x80070057);

// @ 0x82964418:
//   *this      = <CSourceEffect vtable>       (compiler-emitted vptr seats)
//   this[1]    = 1                            ; muReserved4 = 1
//   this[2]    = params[1]                    ; mpContext   = params->mpContext
//   CSourceStream::CSourceStream(this + 0xC)  ; secondary base ctor
CSourceEffect::CSourceEffect(const SSourceEffectCreateParams* apParams)
    : CEffect()
    , CSourceStream()
{
    // muReserved4 (CEffect +4) and mpContext (CEffect +8) are seated directly by
    // the inlined base construction; mpCompleteCallback / muParamB are left for
    // Initialize (the ctor asm does not touch +0xAC / +0xB0).
    muReserved4 = 1;
    mpContext   = apParams->mpContext;
}

// @ 0x82964728 -- the base destructors (CSourceStream then CEffect, the latter
// reseating off_82108FF4) do all the work; the derived body is empty.
CSourceEffect::~CSourceEffect()
{
}

// @ 0x829641A8:
//   this[0xAC] = params[0x48]  ; mpCompleteCallback
//   this[0xB0] = params[0x4C]  ; muParamB
//   return CSourceStream::Initialize(this + 0xC, params + 8,
//                                    params[0x44], params[0x45], aParam);
s32 CSourceEffect::Initialize(const SSourceEffectCreateParams* apParams, int aParam)
{
    mpCompleteCallback = apParams->mpCompleteCallback;
    muParamB           = apParams->muParamB;

    return CSourceStream::Initialize(
        const_cast<u8*>(apParams->mStreamConfig), // params + 8 (config sub-block)
        apParams->muFlagA,
        apParams->muFlagB,
        aParam);
}

// @ 0x829641D8 -- get dispatch. `this + 0xC` (the CSourceStream subobject) is the
// receiver of every virtual call; in C++ that is the inherited virtual dispatch.
s32 CSourceEffect::GetParam(u8 aSection, u8 aType, SAudioParamValue* apValue)
{
    if (aSection == 2 && aType == 3)
    {
        if (apValue->muSize == 56)
            return GetParamA(apValue->mpValue, nullptr);
        if (apValue->muSize == 60)
            return GetParamA(apValue->mpValue,
                             reinterpret_cast<u8*>(apValue->mpValue) + 56);
    }
    else
    {
        if (aSection == 0 && aType == 1)
            return GetParamB(apValue);
        if (aSection == 1 && aType == 1)
            return GetParamC(apValue);
        if (aSection == 3 && aType == 1)
            return GetParamD(apValue);
    }

    return KI_E_INVALIDARG;
}

// @ 0x829642D0 -- set dispatch (mirrors GetParam). The type-1 setters reinterpret
// the descriptor's first word as a 32-bit float (`lfs f1,0(r6)`); the size-60
// section-2/type-3 form dereferences the value pointer at +0x38 for the byte arg.
s32 CSourceEffect::SetParam(u8 aSection, u8 aType, SAudioParamValue* apValue)
{
    if (aSection == 2 && aType == 3)
    {
        if (apValue->muSize == 56)
            return SetParamA(apValue->mpValue, 0);
        if (apValue->muSize == 60)
            return SetParamA(
                apValue->mpValue,
                *(reinterpret_cast<u8*>(apValue->mpValue) + 56));
    }
    else
    {
        if (aSection == 0 && aType == 1)
            return SetParamB(apValue->mfValue);
        if (aSection == 1 && aType == 1)
            return SetParamC(apValue->mfValue);
        if (aSection == 3 && aType == 1)
            return SetParamD(apValue->mfValue);
    }

    return KI_E_INVALIDARG;
}

// @ 0x82964D18:
//   packet = CSourceStream::CompletePacket(aParam1, aParam2);
//   if (packet && mpCompleteCallback) {
//       SCompleteNotification n;            ; _DWORD v8[12] scratch (3 words used)
//       n.mpContext  = mpContext;           ; *(this - 4)  [CSourceStream this]
//       n.miParam    = aParam2;             ; v8[2]
//       n.muUserData = packet->muUserData;  ; *(packet + 0x24)
//       mpCompleteCallback(&n);
//   }
//   return packet;
SCompletedPacket* CSourceEffect::CompletePacket(int aParam1, int aParam2)
{
    SCompletedPacket* pPacket = CSourceStream::CompletePacket(aParam1, aParam2);
    if (pPacket)
    {
        if (mpCompleteCallback)
        {
            SCompleteNotification lNotification;
            lNotification.mpContext  = mpContext;
            lNotification.miParam    = aParam2;
            lNotification.muUserData = pPacket->muUserData;
            mpCompleteCallback(&lNotification);
        }
    }
    return pPacket;
}

} // namespace XAUDIO
