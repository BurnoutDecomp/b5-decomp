// ============================================================================
// BrnVehicleManager_PrepareData.cpp
//
//   BrnPhysics::Vehicle::VehicleManager::PrepareData @0x82633568 (161)
//
// VehicleManager::Prepare's stage-0/1 arm, and THE ONLY CALLER of
// PhysicalTrafficManager::Prepare @0x8262CA48 -- which is the only seater of the three
// traffic pools. It retires the WorldLinkStubs.cpp:543 `return true` gate.
//
// EVERY OFFSET IS REACHED BY NAME. The X360 pseudocode for this function degenerates into
// `_R28`/`_R31` inline asm with every store at a raw console byte offset, which is why the old
// link-stub banner refused to write it from the pseudocode; the body below is derived from the
// raw asm, with each store mapped onto the member BrnVehicleManager.h already documents at that
// console offset.
//
// SIGNATURE: DWARF BrnVehicleManager.h:824 spells `bool PrepareData(rw::LinearResourceAllocator*)`.
// This tree's declaration (BrnVehicleManager.h:485) and the retiring link stub both carry the
// BASE `rw::IResourceAllocator*`; that spelling is kept, because changing it changes the MSVC
// mangle and turns a compile error into LNK2019/LNK2005.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // K_INVALID_ENTITY_ID  (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"       // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // GATE: BrnPhysics::Vehicle::DebugComponent::Construct(&maRaceCarDebugComponent[i],
    // &maRaceCarVehicles[i]) @0x826336A0. BLOCKER: maRaceCarDebugComponent is an OPAQUE
    // 8x1024-byte span (the component models 112 of its 1024 bytes) and DebugComponent has no
    // Construct on its reconstructed surface -- constructing into it would write unmodelled
    // memory. VehicleManager::Construct already declines the same object for the same reason.
    // DELETE-WHEN the vehicle DebugComponent's own reconstruction pass lands.
    void LogDebugComponentGate()
    {
        static bool sbLogged = false;
        if (sbLogged)
        {
            return;
        }
        sbLogged = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T3-gate] VehicleManager::PrepareData: per-car DebugComponent::Construct"
                   " @0x826336A0 skipped (opaque 1024B span) [FLAG PC partial gate]\n";
        }
    }
}

// @0x82633568. Returns the constant 1; there is no failure path.
bool VehicleManager::PrepareData(rw::IResourceAllocator* lpPhysicsAllocator)
{
    // 0x826335A4..0x826335BC -- four `std 0`, in asm order.
    mUsedRaceCars.UnSetAll();                             // +44224
    mRaceCarsAddedForCollision.UnSetAll();                // +44712
    mNetworkCarsAddedForCollisionThisFrame.UnSetAll();    // +44720
    mHiddenRaceCars.UnSetAll();                           // +44704

    mDebugComponent.Register();                           // 0x826335C4 (this + 161968)

    // ---- FIRST eight-car loop (0x826335F4..0x82633638) -------------------------------------
    // ⚠️ THE SAME EIGHT OBJECTS AS THE SECOND LOOP: r28 is a COPY of r31 taken at 0x826335E0 and
    // r31 is not advanced by this loop, so both walk maRaceCarVehicles[0..7] at the 5216 stride.
    // The console really does run the per-car Construct + seeds twice; reproduced as issued.
    // `bl VehiclePhysics::Construct` plus the six in-record seeds (+0x1070 z-lane, +0x13F0 16B,
    // +0x1400, +0x1408, +0x140C, +0x140D) IS RaceCarPhysics::Construct -- one call, not a base
    // call plus pokes; same folding VehicleManager::Construct's loop uses.
    for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
    {
        maRaceCarVehicles[liCar].Construct();                                     // +1856, stride 5216
        maRaceCarEntityIDs[liCar].muValue = CgsSceneManager::K_INVALID_ENTITY_ID; // +43584, stride 4
    }

    // ---- SECOND eight-car loop (0x82633660..0x826336D4) ------------------------------------
    for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
    {
        maRaceCarDrivers[liCar].Prepare();                                        // +64, stride 224
        maRaceCarVehicles[liCar].Construct();

        LogDebugComponentGate();   // stands in for DebugComponent::Construct @0x826336A0

        mabRaceCarDebugComponentRegistered[liCar] = false;                        // +171456 + i
        maRaceCarEntityIDs[liCar].muValue = CgsSceneManager::K_INVALID_ENTITY_ID; // +43584 + 4i
        maRaceCarHandlingBodyIDs[liCar]   = CgsPhysics::K_INVALID_RIGID_BODY_ID;  // +43744 + 8i
    }

    // 0x826336E4 -- the whole point of this function.
    mPhysicalTrafficManager.Prepare(lpPhysicsAllocator);   // this + 44768

    mPlayerAiDriver.Prepare();                             // 0x826336F0 (this + 171968)

    mbPlayerAiDriverValid    = false;                      // +172192  stbx 0
    mfPlayerRecentSteering   = 0.0f;                       // +172196  stfsx 0.0f
    mfSteeringUpdateRemainder = 0.0f;                      // +172200  stfsx 0.0f

    // 0x82633730 `stwx r30, r29, 0x273A8` == this + 160680 == mDiscardedContacts' miLength.
    mDiscardedContacts.Clear();

    // ---- the eight-slot per-car tuning bank (0x82633744..0x82633784) ------------------------
    // One loop, two cursors: r11 walks the f32 arrays at stride 4, r10 the u8 arrays at stride 1.
    // mafVulnerabilityFactor (+171776, `stfs flt_82001C98`) is the ONLY non-zero seat; the two
    // 0x80 bytes are the "no grind recorded" sentinel.
    for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
    {
        maeImpactType[liCar]                        = E_IMPACT_NONE;   // +171644  stw 0
        mafNoImpactTimeSeconds[liCar]               = 0.0f;            // +171684
        maiPhysicsSlamIndex[liCar]                  = 0;               // +171716  stb 0
        mafVulnerableTimeSeconds[liCar]             = 0.0f;            // +171744
        mafVulnerabilityFactor[liCar]               = 1.0f;            // +171776
        mafTotalVulnerableTime[liCar]               = 0.0f;            // +171808
        mafPlayerGrindingOtherDurationSeconds[liCar] = 0.0f;           // +171840
        mafOtherGrindingPlayerDurationSeconds[liCar] = 0.0f;           // +171872
        mafRubbingDurationSeconds[liCar]            = 0.0f;            // +171904
        mau8FramesSincePlayerGrindingOther[liCar]   = 0x80;            // +171936  stb 0x80
        mau8FramesSinceOtherGrindingPlayer[liCar]   = 0x80;            // +171944  stb 0x80
        mabRubbingThisUpdate[liCar]                 = false;           // +171952  stb 0
    }

    mPlayerWonImpact.UnSetAll();          // 0x826337B8 (this + 171736, `std 0`)
    mfContactDisplaySeconds = 0.0f;       // 0x826337C0 (this + 171724)

    // The four bools VehicleManager::Construct leaves alone -- THIS is where they are seeded
    // (0x826337C8..0x826337D4). The header's "(not seeded)" notes were written against Construct
    // alone; correct them there when that header's owner next touches it.
    DEBUG_mbAlwaysCrashRaceCarToRaceCar = false;   // +172310
    DEBUG_mbHornTakedownEnabled         = false;   // +172311
    mbDebugModifyTrafficContacts        = true;    // +172312  `li r6, 1`
    mbForceNoSlowMo                     = false;   // +172317

    return true;   // 0x826337C4 `li r3, 1`
}

}
}
