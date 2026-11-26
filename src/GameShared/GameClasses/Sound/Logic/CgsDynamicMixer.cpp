// ============================================================================
// CgsDynamicMixer.cpp -- CgsSound::Logic::DynamicMixer out-of-line destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DC970
//   (CgsSound::Logic::DynamicMixer::`scalar deleting destructor')
//     bl  Nicotine::IDynamicMixer::~IDynamicMixer(this);  // base sub-object dtor
//     if (a2 & 1) operator delete(this);                  // deleting tail (host delete)
//     return this;
//
// CgsSound::Logic::DynamicMixer is the game's Nicotine::IDynamicMixer subclass (the
// CgsSound seam onto the NFS mix system; the same IDynamicMixer-derived mixer the
// SoundLogic Module embeds @+0x2C30). The scalar-deleting-destructor's only source-
// level effect is running the base ~IDynamicMixer() (which frees the NFSMixMaster /
// SnapshotMixer sub-objects through the mixer allocator); the conditional operator
// delete is the MSVC deleting-destructor tail, re-synthesised from this virtual
// destructor + the class's operator delete.
//
// FLAG (confidence medium): the destructor proves DynamicMixer touches no members of
// its own beyond the IDynamicMixer base at teardown, so no additional fields are
// modelled. The base ~IDynamicMixer() runs implicitly via inheritance -- no explicit
// call is written (that would double-destroy on the host). Mirrors the committed
// sibling destructor-home pattern (CgsEffectObjectDtor.cpp).
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsDynamicMixer.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include <cstdarg>
#include <cstdio>

namespace CgsSound
{
namespace Logic
{

// @ 0x826A5308. Resolve the packed state/instance/effect identity carried by
// the mix-map endpoint and attach it to the matching live logic effect.
bool DynamicMixer::ConnectDMixIO(Nicotine::DMixIO* apDMixIO)
{
    const s32 liStateId = apDMixIO->GetStateID();
    Environment* lpEnvironment = mpEnvironment;
    CGS_ASSERT_AT(liStateId < KI_MAX_NUMBER_OF_STATES,
        "liStateManId < KI_MAX_NUMBER_OF_STATES",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEnvironment.cpp", 481);
    StateManager* const lpManager = lpEnvironment->mapStateManagers[liStateId];
    if (lpManager == 0)
        return false;

    const s32 liInstance = apDMixIO->GetInstanceNum();
    State* lpState = mpEnvironment->GetStateManager(apDMixIO->GetStateID())->GetHeadState();
    while (lpState && lpState->GetInstanceID() != liInstance)
        lpState = lpState->GetNextState();
    if (!lpState)
        return false;

    EffectBase* lpEffect;
    if (apDMixIO->IsSFXObj())
    {
        const s32 liEffectId = apDMixIO->GetSFX_ID();
        EffectObject* lpObject = static_cast<EffectObject*>(lpState->GetHeadEffectObject());
        if (!lpObject)
            return false;
        do
        {
            if (lpObject->GetEffectID() == liEffectId)
                break;
            lpObject = static_cast<EffectObject*>(lpObject->mpNextEffectBase);
        } while (lpObject);
        if (!lpObject)
            return false;
        lpEffect = lpObject;
    }
    else
    {
        const s32 liEffectId = apDMixIO->GetSFX_ID();
        EffectControl* lpControl = static_cast<EffectControl*>(lpState->GetHeadEffectControl());
        while (lpControl && lpControl->GetEffectID() != liEffectId)
            lpControl = static_cast<EffectControl*>(lpControl->mpNextEffectBase);
        lpEffect = lpControl;
    }
    if (!lpEffect)
        return false;

    lpEffect->SetDMixIOPtr(apDMixIO);
    return true;
}

int DynamicMixer::GetStateCount(int aiState)
{
    StateManager* lpManager = mpEnvironment->GetStateManager(aiState);
    return lpManager ? lpManager->GetStateObjCount() : 0;
}

int DynamicMixer::DMixPrintf(const char* apFormat, ...)
{
    char lacText[256];
    va_list lArgs;
    va_start(lArgs, apFormat);
    const int liCopied = ::vsnprintf(lacText, sizeof(lacText), apFormat, lArgs);
    va_end(lArgs);
    CGS_ASSERT_AT(liCopied < static_cast<int>(sizeof(lacText)),
        "lNumBytesCopied<(int32_t)luBytes",
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 96 /* ARTIST 0x8268D2F4 */);

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << lacText;
    return 0;
}

void DynamicMixer::DMixAssert(bool abCondition, const char* apFormat, ...)
{
    if (abCondition)
        return;

    char lacText[256];
    va_list lArgs;
    va_start(lArgs, apFormat);
    const int liCopied = ::vsnprintf(lacText, sizeof(lacText), apFormat, lArgs);
    va_end(lArgs);
    CGS_ASSERT_AT(liCopied < static_cast<int>(sizeof(lacText)),
        "lNumBytesCopied<(int32_t)luBytes",
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 96);
    CGS_FAIL_AT(lacText,
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsEnvironment.cpp", 608);
}

} // namespace Logic
} // namespace CgsSound
