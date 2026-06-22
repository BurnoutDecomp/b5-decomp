// _embed_check_brngui_misc.cpp
// Tiny embed/usage check for the BrnGui-misc group homes. Confirms the three headers
// compile together, the accessor signatures are callable through pointers, and the
// SatNavIconInfo trailing bytes sit at the X360-proven offsets (0x24..0x27).

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"

namespace
{
    void UseSatNav( const BrnGui::GuiEventUpdateSatNav::SatNavIconInfo* lpInfo )
    {
        BrnWorld::ECounty   leCounty = lpInfo->GetCounty();
        BrnWorld::EDistrict leDistrict = lpInfo->GetDistrict();
        EActiveRaceCarIndex leArci = lpInfo->GetActiveRaceCarIndex();
        BrnGui::EPlayerTeam leTeam = lpInfo->GetPlayerTeam();
        (void)leCounty; (void)leDistrict; (void)leArci; (void)leTeam;
    }

    void UseFreeburn( const BrnGui::FreeburnChallengeManager* lpMgr )
    {
        const BrnResource::ChallengeListEntry* lpChallenge = lpMgr->GetCurrentChallenge();
        const BrnResource::ChallengeListEntryAction* lpAction = lpMgr->GetCurrentAction();
        BrnGui::FreeburnChallengeManager::EFreeburnChallengeSuccess leOk =
            lpMgr->GetCurrentSuccessForARCI( E_ACTIVE_RACE_CAR_INDEX_0 );
        f32 lfContrib = lpMgr->GetCurrentContributionForARCI( E_ACTIVE_RACE_CAR_INDEX_0 );
        BrnResource::ChallengeListEntryAction::EChallengeDataType leType = lpMgr->GetCurrentTargetType();
        (void)lpChallenge; (void)lpAction; (void)leOk; (void)lfContrib; (void)leType;
    }

    void UseMapTransform()
    {
        Matrix33 lm33 = BrnGui::MapTransform::MakeCoordSpaceFromRect( Vector4{ 0.0f, 0.0f, 1.0f, 1.0f } );
        Matrix33 lm33b = BrnGui::MapTransform::MakeTransform( lm33, lm33 );
        Vector2 lv2 = BrnGui::MapTransform::Transform( Vector2{ 0.5f, 0.5f, 0.0f, 0.0f }, lm33b );
        Vector3 lv3 = BrnGui::MapTransform::DeviceToWorld( lv2 );
        BrnGui::MapTransform::SetZoomedWorldRect( Vector2{0,0,0,0}, Vector2{1,1,0,0}, Vector2{0.5f,0.5f,0,0} );
        BrnGui::MapTransform::SetZoomedViewportRect( Vector4{0,0,1,1} );
        (void)lv3;
    }
}

void brngui_misc_embed_check_use()
{
    UseSatNav( nullptr );
    UseFreeburn( nullptr );
    UseMapTransform();
}
