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
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"

#include <cstdarg>
#include <cstdio>

namespace CgsSound
{
namespace Logic
{

DynamicMixer::~DynamicMixer()
{
    // No DynamicMixer-specific member teardown: the X360 destructor only runs the
    // Nicotine::IDynamicMixer base sub-object destructor (implicit here) and, in the
    // deleting flavour, frees storage -- both compiler/operator-delete concerns.
}

// @ 0x826A5308. Resolve the packed state/instance/effect identity carried by
// the mix-map endpoint and attach it to the matching live logic effect.
bool DynamicMixer::ConnectDMixIO(Nicotine::DMixIO* apDMixIO)
{
    CGS_ASSERT(apDMixIO != 0, "lpDMixIO");
    const int liStateId = apDMixIO->GetStateID();
    CGS_ASSERT(liStateId < KI_MAX_NUMBER_OF_STATES,
               "liStateManId < KI_MAX_NUMBER_OF_STATES");

    StateManager* lpManager = mpEnvironment->GetStateManager(liStateId);
    if (!lpManager)
        return false;

    const int liInstance = apDMixIO->GetInstanceNum();
    State* lpState = lpManager->GetHeadState();
    while (lpState && lpState->GetInstanceID() != liInstance)
        lpState = lpState->GetNextState();
    if (!lpState)
        return false;

    EffectBase* lpEffect = apDMixIO->IsSFXObj()
        ? lpState->GetHeadEffectObject() : lpState->GetHeadEffectControl();
    const int liEffectId = apDMixIO->GetSFX_ID();
    while (lpEffect && lpEffect->GetEffectID() != liEffectId)
        lpEffect = lpEffect->mpNextEffectBase;
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
    const int liCopied = std::vsnprintf(lacText, sizeof(lacText), apFormat, lArgs);
    va_end(lArgs);
    CGS_ASSERT(liCopied < static_cast<int>(sizeof(lacText)),
               "lNumBytesCopied<(int32_t)luBytes");
    CgsDev::Log::WriteToLog(lacText);
    return 0;
}

void DynamicMixer::DMixAssert(bool abCondition, const char* apFormat, ...)
{
    if (abCondition)
        return;

    char lacText[256];
    va_list lArgs;
    va_start(lArgs, apFormat);
    const int liCopied = std::vsnprintf(lacText, sizeof(lacText), apFormat, lArgs);
    va_end(lArgs);
    CGS_ASSERT(liCopied < static_cast<int>(sizeof(lacText)),
               "lNumBytesCopied<(int32_t)luBytes");
    CGS_ASSERT(false, lacText);
}

} // namespace Logic
} // namespace CgsSound
