// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ProcessContactSpies.cpp
//
// THE RACE-CAR CONTACT-SPY DRIVER -- the function PhysicsModule::Update runs, on the non-catchup
// leg, over the frame's resolved race-car contacts. This is STAGE 0 of the takedown chain:
// every race-car-vs-race-car spy is handed to HandleRaceCarRaceCarContact (BrnVehicleManager.cpp),
// which is the only road to SetRaceCarCrashing -> AddRaceCarCrashEvent.
//
//   VehicleManager::ProcessContactSpies  @0x82646C98  (118 insns; DWARF BrnVehicleManager.cpp:4434)
//
// Slice TU (home BrnVehicleManager.cpp is mounted; this body lives in its own partfile like the
// other per-frame legs). The conductor gate that stood in BrnPhysicsConductorGates.cpp is DELETED
// in the same change (LNK2005 is the tripwire if it ever reappears).
//
// -------------------------------------------------------------------------------------------------
// THE WALK, verbatim (r28 == this, r20 == lpContactSpies, f30 == lfTimeStep):
//
//   0x82646CD0  stwx 0, this+0x2A244          -> muTakedownEventsThisFrame = 0   (+172612)
//   0x82646CD8  lwz  r19, 8(r20)              -> liNumContacts = mRaceCarContactQueue.GetLength()
//                                                (the queue is ContactSpyData's FIRST member, so
//                                                the queue pointer IS lpContactSpies; +8 is the
//                                                BaseEventQueue length word)
//   per contact i (0x82646D08..0x82646E44):
//     0x82646D10  BaseEventQueue<RaceCarContact>::GetEvent(i)   (stride 96, the 0x82285BB8 leaf)
//     0x82646D18  lbz 0(spy) == 1  else assert
//                 "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"        (:4457)
//                 (the owner is the TOP byte of the big-endian EntityId word -> GetOwner())
//     0x82646D3C  luOwnerB = lbz 4(spy)        -> mEntityIdB.GetOwner()
//     0x82646D44  if (luOwnerB == E_ENTITYTYPE_RACECAR)
//     0x82646D8C      HandleRaceCarRaceCarContact(lSpy BY VALUE -- 7 doublewords in r4..r10 +
//                     a 40-byte memcpy of +0x38.. onto the stack == the 96-byte RaceCarContact --
//                     lpRequest, lpVehicle, lpManager, lpDeformation, lfTimeStep)
//     0x82646D90  lpCarA = &maRaceCarVehicles[mEntityIdA.GetEntityIndex()]
//                 (`extrwi 14,8` == (id >> 10) & 0x3FFF; stride 0x1460, base +0x740)
//     0x82646DA4  if (lpCarA->mbCrashing (+0x710)              -> IsCrashing()
//     0x82646DB0      && !lpCarA->mbStartedFatallyCrashing (+0x711) -> !IsFatallyCrashing()
//     0x82646DC4      && lwz 4(spy) == lpCarA+0x1450)          -> mEntityIdB == GetEntityCausingCrash()
//                   (a WHOLE-WORD compare of the EntityId; the earlier lbz was its owner byte)
//     0x82646DCC      if (luOwnerB == RACECAR || luOwnerB == TRAFFIC_VEHICLE)    -> set (below)
//     0x82646DE4      else n = lpCarA->GetNormalCausingCrash()          (asserts mbCrashing itself)
//     0x82646DE8..E0C   vspltisw -1 ; vslw ; vxor  == flip the sign bit of every lane == -n
//     0x82646DF4        lvx128 spy+0x30                                  -> lSpy.mNormal
//     0x82646E18        vmsum3fp128                                      -> vpu::Dot(mNormal, -n)
//     0x82646E24        vcmpgtfp. against the 0.7f splat (flt_82004C68)  -> operator>
//     0x82646E38      stb 1, 0x711(lpCarA)                               -> SetFatallyCrashing()
//   0x82646E50  ProcessShowtimeShunts(lpContactSpies)                     @0x82629F20
//   0x82646E5C  mPhysicalTrafficManager.DisposeOfNonCrashingTraffic()     @0x825EFB40 (this+44768)
//
// The DWARF variable hints for :4434 name exactly this set: lpRaceCarContactSpyQueue,
// liNumContacts, i, lSpy, luOwnerB, lpCarA, and the callees BaseEventQueue<RaceCarContact>::
// GetEvent, EntityId::operator u32, RaceCarPhysics::GetNormalCausingCrash, vpu::Dot,
// vpu::operator>, vpu::operator-, SimpleVehiclePhysics::SetFatallyCrashing.
//
// POLARITY NOTES (read twice, this runs every frame):
//   * The fatal-crash latch is raised only for the car ALREADY crashing (mbCrashing set) whose
//     recorded crash-causer is the other party of THIS contact. Any car or traffic causer
//     latches at once; anything else (world, prop) latches only when the contact normal is
//     within ~45 degrees of the OPPOSITE of the stored crash normal (dot > 0.7).
//   * HandleRaceCarRaceCarContact runs BEFORE that check, so a crash it just committed via
//     SetRaceCarCrashing is visible to the latch on the same contact -- console order.
//   * There is NO range check on the decoded race-car index (the console has none either; the
//     assert on the owner byte is the only guard, and it is not a guard).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // IsCrashing / IsFatallyCrashing / GetEntityCausingCrash / GetNormalCausingCrash / SetFatallyCrashing
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"       // DisposeOfNonCrashingTraffic (the tail call)
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"                  // ContactSpyData::GetRaceCarContacts / RaceCarContactQueue
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                // RaceCarContact (mEntityIdA/B, mNormal)
#include "GameSource/World/BrnEntityTypes.h"                                    // BrnWorld::E_ENTITYTYPE_RACECAR / _TRAFFIC_VEHICLE
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                      // vpu::Dot, unary operator-

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // flt_82004C68 == 0.7f (read from the image). The cosine threshold between the incoming
        // contact normal and the NEGATED stored crash normal that promotes a world/prop contact
        // to a fatal crash. FLAG: constant NAME proposed (no DWARF symbol for the literal); the
        // value and its use are asm-literal.
        const f32 KF_FATAL_CRASH_NORMAL_DOT_THRESHOLD = 0.7f;
    }

    void VehicleManager::ProcessContactSpies(const ContactSpy::ContactSpyData* lpContactSpies,
                                             BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                             VehicleOutputInterface* lpVehicleOutputInterface,
                                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                             Deformation::DeformationManager* lpDeformationManager,
                                             f32 lfTimeStep)
    {
        (void)lpDeformationManager;   // asm: r9 (the manager) is never read in the 118 instructions.

        muTakedownEventsThisFrame = 0;                                          // 0x82646CD0

        const ContactSpy::ContactSpyData::RaceCarContactQueue* lpRaceCarContactSpyQueue =
            lpContactSpies->GetRaceCarContacts();
        const s32 liNumContacts = lpRaceCarContactSpyQueue->GetLength();        // 0x82646CD8

        for (s32 i = 0; i < liNumContacts; ++i)
        {
            const ContactSpy::RaceCarContact& lSpy = lpRaceCarContactSpyQueue->GetEvent(i);   // 0x82646D10

            // EntityId here is the physics-side `{ u32 muValue; }` word (BrnCommonTypes.h), so the
            // owner/index reads are spelled on the word exactly as HandleRaceCarRaceCarContact does:
            // owner == the top byte (`>> 24`), index == bits 10..23 (`(>> 10) & 0x3FFF`).
            CGS_ASSERT(static_cast<u8>(lSpy.mEntityIdA.muValue >> 24) == BrnWorld::E_ENTITYTYPE_RACECAR,
                       "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :4457

            const u8 luOwnerB = static_cast<u8>(lSpy.mEntityIdB.muValue >> 24);   // 0x82646D3C lbz 4(spy)

            if (luOwnerB == BrnWorld::E_ENTITYTYPE_RACECAR)                     // 0x82646D44
            {
                HandleRaceCarRaceCarContact(lSpy,                               // 0x82646D8C (by value)
                                            lpRequestOutputInterface,
                                            lpVehicleOutputInterface,
                                            lpManagerOutputInterface,
                                            lpDeformationInterface,
                                            lfTimeStep);
            }

            // Does this contact confirm the crash car A is already in? If the other party is the
            // recorded crash-causer, the crash becomes fatal -- immediately for a car or a traffic
            // causer, else only when the contact comes from the opposite side of the stored normal.
            RaceCarPhysics* lpCarA = &maRaceCarVehicles[(lSpy.mEntityIdA.muValue >> 10) & 0x3FFF];   // 0x82646D90 extrwi 14,8
            if (lpCarA->IsCrashing()                                            // 0x82646DA4  +0x710
                && !lpCarA->IsFatallyCrashing()                                 // 0x82646DB0  +0x711
                && lSpy.mEntityIdB.muValue == lpCarA->GetEntityCausingCrash().muValue)   // 0x82646DC4  +0x1450 (whole word)
            {
                if (luOwnerB == BrnWorld::E_ENTITYTYPE_RACECAR                  // 0x82646DCC
                    || luOwnerB == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE       // 0x82646DD4
                    || vpu::Dot(lSpy.mNormal, -lpCarA->GetNormalCausingCrash()) // 0x82646DE4..E18
                           > KF_FATAL_CRASH_NORMAL_DOT_THRESHOLD)               // 0x82646E24 vcmpgtfp
                {
                    lpCarA->SetFatallyCrashing();                               // 0x82646E38 stb 1,0x711
                }
            }
        }

        ProcessShowtimeShunts(lpContactSpies);                                  // 0x82646E50 @0x82629F20
        mPhysicalTrafficManager.DisposeOfNonCrashingTraffic();                  // 0x82646E5C @0x825EFB40
    }

} // namespace Vehicle
} // namespace BrnPhysics
