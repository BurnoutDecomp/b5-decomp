// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Create.cpp
//
// Wave T3 round 1, cluster C2 -- the PHYSICS-SIDE CREATE DRAIN. Everything between
// PhysicalTrafficManager::ProcessTrafficMaintenanceEvents' create arm and a seated physics body.
//
//   ProcessCreateEvents                 @0x826495E8 ( 95)  export HOLE, closed by the wave-T3 scout
//   ProcessCreateNonArticulatedTraffic  @0x82647E20 (617)
//   CreateTrafficVehicle                @0x82646E70 (656)
//   GetFreeTrafficVehicleWithPhysics    @0x82637608 (266)
//   GetLeastInterestingFullyPhysicalVehicle @0x825F1990 (446)
//   GetTrafficInterest                  @0x825F1658 (206)
//   PreparePhysicsForNewTrafficVehicle  @0x826440D0 (210)
//   ProcessCreateArticulatedTrafficEvents @0x826487C8 (614)  -- named gate (trailers parked)
//
// Slice TU: the console home BrnPhysicalTrafficManager.cpp is a different, already-mounted file,
// so these bodies land in their own translation unit -- the same shape as the sibling
// BrnPhysicalTrafficManager_Prepare.cpp / _UpdateTrafficDriver.cpp slices.
//
// -------------------------------------------------------------------------------------------------
// ⭐ ONE INDEX, TWO NAMES (GetFreeTrafficVehicleWithPhysics). The console computes
// mUsedTrafficVehicles.GetFirstZeroBit() ONCE (r31 @0x82637790) and then fires BOTH of the source's
// bound asserts against it -- ":3002 liFreeFullTrafficPhysics < ku8MaxNumFullyPhysicalTraffic"
// (0x826377F0 `li r5,0xBBA`) and ":3003 liFreeTrafficVehicle < ku8TotalMaxNumPhysicalTraffic"
// (0x8263781C `li r5,0xBBB`) -- and sets the matching bit in BOTH bitsets. Both console constants
// are 20 here (`cmpwi r31,0x14` twice; maFullTrafficPhysics[20] and BitArray<20u> for both
// bitsets), so the one host constant KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC serves both bounds. The
// traffic-vehicle slot index and the FULL-physics pool index are the same number by construction.
// Reproduced as one value with both assert texts, not as two independently derived indices.
//
// ⚠️ NO FEB-2007 SOURCE EXISTS FOR ANY OF THIS. references/Feb-2007/.../Physics/VehicleManager/
// holds only BrnVehicleConstants.h, SharedIO/ and VehiclePhysics/ -- there is no leaked
// PhysicalTrafficManager at all. Everything below is ARTIST pseudocode + asm, with the DecFIGS
// DWARF for declaration shape.
//
// ⚠️ HEX-RAYS MERGES THE SWITCH/BIT-WALK ARMS in ProcessCreateNonArticulatedTraffic and
// GetLeastInterestingFullyPhysicalVehicle. Both are written from the ASM; the pseudocode's
// interleaved `GetFirstNonZeroBit`/`GetNextNonZeroBit` cursor expansions are re-rolled into the
// container's own accessors, which is what the source had.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"   // the two create queues
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // CreatePhysicalTrafficEvent
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"       // the interest walk's stride
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"                // the +0x158 handling RefSpec
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "rw/math/vpu/matrix44affine_operation.h"                                  // IsValid(Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // the named gates + [T3-*] diag
#include <cstdlib>                                                                 // getenv (BRN_TRAFFIC_DIAG)

namespace
{
    // ---- named one-shot gate + [T3-*] diag plumbing (DELETE-WHEN-STABLE) ------------------------
    inline void TrafficCreateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }

    inline bool TrafficDiagOn()
    {
        static const bool sbOn = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbOn;
    }

    // gpDebugPrint is an extern DebugPrint* (CgsLog.h:33) that is NULL until the log front-end is
    // constructed; this drain runs at PostSceneUpdate, so every [T3-*] line must null-test it.
    // Same shape as the wave-T3 precedent BrnTrafficJob.cpp:36 / BrnUpdateVehiclesJob.cpp:61.
    inline CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagOn() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // 2.0f flt_82001D9C, 0.5f flt_82001DA0 -- the same two the race-car create drain names.
    const f32 KF_HALF = 0.5f;
}

#define BRN_T3_CREATE_GATE(TAG)                                                              \
    do { static bool s_bLogged = false;                                                      \
         TrafficCreateLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{
namespace Vehicle
{
    // =============================================================================================
    // ProcessCreateEvents @0x826495E8 (95)
    //   asserts BrnPhysicalTrafficManager.cpp:791..:796 (a2..a6 and a8 -- lpaRaceCarDrivers, a7,
    //   is deliberately NOT asserted; reproduced as shipped).
    //   DWARF BrnPhysicalTrafficManager.h:318.
    //
    // The two queue arguments are the console's `lpInputInterface + 132976` and `+ 136592`, reached
    // BY NAME through the DWARF-attested readers this wave added to BrnVehicleInputInterface.h.
    // =============================================================================================
    void PhysicalTrafficManager::ProcessCreateEvents(
            const VehicleInputInterface* lpInputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface,
            RaceCarPhysics* lpaRaceCarVehicles,
            VehicleDriver* lpaRaceCarDrivers,
            CgsContainers::BitArray<8u>* lpUsedRaceCars)
    {
        CGS_ASSERT(lpInputInterface          != 0, "lpInputInterface != NULL");          // :791
        CGS_ASSERT(lpManagerOutputInterface  != 0, "lpManagerOutputInterface != NULL");  // :792
        CGS_ASSERT(lpOutputRequestInterface  != 0, "lpOutputRequestInterface != NULL");  // :793
        CGS_ASSERT(lpDeformationInterface    != 0, "lpDeformationInterface != NULL");    // :794
        CGS_ASSERT(lpaRaceCarVehicles        != 0, "lpaRaceCarVehicles != NULL");        // :795
        CGS_ASSERT(lpUsedRaceCars            != 0, "lpUsedRaceCars != NULL");            // :796

        // ⛔ GATE ArticulatedJointPool::RemoveBrokenJointsFromSimulation @0x82601028 (920)
        //    blocker: declare-only on the pool's reconstructed surface (BrnArticulatedJointPool.h
        //    scores it "not bodied"); the whole articulated sub-tree is parked this round.
        //    DELETE-WHEN ProcessCreateArticulatedTrafficEvents lands -- until then no joint is ever
        //    created, so the pool's broken-joint set is always empty and this pass is a no-op.
        BRN_T3_CREATE_GATE("ArticulatedJointPool::RemoveBrokenJointsFromSimulation @0x82601028 (920)");

        ProcessCreateNonArticulatedTraffic(lpInputInterface->GetCreateTrafficBodyEvents(),
                                           lpManagerOutputInterface, lpOutputRequestInterface,
                                           lpDeformationInterface, lpaRaceCarVehicles,
                                           lpaRaceCarDrivers, lpUsedRaceCars);

        ProcessCreateArticulatedTrafficEvents(lpInputInterface->GetCreateArticulatedTrafficQueue(),
                                              lpManagerOutputInterface, lpOutputRequestInterface,
                                              lpDeformationInterface, lpaRaceCarVehicles,
                                              lpaRaceCarDrivers, lpUsedRaceCars);
    }

    // =============================================================================================
    // ProcessCreateArticulatedTrafficEvents @0x826487C8 (614)  -- NAMED GATE
    //   blocker: trailers/cab-trailer articulation are parked for wave T3 round 1 (the whole
    //   ArticulatedJointPool create/remove surface is declare-only, and wave-T2 generation builds
    //   InitialiseAsStandard cars only, so this queue is empty at runtime).
    //   DELETE-WHEN the articulated wave lands VehicleInputInterface::CreateArticulatedTraffic
    //   @0x8271C9C0 and the pool's ConstructArticulatedJoint/CreateJoint bodies.
    // =============================================================================================
    void PhysicalTrafficManager::ProcessCreateArticulatedTrafficEvents(
            const CgsModule::EventQueue<CreateArticulatedTrafficEvent, 10>* lpCreateArticulatedTrafficEvents,
            VehicleManagerOutputInterface*, VehicleOutputRequestInterface*,
            Deformation::DeformationInputInterface*, RaceCarPhysics*, VehicleDriver*,
            CgsContainers::BitArray<8u>*)
    {
        (void)lpCreateArticulatedTrafficEvents;
        BRN_T3_CREATE_GATE("PhysicalTrafficManager::ProcessCreateArticulatedTrafficEvents @0x826487C8 (614)");
    }

    // =============================================================================================
    // ProcessCreateNonArticulatedTraffic @0x82647E20 (617)
    //   asserts BrnPhysicalTrafficManager.cpp:845..:849 (again no lpaRaceCarDrivers check),
    //   :870 :878 :890 :891 :892 :893 :894 :898.  DWARF :324.
    //
    // TWO passes over the SAME queue, in this order (both are real: each copies the event by value,
    // 0x82647FBC.. and 0x82648394..):
    //   1. a validation sweep -- no duplicate mVolumeInstanceID left in the queue, and no live
    //      traffic slot already carrying this global entity id;
    //   2. the work sweep -- five more asserts, then CreateTrafficVehicle.
    //
    // lDoNotRecycleBitArray is declared and zeroed BEFORE pass 1 (`std r21,0(r11)` twice at
    // 0x82647F44/48 -- the console writes the one 8-byte BitArray<20> word twice; idempotent, done
    // once here) and is asserted EMPTY at the top of every pass-2 iteration (:898). Nothing in this
    // build ever sets a bit in it: it exists so a create batch cannot recycle a slot it just
    // created, and CreateTrafficVehicle only ever READS it.
    // =============================================================================================
    void PhysicalTrafficManager::ProcessCreateNonArticulatedTraffic(
            const CgsModule::EventQueue<CreatePhysicalTrafficEvent, 25>* lpCreateTrafficEvents,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface,
            RaceCarPhysics* lpaRaceCarVehicles,
            VehicleDriver* lpaRaceCarDrivers,
            CgsContainers::BitArray<8u>* lpUsedRaceCars)
    {
        CGS_ASSERT(lpCreateTrafficEvents     != 0, "lpCreateTrafficEvents != NULL");     // :845
        CGS_ASSERT(lpManagerOutputInterface  != 0, "lpManagerOutputInterface != NULL");  // :846
        CGS_ASSERT(lpDeformationInterface    != 0, "lpDeformationInterface != NULL");    // :847
        CGS_ASSERT(lpaRaceCarVehicles        != 0, "lpaRaceCarVehicles != NULL");        // :848
        CGS_ASSERT(lpUsedRaceCars            != 0, "lpUsedRaceCars != NULL");            // :849

        TotalPhysicalTrafficBitArray lDoNotRecycleBitArray;
        lDoNotRecycleBitArray.UnSetAll();

        // ---- pass 1: the validation sweep --------------------------------------------------------
        for (s32 liEvent = 0; liEvent < lpCreateTrafficEvents->GetLength(); ++liEvent)
        {
            const CreatePhysicalTrafficEvent lCreateTrafficEvent = lpCreateTrafficEvents->GetEvent(liEvent);

            for (s32 liOther = liEvent + 1; liOther < lpCreateTrafficEvents->GetLength(); ++liOther)
            {
                // `ld r11,0(r3) ; cmpld r30,r11` -- the WHOLE 64-bit VolumeInstanceId, not its
                // entity word. (The console's own typo is preserved in the message.)
                CGS_ASSERT(lpCreateTrafficEvents->GetEvent(liOther).mVolumeInstanceID.muId
                               != lCreateTrafficEvent.mVolumeInstanceID.muId,
                           "Duplicate crash body messgae in input buffer");              // :870
            }

            // `srdi r11,r30,32` -- the embedded entity word is the HIGH dword (DWARF
            // CgsVolumeInstanceId.h:85 `EntityId GetEntityId() const`, folded in place here).
            const u32 luGlobalEntityId =
                static_cast<u32>(lCreateTrafficEvent.mVolumeInstanceID.muId
                                 >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);

            for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
                 liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
                 liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
            {
                CGS_ASSERT(maTrafficEntityIDs[liVehicle].muValue != luGlobalEntityId,
                           "Traffic already has body");                                  // :878
            }
        }

        // ---- pass 2: the work sweep --------------------------------------------------------------
        for (s32 liEvent = 0; liEvent < lpCreateTrafficEvents->GetLength(); ++liEvent)
        {
            const CreatePhysicalTrafficEvent lCreateTrafficEvent = lpCreateTrafficEvents->GetEvent(liEvent);

            CGS_ASSERT(lCreateTrafficEvent.mVolumeInstanceID.GetEntityIDOwner()
                           == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                       "lCreateTrafficEvent.mVolumeInstanceID.GetEntityId().GetOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");                        // :890
            CGS_ASSERT(lCreateTrafficEvent.meTrafficType < E_TRAFFIC_TYPE_COUNT,
                       "lCreateTrafficEvent.meTrafficType < BrnPhysics::Vehicle::E_TRAFFIC_TYPE_COUNT"); // :891
            CGS_ASSERT(rw::math::vpu::IsValid(lCreateTrafficEvent.mInitialTransform),
                       "RwMath::IsValid( lCreateTrafficEvent.mInitialTransform )");      // :892
            CGS_ASSERT(rw::math::vpu::IsValid(lCreateTrafficEvent.mInitialVelocity),
                       "RwMath::IsValid( lCreateTrafficEvent.mInitialVelocity )");       // :893
            CGS_ASSERT(rw::math::vpu::IsValid(lCreateTrafficEvent.mAngularVelocity),
                       "RwMath::IsValid( lCreateTrafficEvent.mAngularVelocity )");       // :894
            CGS_ASSERT(lDoNotRecycleBitArray.GetFirstNonZeroBit()
                           == TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX,
                       "lDoNotRecycleBitArray.GetFirstNonZeroBit() == -1");              // :898

            CreateTrafficVehicle(lCreateTrafficEvent, lpManagerOutputInterface,
                                 lpOutputRequestInterface, lpDeformationInterface,
                                 lpaRaceCarVehicles, lpaRaceCarDrivers, lpUsedRaceCars,
                                 &lDoNotRecycleBitArray);
        }
    }

    // =============================================================================================
    // CreateTrafficVehicle @0x82646E70 (656)
    //   asserts BrnPhysicalTrafficManager.cpp:1095..:1100, :1110 (streamed), :1139, :1140.
    //   DWARF :330. Returns the physical slot index.
    // =============================================================================================
    s32 PhysicalTrafficManager::CreateTrafficVehicle(
            const CreatePhysicalTrafficEvent& lrCreateTrafficEvent,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface,
            RaceCarPhysics* lpaRaceCarVehicles,
            VehicleDriver* lpaRaceCarDrivers,
            CgsContainers::BitArray<8u>* lpUsedRaceCars,
            const TotalPhysicalTrafficBitArray* lpDoNotRecycleBitArray)
    {
        CGS_ASSERT(lrCreateTrafficEvent.mVolumeInstanceID.GetEntityIDOwner()
                       == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                   "lCreateTrafficEvent.mVolumeInstanceID.GetEntityId().GetOwner() == "
                   "BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");                            // :1095
        CGS_ASSERT(lrCreateTrafficEvent.meTrafficType < E_TRAFFIC_TYPE_COUNT,
                   "lCreateTrafficEvent.meTrafficType < BrnPhysics::Vehicle::E_TRAFFIC_TYPE_COUNT"); // :1096
        CGS_ASSERT(rw::math::vpu::IsValid(lrCreateTrafficEvent.mInitialTransform),
                   "RwMath::IsValid( lCreateTrafficEvent.mInitialTransform )");          // :1097
        CGS_ASSERT(rw::math::vpu::IsValid(lrCreateTrafficEvent.mInitialVelocity),
                   "RwMath::IsValid( lCreateTrafficEvent.mInitialVelocity )");           // :1098
        CGS_ASSERT(rw::math::vpu::IsValid(lrCreateTrafficEvent.mAngularVelocity),
                   "RwMath::IsValid( lCreateTrafficEvent.mAngularVelocity )");           // :1099
        CGS_ASSERT(lpDoNotRecycleBitArray != 0, "lpDoNotRecycleBitArray != NULL");       // :1100

        // `srdi r10,r10,32 ; clrlwi r10,r10,0` -- the global entity word.
        const u32 luGlobalEntityId =
            static_cast<u32>(lrCreateTrafficEvent.mVolumeInstanceID.muId
                             >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);

        for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
             liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
        {
            // The console streams the message ("Tried to add traffic <index> to physics module
            // twice\n"); collapsed to the static text, which is the committed precedent for
            // streamed asserts whose only variable part is the index the caller already has.
            CGS_ASSERT(maTrafficEntityIDs[liVehicle].muValue != luGlobalEntityId,
                       "Tried to add traffic vehicle to physics module twice");          // :1110
        }

        const s32 liFreeVehicleIndex = GetFreeTrafficVehicleWithPhysics(
                lpManagerOutputInterface, lpOutputRequestInterface, lpDeformationInterface,
                lpaRaceCarVehicles, lpaRaceCarDrivers, lpUsedRaceCars, lpDoNotRecycleBitArray);

        // `lwz r11,0x80(event) ; cmpwi 0 ; bne` -- only a POTENTIAL spawn joins that set.
        if (lrCreateTrafficEvent.meTrafficType == E_TRAFFIC_TYPE_POTENTIAL)
        {
            mPotentialTrafficVehicles.SetBit(static_cast<u32>(liFreeVehicleIndex));
        }

        PreparePhysicsForNewTrafficVehicle(lrCreateTrafficEvent, liFreeVehicleIndex);

        // `extrwi r11,r11,14,8` -- the 14-bit entity index inside the entity word.
        const u32 luEntityIndex =
            lrCreateTrafficEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

        CGS_ASSERT(mu8GlobalToPhysicalEntityIndexMap[luEntityIndex] == KU8_INVALID_MAP,
                   "mu8GlobalToPhysicalEntityIndexMap[lCreateTrafficEvent.mVolumeInstanceID."
                   "GetEntityId().GetEntityIndex()] == KU8_INVALID_MAP");                // :1139
        CGS_ASSERT(liFreeVehicleIndex >= 0 && liFreeVehicleIndex < KU8_INVALID_MAP,
                   "liFreeVehicleIndex >= 0 && liFreeVehicleIndex < KU8_INVALID_MAP");   // :1140

        mu8GlobalToPhysicalEntityIndexMap[luEntityIndex] = static_cast<u8>(liFreeVehicleIndex);
        mAddedTrafficVehicles.SetBit(static_cast<u32>(liFreeVehicleIndex));

        // [T3-drain] one-shot: the first CreatePhysicalTraffic event ever drained.
        // DELETE-WHEN-STABLE.
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                *lpDiag << "[T3-drain] first CreatePhysicalTraffic drained: global="
                    << static_cast<s32>(luGlobalEntityId) << " idx=" << static_cast<s32>(luEntityIndex)
                    << " slot=" << liFreeVehicleIndex
                    << " type=" << static_cast<s32>(lrCreateTrafficEvent.meTrafficType)
                    << " FULL(mu8PhysicalType="
                    << static_cast<s32>(GetTrafficVehicle(liFreeVehicleIndex)->mu8PhysicalType)
                    << ")\n";
            }
        }

        return liFreeVehicleIndex;
    }

    // =============================================================================================
    // GetFreeTrafficVehicleWithPhysics @0x82637608 (266)
    //   asserts BrnPhysicalTrafficManager.cpp:2920..:2926, :3002, :3003. DWARF :306.
    // =============================================================================================
    s32 PhysicalTrafficManager::GetFreeTrafficVehicleWithPhysics(
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface,
            RaceCarPhysics* lpaRaceCarVehicles,
            VehicleDriver* lpaRaceCarDrivers,
            CgsContainers::BitArray<8u>* lpUsedRaceCars,
            const TotalPhysicalTrafficBitArray* lpDoNotRecycleBitArray)
    {
        CGS_ASSERT(lpManagerOutputInterface  != 0, "lpManagerOutputInterface != NULL");  // :2920
        CGS_ASSERT(lpOutputRequestInterface  != 0, "lpOutputRequestInterface != NULL");  // :2921
        CGS_ASSERT(lpDeformationInterface    != 0, "lpDeformationInterface != NULL");    // :2922
        CGS_ASSERT(lpaRaceCarVehicles        != 0, "lpaRaceCarVehicles != NULL");        // :2923
        CGS_ASSERT(lpaRaceCarDrivers         != 0, "lpaRaceCarDrivers != NULL");         // :2924
        CGS_ASSERT(lpUsedRaceCars            != 0, "lpUsedRaceCars != NULL");            // :2925
        CGS_ASSERT(lpDoNotRecycleBitArray    != 0, "lpDoNotRecycleBitArray != NULL");    // :2926

        s32 liFreeTrafficVehicle = mUsedTrafficVehicles.GetFirstClearBit();
        if (liFreeTrafficVehicle == TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX)
        {
            liFreeTrafficVehicle = GetLeastInterestingFullyPhysicalVehicle(
                    lpaRaceCarVehicles, lpaRaceCarDrivers, lpUsedRaceCars, lpDoNotRecycleBitArray);
            RecycleTrafficVehicle(liFreeTrafficVehicle, lpManagerOutputInterface,
                                  lpOutputRequestInterface, lpDeformationInterface);
        }

        // ⭐ ONE INDEX, TWO NAMES -- see the banner. r31 feeds both asserts and both SetBits.
        const s32 liFreeFullTrafficPhysics = liFreeTrafficVehicle;

        // Verbatim console texts: the FIRST names ku8MaxNumFullyPhysicalTraffic, the second
        // ku8TotalMaxNumPhysicalTraffic. Both are 20 (`cmpwi r31,0x14` at 0x826377E0/0x8263780C).
        CGS_ASSERT(liFreeFullTrafficPhysics >= 0
                       && liFreeFullTrafficPhysics < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liFreeFullTrafficPhysics >= 0 && liFreeFullTrafficPhysics < "
                   "static_cast<int32_t>( ku8MaxNumFullyPhysicalTraffic )");             // :3002
        CGS_ASSERT(liFreeTrafficVehicle >= 0
                       && liFreeTrafficVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liFreeTrafficVehicle >= 0 && liFreeTrafficVehicle < "
                   "static_cast<int32_t>( ku8TotalMaxNumPhysicalTraffic )");             // :3003

        mUsedTrafficVehicles.SetBit(static_cast<u32>(liFreeTrafficVehicle));
        mUsedFullTrafficPhysics.SetBit(static_cast<u32>(liFreeFullTrafficPhysics));

        PhysicalTrafficVehicle* lpTrafficVehicle = GetTrafficVehicle(liFreeTrafficVehicle);
        TrafficPhysics* lpFullTrafficPhysics = &maFullTrafficPhysics[liFreeFullTrafficPhysics];
        CGS_ASSERT(lpFullTrafficPhysics != 0, "lpFullTrafficPhysics != NULL");

        // `stw r30,0x1C ; stb r18(0),0x32 ; stb r31,0x31` -- ROUND 1 IS THE FULL BODY. There are
        // twenty FULL slots and exactly one SIMPLE one, and UpdateTrafficPhysics asserts
        // IsFullyPhysical(); every slot this function hands out is E_PHYSICAL_TRAFFIC_TYPE_FULL.
        lpTrafficVehicle->mpVehicleBody       = lpFullTrafficPhysics;
        lpTrafficVehicle->mu8PhysicalType     =
            static_cast<u8>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL);
        lpTrafficVehicle->mu8PhysicsPoolIndex = static_cast<u8>(liFreeFullTrafficPhysics);

        // [T3-body] one-shot: the first physics body ever seated. DELETE-WHEN-STABLE.
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                *lpDiag << "[T3-body] first traffic physics body: slot="
                    << liFreeTrafficVehicle << " pool=" << liFreeFullTrafficPhysics
                    << " mu8PhysicalType=" << static_cast<s32>(lpTrafficVehicle->mu8PhysicalType)
                    << " usedPopcount=" << static_cast<s32>(mUsedTrafficVehicles.CountSetBits())
                    << "\n";
            }
        }

        return liFreeTrafficVehicle;
    }

    // =============================================================================================
    // GetLeastInterestingFullyPhysicalVehicle @0x825F1990 (446)
    //   asserts BrnPhysicalTrafficManager.cpp:3345..:3348, :3356, :3400. DWARF :309.
    //
    // ⚠️ ONLY FULLY-PHYSICAL SLOTS ARE CANDIDATES (`lbz +0x32 ; cmpwi 0 ; bne skip`) and only ones
    // whose bit is clear in lpDoNotRecycleBitArray. liNumRecyclable is bumped for every candidate,
    // and the FIRST candidate always wins the compare (`cntlzw`/`extrwi` == "is the count zero").
    // =============================================================================================
    s32 PhysicalTrafficManager::GetLeastInterestingFullyPhysicalVehicle(
            RaceCarPhysics* lpaRaceCarVehicles,
            VehicleDriver* lpaRaceCarDrivers,
            CgsContainers::BitArray<8u>* lpUsedRaceCars,
            const TotalPhysicalTrafficBitArray* lpDoNotRecycleBitArray)
    {
        CGS_ASSERT(lpaRaceCarVehicles     != 0, "lpaRaceCarVehicles != NULL");           // :3345
        CGS_ASSERT(lpaRaceCarDrivers      != 0, "lpaRaceCarDrivers != NULL");            // :3346
        CGS_ASSERT(lpUsedRaceCars         != 0, "lpUsedRaceCars != NULL");               // :3347
        CGS_ASSERT(lpDoNotRecycleBitArray != 0, "lpDoNotRecycleBitArray != NULL");       // :3348

        f32 lfLeastInterest              = 0.0f;                              // flt_82001CC0
        s32 liLeastInterestingVehicle    = -1;
        s32 liNumRecyclable              = 0;

        CGS_ASSERT(mUsedFullTrafficPhysics.GetFirstClearBit()
                       == FullyPhysicalTrafficBitArray::KI_INVALID_BITINDEX,
                   "mUsedFullTrafficPhysics.GetFirstZeroBit() == -1");                   // :3356

        for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
             liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
        {
            const PhysicalTrafficVehicle* lpVehicle = GetTrafficVehicle(liVehicle);
            const u8 lu8Type = lpVehicle->mu8PhysicalType;
            CGS_ASSERT(lu8Type < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                       "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
            if (lu8Type != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
            {
                continue;
            }

            const f32 lfInterest = GetTrafficInterest(lpVehicle, lpaRaceCarVehicles,
                                                      lpaRaceCarDrivers, lpUsedRaceCars);
            if (lpDoNotRecycleBitArray->IsBitSet(static_cast<u32>(liVehicle)))
            {
                continue;
            }

            if (liNumRecyclable == 0 || lfInterest < lfLeastInterest)
            {
                lfLeastInterest           = lfInterest;
                liLeastInterestingVehicle = liVehicle;
            }
            ++liNumRecyclable;
        }

        // Streamed on the console ("Failed to recycle fully-physical traffic ... <n> recyclable
        // vehicles"); collapsed to the static text.
        CGS_ASSERT(liLeastInterestingVehicle >= 0,
                   "Failed to recycle fully-physical traffic vehicle");                  // :3400

        return liLeastInterestingVehicle;
    }

    // =============================================================================================
    // GetTrafficInterest @0x825F1658 (206)
    //   assert BrnPhysicalTrafficManager.cpp:2886. DWARF :303.
    //
    // "Interest" == 1 / (1 + squared distance to the nearest PLAYER-driven race car). Only
    // E_DRIVER_TYPE_PLAYER cars count (`mulli r11,r10,0xE0 ; lwz 0xD0(r11) ; cmpwi 0 ; bne skip`),
    // so a lobby of AI rivals leaves every traffic car equally (un)interesting -- which is what
    // makes the car nearest the PLAYER the last one recycled.
    // =============================================================================================
    f32 PhysicalTrafficManager::GetTrafficInterest(const PhysicalTrafficVehicle* lpTrafficVehicle,
                                                   RaceCarPhysics* lpaRaceCarVehicles,
                                                   VehicleDriver* lpaRaceCarDrivers,
                                                   CgsContainers::BitArray<8u>* lpUsedRaceCars)
    {
        // flt_8208F5EC == 0x7F7FFFFF == FLT_MAX.
        f32 lfMinSquareDistToRaceCar = 3.402823466e+38F;

        // `lwz r9,0x1C(r4) ; lvx128 v127,r9,0x40` -- the body's transform translation row.
        const Vector3 lvTrafficPosition = lpTrafficVehicle->mpVehicleBody->GetPosition();

        for (s32 liCar = lpUsedRaceCars->GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = lpUsedRaceCars->GetNextNonZeroBit(liCar))
        {
            if (lpaRaceCarDrivers[liCar].GetDriverType() != E_DRIVER_TYPE_PLAYER)
            {
                continue;
            }

            const Vector3 lvDelta =
                rw::math::vpu::Subtract(lpaRaceCarVehicles[liCar].GetPosition(), lvTrafficPosition);
            const f32 lfSquareDist = rw::math::vpu::Dot(lvDelta, lvDelta);

            // `fsubs f13,f31,f0 ; fsel f31,f13,f0,f31` == min(min, sq).
            if (lfSquareDist < lfMinSquareDistToRaceCar)
            {
                lfMinSquareDistToRaceCar = lfSquareDist;
            }
        }

        CGS_ASSERT(lfMinSquareDistToRaceCar >= 0.0f, "lfMinSquareDistToRaceCar >= 0.0f"); // :2886

        return 1.0f / (lfMinSquareDistToRaceCar + 1.0f);
    }

    // =============================================================================================
    // PreparePhysicsForNewTrafficVehicle @0x826440D0 (210)
    //   asserts BrnPhysicalTrafficManager.cpp:1174, :1192, :1195; BrnPhysicalTrafficManager.h:740/:741.
    //   DWARF :300 `void (const CreatePhysicalTrafficEvent&, int32_t)`.
    //
    // The AttribSys chase is the SAME inlined sequence VehicleManager::ProcessCreateEvents already
    // carries for a race car (BrnVehicleManager_ProcessCreateEvents.cpp:205-218), one for one:
    //   sub_82204998                    == Attrib::Gen::burnoutcarasset(key, NULL)
    //   +0x158 RefSpec -> GetCollection == GetPhysicsVehicleHandlingRefSpec()->GetCollection()
    //   Attrib::Gen::physicsvehiclehandling(collection, NULL)
    //   VehicleAttribs::Construct
    //   sub_825BDB88                    == the checked physicsvehiclehandling COPY (by-value arg)
    //   VehicleAttribs::SetupAttribs(copy)
    // ⚠️ THE TRAFFIC ARM DOES **NOT** RE-CENTRE THE COM ON THE WHEEL MEAN. The race-car drain adds
    // the four-wheel mean into mBaseAttribs.mCOMOffset before building the box; this one does not
    // (no vaddfp/vmulfp on v125 anywhere in 0x82644290..0x826442F4). Do not "fix" that by copying
    // the sibling.
    // =============================================================================================
    void PhysicalTrafficManager::PreparePhysicsForNewTrafficVehicle(
            const CreatePhysicalTrafficEvent& lrCreateTrafficEvent, s32 liTrafficVehicleIndex)
    {
        CGS_ASSERT(liTrafficVehicleIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liTrafficVehicleIndex < ku8TotalMaxNumPhysicalTraffic");             // :1174

        maTrafficEntityIDs[liTrafficVehicleIndex].muValue =
            static_cast<u32>(lrCreateTrafficEvent.mVolumeInstanceID.muId
                             >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);
        maTrafficCarModelHandles[liTrafficVehicleIndex] = lrCreateTrafficEvent.mModelHandle;

        VehicleAttribs lVehicleAttribs;
        {
            Attrib::Gen::burnoutcarasset lCarAsset(lrCreateTrafficEvent.mCarAssetAttribKey, NULL);
            Attrib::Gen::physicsvehiclehandling lHandling(
                const_cast<Attrib::Collection*>(
                    lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), NULL);

            lVehicleAttribs.Construct();
            Attrib::Gen::physicsvehiclehandling lHandlingCopy(lHandling);
            lVehicleAttribs.SetupAttribs(lHandlingCopy);
        }
        // `ld r11,0x70(r27) ; std r11,var_78` -- VehicleAttribs + 0x358.
        lVehicleAttribs.mAttribsKey = lrCreateTrafficEvent.mCarAssetAttribKey;

        CGS_ASSERT(lrCreateTrafficEvent.mModelHandle.mpResourceMemory != 0,
                   "lpModelResource != NULL");                                           // :1192
        Deformation::StreamedDeformationSpec* lpModelData =
            *reinterpret_cast<Deformation::StreamedDeformationSpec* const*>(
                 lrCreateTrafficEvent.mModelHandle.mpResourceMemory);
        CGS_ASSERT(lpModelData != 0, "lpModelData != NULL");                             // :1195

        lpModelData->TransformToNewCOMSpace(lVehicleAttribs.mBaseAttribs.mCOMOffset);

        Vector3 lavWheelPositions[4];
        f32     lafWheelRadii[4];
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            // GetWheelSpec's own bound tripwire, emitted once per member read.
            CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
            lavWheelPositions[liWheel] = lpModelData->maWheelSpecs[liWheel].mPosition;
            CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");
            lafWheelRadii[liWheel] = KF_HALF * lpModelData->maWheelSpecs[liWheel].mScale.y;
        }

        CGS_ASSERT(liTrafficVehicleIndex >= 0, "liVehicle >= 0");                        // h:740
        CGS_ASSERT(liTrafficVehicleIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                         // h:741

        // The delegation C3 wires: PhysicalTrafficVehicle::PreparePhysical @0x82641058 ->
        // TrafficPhysics::PreparePhysical @0x82639380.
        mpaTrafficVehicles[liTrafficVehicleIndex].PreparePhysical(
                &lrCreateTrafficEvent, &lVehicleAttribs, lpModelData,
                lavWheelPositions, lafWheelRadii);

        // The inlined VehicleDriver::ClearControls over mpaTrafficDrivers[index] -- the same
        // out-of-line body the race-car create/remove drains already call by name.
        mpaTrafficDrivers[liTrafficVehicleIndex].ClearControls();
    }
}
}
