#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, u32 luColour, bool lbCentred);

namespace BrnGameState
{
    // ARTIST off_82CDB93C. The table is indexed by StuntElementType.
    const char* KAPC_STUNT_NAMES[E_STUNT_ELEMENT_TYPE_COUNT] =
    {
        "Jump",
        "Smash",
        "Billboard",
    };

    // X360 0x82359000.
    const char* StuntManagerDebugComponent::GetName() const
    {
        return "Stunt Manager";
    }

    // The StuntManager vtable's GetPath slot points at the ICF-folded body
    // BrnWorld::CrashPlayDebugComponent::GetPath @ 0x82312480.
    const char* StuntManagerDebugComponent::GetPath() const
    {
        return "Gameplay";
    }

    // X360 0x82388820.
    void StuntManagerDebugComponent::OnActivate()
    {
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllJumpsButOneCallback,
                         this, "Complete All Jumps but one");
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllSmashesButOneCallback,
                         this, "Complete All Smashes but one");
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllBillboardsButOneCallback,
                         this, "Complete All Billboards but one");
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllJumps,
                         this, "Complete All Jumps");
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllSmashes,
                         this, "Complete All Smashes");
        RegisterFunction(&StuntManagerDebugComponent::CompleteAllStunt,
                         this, "Complete All Stunts");
    }

    // X360 0x82358D00: three stream buffers, 20-pixel row spacing.
    void StuntManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        for (s32 liIndex = 0; liIndex < E_STUNT_ELEMENT_TYPE_COUNT; ++liIndex)
        {
            MaybeDrawText(lpRender, maStrStreams[liIndex].GetBuffer(),
                          100.0f, 100.0f + static_cast<f32>(liIndex) * 20.0f,
                          16.0f, 0xFFFFFFFFu, false);
        }
    }

    // X360 0x82358D98: the last uncompleted trigger for each stunt type.
    void StuntManagerDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        const rw::RGBA lColour(255, 255, 255, 255);
        for (s32 liIndex = 0; liIndex < E_STUNT_ELEMENT_TYPE_COUNT; ++liIndex)
        {
            lpRender->DrawSphere(maLastPositions[liIndex], 10.0f, lColour);
        }
    }

    // X360 0x82358DF0 / 0x82358EA0 / 0x82358F50. The menu action does not
    // mutate the profile immediately; StuntManager::Update consumes this pending type.
    void StuntManagerDebugComponent::CompleteAllJumps(void* lpData)
    {
        CGS_ASSERT(lpData != 0, "lpData");
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        CGS_ASSERT(lpComponent != 0, "lpStuntManagerDebugComponent");
        CGS_ASSERT(lpComponent->mpStuntManager != 0,
                   "lpStuntManagerDebugComponent->mpStuntManager");
        lpComponent->mpStuntManager->SetDebugCompletedUnlockType(E_STUNT_ELEMENT_TYPE_JUMP);
    }

    void StuntManagerDebugComponent::CompleteAllSmashes(void* lpData)
    {
        CGS_ASSERT(lpData != 0, "lpData");
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        CGS_ASSERT(lpComponent != 0, "lpStuntManagerDebugComponent");
        CGS_ASSERT(lpComponent->mpStuntManager != 0,
                   "lpStuntManagerDebugComponent->mpStuntManager");
        lpComponent->mpStuntManager->SetDebugCompletedUnlockType(E_STUNT_ELEMENT_TYPE_SMASH);
    }

    void StuntManagerDebugComponent::CompleteAllStunt(void* lpData)
    {
        CGS_ASSERT(lpData != 0, "lpData");
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        CGS_ASSERT(lpComponent != 0, "lpStuntManagerDebugComponent");
        CGS_ASSERT(lpComponent->mpStuntManager != 0,
                   "lpStuntManagerDebugComponent->mpStuntManager");
        lpComponent->mpStuntManager->SetDebugCompletedUnlockType(E_STUNT_ELEMENT_TYPE_BILLBOARD);
    }

    // X360 0x8237A0E8. Complete every authored trigger of the selected type except
    // the first one, which is retained as the HUD/world marker.
    void StuntManagerDebugComponent::CompleteAllStuntTypeButOne(StuntElementType leStuntElementType)
    {
        BrnTrigger::GenericRegion::Type leRegionType;
        switch (leStuntElementType)
        {
        case E_STUNT_ELEMENT_TYPE_JUMP:
            leRegionType = BrnTrigger::GenericRegion::E_TYPE_JUMP;
            break;
        case E_STUNT_ELEMENT_TYPE_SMASH:
            leRegionType = BrnTrigger::GenericRegion::E_TYPE_SMASH;
            break;
        case E_STUNT_ELEMENT_TYPE_BILLBOARD:
            // ARTIST maps billboard stunt elements to generic-region slot 12. The
            // near-ancestor DWARF retains that slot's older OVERDRIVE_BOOST name.
            leRegionType = BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST;
            break;
        default:
            CGS_ASSERT(false, "Unknown stunt type.");
            return;
        }

        bool lbFirstTrigger = true;
        const BrnTrigger::TriggerData* lpTriggerData =
            mpStuntManager->GetTriggerQueryManager()->GetTriggerData();

        for (s32 liGenericRegionIndex = 0;
             liGenericRegionIndex < lpTriggerData->GetGenericRegionCount();
             ++liGenericRegionIndex)
        {
            const BrnTrigger::GenericRegion* lpGenericRegion =
                lpTriggerData->GetGenericRegion(liGenericRegionIndex);
            if (lpGenericRegion->GetType() != leRegionType)
                continue;

            const BrnWorld::WorldRegion lTriggerWorldRegion =
                GetTriggerWorldRegion(lpGenericRegion);

            if (lbFirstTrigger)
            {
                const Vector3 lPosition = lpGenericRegion->GetBoxRegion()->GetPosition();
                maLastPositions[leStuntElementType] = lPosition;
                lbFirstTrigger = false;

                CgsDev::SimpleStrStream& lrStream = maStrStreams[leStuntElementType];
                lrStream.Reset();
                lrStream << "Last ";
                lrStream << KAPC_STUNT_NAMES[leStuntElementType];
                lrStream << ": ";
                lrStream << BrnWorld::WorldRegion::DistrictToString(
                    lTriggerWorldRegion.GetDistrict());
                lrStream << ", ";
                lrStream.AppendFormat("(%f, %f, %f)",
                                      lPosition.x, lPosition.y, lPosition.z);
            }
            else
            {
                CgsID lId = lpGenericRegion->GetGroupId();
                if (lId == 0)
                    lId = lpGenericRegion->GetId();

                mpStuntManager->GetProgressionManager()->GetProfile()->AddStuntElement(
                    leStuntElementType, lId, lTriggerWorldRegion.GetCounty());
            }
        }
    }

    // X360 0x8237A3A8 / 0x8237A3B0 / 0x8237A3B8: three tail-forwarders.
    void StuntManagerDebugComponent::CompleteAllJumpsButOneCallback(void* lpData)
    {
        static_cast<StuntManagerDebugComponent*>(lpData)->CompleteAllStuntTypeButOne(
            E_STUNT_ELEMENT_TYPE_JUMP);
    }

    void StuntManagerDebugComponent::CompleteAllSmashesButOneCallback(void* lpData)
    {
        static_cast<StuntManagerDebugComponent*>(lpData)->CompleteAllStuntTypeButOne(
            E_STUNT_ELEMENT_TYPE_SMASH);
    }

    void StuntManagerDebugComponent::CompleteAllBillboardsButOneCallback(void* lpData)
    {
        static_cast<StuntManagerDebugComponent*>(lpData)->CompleteAllStuntTypeButOne(
            E_STUNT_ELEMENT_TYPE_BILLBOARD);
    }
}
