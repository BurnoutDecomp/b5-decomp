// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_GetUpdatedVehicleBodies.cpp
//
// THE PER-FRAME EXTERNAL-BODY PUBLISH -- the leg that tells the rigid-body simulation where
// every live car IS this frame. Two bodies, one slice TU (home TUs BrnVehicleManager.cpp /
// BrnPhysicalTrafficManager.cpp are still unmounted; this is the established slice pattern of
// BrnVehicleManager_ReadUpdatedBodies.cpp / _WriteOutVehicleStats.cpp -- fold back when the
// homes mount):
//
//   VehicleManager::GetUpdatedVehicleBodies         @0x82619340  (435 insns)  DWARF h:361 / .cpp:4154
//   PhysicalTrafficManager::GetUpdatedVehicleBodies @0x825EEF70  (~440 insns) DWARF h:162 / .cpp:1821
//
// Caller: PhysicsModule::BridgeUpdatedVehiclesToSimulation @0x825ADEA8, once per physics frame,
// AFTER UpdateVehiclePhysics and BEFORE the sim step, into a stack-local
// EventQueue<InUpdateExternalBody,60> that is then appended to the sim input. The sim's drain
// (PhysicsSimulationModule::ProcessUpdateExternalBodyQueue @0x828A3B30) looks each id up and
// SILENTLY skips one it does not know -- so a never-created proxy costs nothing.
//
// ---- WHY THIS EXISTS (2026-09-02, "prop collision at high speed only works sometimes") ------
// Every race car and every physical traffic car has a PROXY rigid body in the simulation --
// the E_ENTITYTYPE_PROP_COLLISION_RACECAR (11) / _TRAFFIC (12) body that ProcessCreateEvents /
// the traffic create path post with the car's mass. PropManager::SetupAndValidatePropContact
// re-routes every prop-vs-car contact onto that proxy (RoutePropVsRaceCarContactToDummyCar),
// and the sim solves the prop against IT. The proxy is an ACTIVE body with 10,000x drag: it
// goes nowhere on its own, and THIS function is the only thing that ever moves it. With this
// function gated the proxy sat at the car's creation pose with zero velocity for the whole
// session, so a whole prop promoted to E_PHYSICAL was solved against a body kilometres away
// with no momentum -- the lever arm made the proxy look weightless at the contact and the prop
// got nothing but a penetration nudge. Jointed props below their move threshold never noticed
// (HandleContactWithLean/TiltProp move them directly, off the car's own velocity), which is
// exactly the "works at low speed, not at high speed" shape of the report.
//
// ---- GROUND TRUTH (X360 ARTIST assembly, both functions read instruction by instruction) ----
// The Hex-Rays for 0x82619340 was an .ida-exports HOLE; it was pulled headless from the IDB on
// 2026-09-02 (tools/ida/export_all.py + EXPORT_ADDR_FILE) and the JSON landed beside the others
// in .ida-exports/BURNOUT_X360_ARTIST.XEX/. The traffic twin @0x825EEF70 was always exported.
//
// VehicleManager::GetUpdatedVehicleBodies -- register map (prologue 0x82619354..0x82619358):
//   r20 = this, arg_1C = r4 = lpUpdatedBodyQueue, r29 = the live-car index, r21 = 1, r28 = 0.
//   0x82619360  addis/addi this+0x10000-0x5340 == +44224 == mUsedRaceCars (one 64-bit word),
//               then the inlined BitArray<8>::GetFirstNonZeroBit (cntlzd on the lowest set bit);
//               no bit, or an index outside [0,8) -> straight to the traffic call (0x826199F0).
//   0x82619440  mulli r11,r29,0x1460 ; add this      -> &maRaceCarVehicles[idx] - 0x740 (the
//               console folds the +0x740 array base into every displacement below)
//   0x82619448  addi r10,r29,0x155C ; slwi 3 ; ldx    -> maRaceCarHandlingBodyIDs[idx] (+43744)
//   0x82619458  lbz 0x1B74(r11)      == car+0x1434 == RaceCarPhysics::mbAISlowMo
//   0x82619460  lvx128 v127, r11+0x7A0  == car+0x60 == mAngularVelocity
//   0x82619470  lvx128 v125..v122, r10=car+0x10 +0/+0x10/+0x20/+0x30 == mTransform's four rows
//   0x82619480  lvx128 v126, r11+0x790  == car+0x50 == mLinearVelocity
//   0x82619488  beq -> 0x82619520 : NOT in AI slow-mo -> `stb 0, 0x1B75` == SetWrittenIntoRWInSlowMo(false)
//   0x8261948C..0x82619518 (in slow-mo): `stb 1, 0x1B75` == SetWrittenIntoRWInSlowMo(true), then
//               `lfs flt_820049E0` (== 100.0f, the same AI-crash slow-motion factor
//               RaceCarPhysics::Update divides its timestep by) splatted into two VecFloats,
//               vrefp + two Newton-Raphson steps each (== the SDK operator/(Vector3,VecFloat)),
//               `vmulfp128 v127` and `vmulfp128 v126` -- BOTH velocities divided by 100.
//               (While the car integrates in slow motion its velocities are the SLOW ones; the
//               proxy is written at the same rate so the sim sees a consistent body.)
//   0x82619524..0x826197C0  the three tripwires, in this order: IsValid(mAngularVel) :4188,
//               IsValid(mVel) :4189, IsValid(mTransform) :4190 (all four rows, xyz lanes,
//               vcmpeqfp self-compare per lane == the NaN test).
//   0x826197C4..0x82619818  the event: rows -> +0x10..+0x40, v126 -> +0x50, v127 -> +0x60, and
//               mID = ((handlingBodyId>>32 & 0x00FFFFFF) | 0x0B000000) << 32 | (handlingBodyId & 0xFFFFFFFF)
//               -- the handling-body handle with its EntityId owner byte stamped to 11, i.e.
//               RigidBodyId::GetEntityId / EntityId::SetOwner(11) / RigidBodyId::SetEntityId,
//               which is the DWARF's callee list for the block at .cpp:4187 (lUpdateDummyRaceCarEvent).
//   0x8261981C  bl EventQueue<InUpdateExternalBody>::AddEvent -- ONE event per car.
//               (The DWARF's other local, lUpdateEvent @:4156, is the staging record the rows
//               are read into; the X360 emits exactly one AddEvent per car -- the dummy.)
//   0x82619820..0x826199E8  the inlined GetNextNonZeroBit with IsBitSet's "invalid index"
//               tripwire (CgsBitArray.h:203) -- unreachable, the bound is clamped to 8 first.
//   0x826199F0  addis/addi this+0x10000-0x5120 == +44768 == &mPhysicalTrafficManager ;
//               bl PhysicalTrafficManager::GetUpdatedVehicleBodies(r4 = the same queue) -- tail.
//
// PhysicalTrafficManager::GetUpdatedVehicleBodies -- register map (0x825EEF84..0x825EEFA0):
//   this+0x20000-0x6798 == +104552 == mUsedTrafficVehicles (BitArray<20>), same inlined walk,
//   bound 20 (0x825EEFF4 `cmpwi 0x14`).
//   0x825EF0C0  bl GetPhysicsEntityId(idx) -> r31 (owner 2, idx<<10) -- called BEFORE the
//               `liVehicle < ku8TotalMaxNumPhysicalTraffic` tripwire (h:730) at 0x825EF0C8.
//   0x825EF0E8  lwz this+0x20000-0x6B4C == +103604 == mpaTrafficVehicles ; slwi idx,6 (stride 64)
//   0x825EF100  lwz 0x1C(vehicle)  == PhysicalTrafficVehicle::mpVehicleBody (SimpleVehiclePhysics*)
//   0x825EF108  lvx128 v127 body+0x60 (mAngularVelocity), v126 body+0x50 (mLinearVelocity),
//               v125..v122 body+0x10.. (mTransform rows) -- the same ExternallySimulatedBody frame.
//   0x825EF124..0x825EF3B8  the three tripwires: :1844 mAngularVel, :1845 mVel, :1846 mTransform.
//   0x825EF3BC..0x825EF40C  mID = ((entity & 0x00FFFFFF) | 0x0C000000) << 32 | low32(entity<<32)
//               == the physics-traffic EntityId re-owned to 12 (PROP_COLLISION_TRAFFIC), index 0.
//   0x825EF410  bl AddEvent -- again ONE event per vehicle. No slow-mo arm on the traffic side.
//
// ---- LAYOUT DISCIPLINE (AGENTS.md gotcha 1) ------------------------------------------------
// Not one console byte offset is used below: mUsedRaceCars / maRaceCarVehicles /
// maRaceCarHandlingBodyIDs (through GetRigidBodyId) / mPhysicalTrafficManager /
// mUsedTrafficVehicles / mpaTrafficVehicles / mpVehicleBody are all reached BY NAME, and the
// ExternallySimulatedBody frame through its accessors.
//
// ---- NaN POLARITY (gotcha 4) -----------------------------------------------------------------
// The three tripwires are `vcmpeqfp.` self-compares reduced through CR6 -- a NaN lane compares
// unequal to itself, so a NaN FAILS the assert. rw::math::vpu::IsValid is exactly that test.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/World/BrnEntityTypes.h"                               // E_ENTITYTYPE_PROP_COLLISION_RACECAR / _TRAFFIC
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint (the opt-in [extbody] witness)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                   // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // InUpdateExternalBody
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                   // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"               // CgsSceneManager::EntityId::SetOwner
#include "rw/math/vpu/vector3_operation.h"                                 // rw::math::vpu::IsValid(Vector3) / Divide / Magnitude
#include "rw/math/vpu/matrix44affine_operation.h"                          // rw::math::vpu::IsValid(Matrix44Affine)
#include <stdlib.h>                                                        // getenv (host-only diag latch)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // flt_820049E0 -- the .rdata literal the slow-mo arm loads at 0x82619490 and splats
        // into both reciprocal chains. Hex-Rays folds it to 100.0 at both `stfs` sites; it is
        // the same AI-crash slow-motion factor RaceCarPhysics::Update divides its timestep by
        // (RaceCarPhysics.cpp KVF_AI_CRASH_SLOWMO_FACTOR, seated from unk_82FB8880). Spelled
        // locally because the two are different rodata homes on the console.
        const f32 KF_AI_CRASH_SLOWMO_FACTOR = 100.0f;

        // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_PROP_DIAG) one-shot witnesses so a
        // boot-drive can prove the proxies are being fed: the first race-car publish and the
        // first traffic publish, once each, then silence. The getenv latch is a function-local
        // static; the shipped path pays one predicted branch per call.
        // DELETE-WHEN the high-speed prop reaction is confirmed on screen.
        bool PropDiagEnabled()
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            return sbPropDiag && CgsDev::Log::gpDebugPrint != 0;
        }
    }

    // ========================================================================================
    // VehicleManager::GetUpdatedVehicleBodies   @0x82619340   (435 insns)
    // ========================================================================================
    void VehicleManager::GetUpdatedVehicleBodies(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>* lpUpdatedBodyQueue )
    {
        namespace vpu = rw::math::vpu;

        // The inlined BitArray<8> walk (GetFirstNonZeroBit / GetNextNonZeroBit). The console
        // additionally clamps the index to [0,8) before the body (0x826193B8/0x826193C4 and
        // the per-step `cmplwi 8` at 0x82619840); the committed walker never returns an index
        // outside the array, so the clamp is folded into the loop condition.
        for ( s32 i = mUsedRaceCars.GetFirstNonZeroBit();
              i >= 0 && i < 8;
              i = mUsedRaceCars.GetNextNonZeroBit( i ) )
        {
            RaceCarPhysics& lrRaceCar = maRaceCarVehicles[ i ];

            // DWARF :4156 lUpdateEvent -- the staging record. Rows/velocities first, exactly
            // the console's load order (angular, transform, linear), then the slow-mo arm.
            CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody lUpdateEvent;
            lUpdateEvent.mIDPad      = 0;   // host hygiene only: the console leaves this slot as stack garbage and nothing reads it
            lUpdateEvent.mTransform  = lrRaceCar.GetTransform();
            lUpdateEvent.mVel        = lrRaceCar.GetLinearVelocity();
            lUpdateEvent.mAngularVel = lrRaceCar.GetAngularVelocity();

            if ( lrRaceCar.IsInAICrashSlowMo() )
            {
                // 0x8261949C `stb 1, 0x1B75` then the two vrefp+2xNR reciprocal multiplies.
                lrRaceCar.SetWrittenIntoRWInSlowMo( true );
                lUpdateEvent.mAngularVel = vpu::Divide( lUpdateEvent.mAngularVel, KF_AI_CRASH_SLOWMO_FACTOR );
                lUpdateEvent.mVel        = vpu::Divide( lUpdateEvent.mVel,        KF_AI_CRASH_SLOWMO_FACTOR );
            }
            else
            {
                lrRaceCar.SetWrittenIntoRWInSlowMo( false );   // 0x82619520 `stb 0, 0x1B75`
            }

            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mAngularVel ),
                        "rw::math::IsValid( lUpdateEvent.mAngularVel )" );            // :4188 (0x105C)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mVel ),
                        "rw::math::IsValid( lUpdateEvent.mVel )" );                   // :4189 (0x105D)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mTransform ),
                        "rw::math::IsValid( lUpdateEvent.mTransform )" );             // :4190 (0x105E)

            // DWARF :4187/:4189 -- the dummy race car: the handling-body handle with its
            // EntityId owner re-stamped to the PROP_COLLISION_RACECAR proxy (0x826197C8..
            // 0x826197F8: `srdi 32 ; clrlwi 8 ; oris 0xB00 ; sldi 32 ; or low32`). The
            // low dword (the handle's index half) is carried through untouched.
            CgsPhysics::RigidBodyId lDummyRaceCarId = GetRigidBodyId( i );
            CgsSceneManager::EntityId lDummyRaceCarEntityId = lDummyRaceCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR ) );
            lDummyRaceCarId.SetEntityId( lDummyRaceCarEntityId );
            lUpdateEvent.mID = static_cast<u64>( lDummyRaceCarId );

            // [DIAG] NOT IN THE X360 BINARY -- see PropDiagEnabled.
            {
                static bool sbLoggedFirst = false;
                if ( !sbLoggedFirst && PropDiagEnabled() )
                {
                    sbLoggedFirst = true;
                    const Vector3 lPos = lUpdateEvent.mTransform.wAxis;
                    *CgsDev::Log::gpDebugPrint
                        << "[extbody] FIRST race-car proxy publish: car " << i
                        << " proxy entity " << CgsDev::E_PRINTMODE_HEXONCE
                        << static_cast<u32>( lDummyRaceCarEntityId )
                        << " pos (" << lPos.x << ", " << lPos.y << ", " << lPos.z << ")"
                        << " |v|=" << vpu::Magnitude( lUpdateEvent.mVel )
                        << " slowMo=" << ( lrRaceCar.IsInAICrashSlowMo() ? 1 : 0 )
                        << "\n";
                }
            }

            lpUpdatedBodyQueue->AddEvent( lUpdateEvent );                      // 0x8261981C
        }

        // 0x826199F0..0x826199FC -- the traffic half, same queue. On the console this is the
        // tail of the function whether or not any race car was live.
        mPhysicalTrafficManager.GetUpdatedVehicleBodies( lpUpdatedBodyQueue );
    }

    // ========================================================================================
    // PhysicalTrafficManager::GetUpdatedVehicleBodies   @0x825EEF70
    // ========================================================================================
    void PhysicalTrafficManager::GetUpdatedVehicleBodies(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>* lpUpdatedBodyQueue ) const
    {
        namespace vpu = rw::math::vpu;

        for ( s32 liUsedVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
              liUsedVehicle >= 0 && liUsedVehicle < static_cast<s32>( KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC );
              liUsedVehicle = mUsedTrafficVehicles.GetNextNonZeroBit( liUsedVehicle ) )
        {
            // DWARF :1824 lBodyID. 0x825EF0C0 -- called BEFORE the bound tripwire at 0x825EF0C8.
            const EntityId lBodyID = GetPhysicsEntityId( liUsedVehicle );

            CGS_ASSERT( liUsedVehicle < static_cast<s32>( KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC ),
                        "liVehicle < ku8TotalMaxNumPhysicalTraffic" );                 // h:730 (0x2DA)

            // DWARF :1834 / :1837. `lwz 0x1C(vehicle)` == mpVehicleBody -- the SimpleVehiclePhysics
            // base of whichever body (full or simple) this slot carries.
            const PhysicalTrafficVehicle* lpTrafficVehicle = GetTrafficVehicle( liUsedVehicle );
            const SimpleVehiclePhysics*   lpTrafficPhysics = lpTrafficVehicle->mpVehicleBody;

            CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody lUpdateEvent;
            lUpdateEvent.mIDPad      = 0;   // host hygiene only (see the race-car half)
            lUpdateEvent.mAngularVel = lpTrafficPhysics->GetAngularVelocity();   // lvx128 v127 +0x60
            lUpdateEvent.mVel        = lpTrafficPhysics->GetLinearVelocity();    // lvx128 v126 +0x50
            lUpdateEvent.mTransform  = lpTrafficPhysics->GetTransform();         // v125..v122 +0x10..

            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mAngularVel ),
                        "RwMath::IsValid( lUpdateEvent.mAngularVel )" );              // :1844 (0x734)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mVel ),
                        "RwMath::IsValid( lUpdateEvent.mVel )" );                     // :1845 (0x735)
            CGS_ASSERT( vpu::IsValid( lUpdateEvent.mTransform ),
                        "RwMath::IsValid( lUpdateEvent.mTransform )" );               // :1846 (0x736)

            // DWARF :1852/:1854 -- the dummy traffic car: RigidBodyId::Set(lBodyID, 0) then
            // the owner re-stamped to PROP_COLLISION_TRAFFIC (0x825EF3C4..0x825EF3EC:
            // `srdi 32 ; clrlwi 8 ; oris 0xC00 ; sldi 32 ; or low32`, low32 == 0 here).
            CgsPhysics::RigidBodyId lDummyCarId( static_cast<u64>( lBodyID.muValue ) << 32 );
            CgsSceneManager::EntityId lDummyRaceCarEntityId = lDummyCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );
            lDummyCarId.SetEntityId( lDummyRaceCarEntityId );
            lUpdateEvent.mID = static_cast<u64>( lDummyCarId );

            // [DIAG] NOT IN THE X360 BINARY -- see PropDiagEnabled.
            {
                static bool sbLoggedFirst = false;
                if ( !sbLoggedFirst && PropDiagEnabled() )
                {
                    sbLoggedFirst = true;
                    const Vector3 lPos = lUpdateEvent.mTransform.wAxis;
                    *CgsDev::Log::gpDebugPrint
                        << "[extbody] FIRST traffic proxy publish: slot " << liUsedVehicle
                        << " proxy entity " << CgsDev::E_PRINTMODE_HEXONCE
                        << static_cast<u32>( lDummyRaceCarEntityId )
                        << " pos (" << lPos.x << ", " << lPos.y << ", " << lPos.z << ")"
                        << " |v|=" << vpu::Magnitude( lUpdateEvent.mVel )
                        << "\n";
                }
            }

            lpUpdatedBodyQueue->AddEvent( lUpdateEvent );                      // 0x825EF410
        }
    }
}
}
