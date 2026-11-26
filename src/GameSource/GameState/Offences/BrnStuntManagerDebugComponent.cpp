// X360 0x827DB6B0. Compiler-synthesized default constructor (the LegacySrc partial source has no hand-written
// body; the DecFIGS DWARF emits an empty body at BrnStuntManagerDebugComponent.h:50). It exists
// only because members need non-trivial construction: the CgsDev::DebugComponent base (installs its
// vtable, off_820CDE8C as the most-derived) and the three CgsDev::SimpleStrStream subobjects (each
// runs StrStreamBase() -> installs base vtable off_82000D00 + mePrintMode=0, then SimpleStrStream()
// -> installs derived vtable off_82014B00 + resets the inline buffer). The X360 pseudocode is
// exactly that, unrolled (stride 0x108 = 264B per element, 3 elements). mpStuntManager is left
// uninitialised (assigned later by Construct()); the three Vector3 maLastPositions are trivially
// default-constructed. Reusing the real committed base + member types lands the natural layout on
// the X360 offsets (DebugComponent base +0x00..+0x0F, mpStuntManager +0x0C, maStrStreams[0] +0x10
// == r26+0x10), so an explicitly-defaulted ctor regenerates the observed code with no raw-offset
// access and no manual padding.
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnStuntManagerDebugComponent.cpp"
namespace BrnGameState
{
    StuntManagerDebugComponent::StuntManagerDebugComponent()
        : CgsDev::DebugComponent(CgsDev::DebugComponent::GameStateConstructorTag())
    {
    }

    // ------------------------------------------------------------------------
    // [gateui] Construct @ X360 0x82358C80 -- BODIED 2026-08-20.
    //
    // Why it had to land: StuntManager::Construct calls it (BrnStuntManager.cpp :: Construct,
    // `mStuntManagerDebugComponent.Construct(this)`), and the component is held BY VALUE at the
    // manager's +0x000 -- so mounting BrnStuntManager.cpp without this body is an LNK2019. It was
    // on the round-1 verify pass's measured UNDEF list.
    //
    // The console body is three things and nothing else:
    //   0x82358C9C  bl CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct
    //               -- an ICF FOLD, not a collision call: 0x8284CB38 is a bare `blr`, so the
    //                  linker folded every empty function in the image onto it. The one that
    //                  belongs here is the DebugComponent base's own Construct.
    //   0x82358CA4  stw r30, 0xC(r31)                 -- mpStuntManager = lpStuntManager
    //   0x82358CA8..0x82358CE4  a 3-iteration `stvx128` loop from r31+0x330, stride 0x10, storing
    //               a stack quad built from flt_82001CC0 (== 0.0f) x3 + a zeroed high lane
    //               -- i.e. ZERO THE THREE maLastPositions.
    // ⓘ 0x330 == 816, not the 808 that `maStrStreams` (+0x10, 3 x 264B) ends at: Vector3 is
    // 16-byte aligned (rw::math::vpu), so the compiler pads 808 -> 816. Reproduced by NAME here,
    // which lands the same padding on the host without baking either literal.
    // ------------------------------------------------------------------------
    void StuntManagerDebugComponent::Construct(StuntManager* lpStuntManager)
    {

        mpStuntManager = lpStuntManager;

        for (s32 liIndex = 0; liIndex < 3; ++liIndex)
        {
            maLastPositions[liIndex].SetZero();   // the console's zeroed stack quad, stvx128'd
        }
    }
}

// FOLDED FROM StuntManagerDebugComponent_gUI_00.cpp
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

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
            lpRender->DrawText(maStrStreams[liIndex].GetBuffer(),
                          100.0f, 100.0f + static_cast<f32>(liIndex) * 20.0f,
                          16.0f, 0xFFFFFFFFu);
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
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpData",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                112));
            CgsDev::Assert::EndAssert();
        }
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                114));
            CgsDev::Assert::EndAssert();
        }
        if (lpComponent->mpStuntManager == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent->mpStuntManager",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                115));
            CgsDev::Assert::EndAssert();
        }
        lpComponent->mpStuntManager->SetDebugCompletedUnlockType(E_STUNT_ELEMENT_TYPE_JUMP);
    }

    void StuntManagerDebugComponent::CompleteAllSmashes(void* lpData)
    {
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpData",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                130));
            CgsDev::Assert::EndAssert();
        }
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                132));
            CgsDev::Assert::EndAssert();
        }
        if (lpComponent->mpStuntManager == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent->mpStuntManager",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                133));
            CgsDev::Assert::EndAssert();
        }
        lpComponent->mpStuntManager->SetDebugCompletedUnlockType(E_STUNT_ELEMENT_TYPE_SMASH);
    }

    void StuntManagerDebugComponent::CompleteAllStunt(void* lpData)
    {
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpData",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                147));
            CgsDev::Assert::EndAssert();
        }
        StuntManagerDebugComponent* lpComponent =
            static_cast<StuntManagerDebugComponent*>(lpData);
        if (lpData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                149));
            CgsDev::Assert::EndAssert();
        }
        if (lpComponent->mpStuntManager == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpStuntManagerDebugComponent->mpStuntManager",
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                150));
            CgsDev::Assert::EndAssert();
        }
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
            CGS_FAIL_AT("Unknown stunt type.", CGS_ARTIST_FILE, 245);
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
                                      lPosition.GetX().GetFloat(),
                                      lPosition.GetY().GetFloat(),
                                      lPosition.GetZ().GetFloat());
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

#undef CGS_ARTIST_FILE
