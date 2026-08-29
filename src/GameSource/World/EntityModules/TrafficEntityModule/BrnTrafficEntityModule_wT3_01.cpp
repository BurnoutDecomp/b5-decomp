// ============================================================================
// BrnTrafficEntityModule_wT3_01.cpp -- cluster C1: WORLD-SIDE PROMOTION.
// The five-step chain that turns a driving traffic car into a physics body request.
//
//   TrafficEntityModule::SendPhysicalRequests           @0x8274C510 (96)   DWARF :1569
//   TrafficEntityModule::SafeRequestMakeVehiclePhysical @0x8274AFD0 (234)  DWARF :1572
//   TrafficEntityModule::MakeVehiclePhysical            @0x82747200 (162)  DWARF :1575
//   TrafficEntityModule::AddVehicleToPhysics            @0x827425B0 (462)  DWARF :1405
//   TrafficEntityModule::RecordTrafficVehicleIsPhysical @0x82720EC0 (188)  DWARF :1170
//
// None of the five is in the Feb-2007 leak; ARTIST pseudocode + asm are the only sources.
// Every X360 displacement quoted below is reached BY NAME, never by offset.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"      // MakeTrafficEntityId
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h" // KU_MAX_JOBS
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                       // KU_ENTITYTYPE_TRAFFIC_VEHICLE
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace BrnTraffic
{
namespace
{
    // SafeRequestMakeVehiclePhysical's sympathetic-crash seed, 0x8274B340..0x8274B348.
    // The modulus is the 0x446F8657 magic-multiply reciprocal's divisor and the split is the
    // literal `cmpwi r11, 0x32`. Deliberately NOT shared with the identically-valued pair in
    // _wT3_02.cpp: that seat's split is a computed percentage (slider*35 + 30) and this one is a
    // hard 50, so they are two different constants that happen to share a modulus.
    const u32 KU_SYMP_CRASH_PERCENT_MODULUS   = 101u;
    const s32 KI_SYMP_CRASH_ACCELERATE_PERCENT = 50;
}

// HOST SEAT REACHED ACROSS A TU BOUNDARY. The console walks maJobs[0..
// muNumUpdateVehiclesJobs) (DWARF :619) and calls TrafficJobStub::GetNewPhysicalRequests on
// each. That member is [MEMBER HOLE 5] on this tree (BrnTrafficJob.h cannot be included from
// BrnTrafficEntityModule.h -- EAThread C2011), and its host stand-in lives in
// BrnTrafficEntityModule_wT2_04.cpp, which seats it in an ANONYMOUS namespace. The producer
// (UpdateVehicles' job split) writes that array and this consumer reads it, so the two must be
// the same object: declared extern here rather than forked into a second array.
// LINK BLOCKER for the conductor, one line in a file this cluster does not own: move
// gaHostUpdateVehiclesJobs / gaHostNewPhysicalRequests out of _wT2_04.cpp's anonymous
// namespace (into namespace BrnTraffic) so this declaration resolves. See REPORT section 2.
extern PhysicalRequestInfoList gaHostNewPhysicalRequests[KU_MAX_JOBS];

// ----------------------------------------------------------------------------
// TrafficEntityModule::SendPhysicalRequests  @ 0x8274C510 (96)   DWARF :1569
//
// PrePhysicsUpdate's RUNNING arm. Drains each UpdateVehicles job's PhysicalRequestInfoList
// into SafeRequestMakeVehiclePhysical, then clears the list.
//
// Register split from the prologue (0x8274C51C..0x8274C524): r3 this, r4 lpOutput,
// r5 lpCreatedBodies. Hex-Rays LOSES r5 and renders the seventh argument of the inner call as
// `*v7`; the asm is unambiguous -- 0x8274C618 `mr r10, r24` with r24 == r5.
//
// The inner call's argument build is 0x8274C608..0x8274C644:
//   r4 lhz   var_70+0 == PhysicalRequestInfo::muVehicle (u16, zero-extended)
//   r5 extsb var_70+2 == PhysicalRequestInfo::miReason  (s8,  SIGN-extended)
//   r6 lwz   var_70+4 == PhysicalRequestInfo::mTargetEntityId
//   r7 = 2 (E_TRAFFIC_TYPE_PHYSICAL)   r8 = 2 (eCrashTrafficType_Spontaneous)
//   r9 = lpOutput                      r10 = lpCreatedBodies
// ----------------------------------------------------------------------------
void TrafficEntityModule::SendPhysicalRequests(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                              TotalTrafficBitArray* lpMadePhysical)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput");   // 0x8274C530, BrnTrafficEntityModule.cpp:2369

    // 0x8274C550 `lwz 0x2A00` == muNumUpdateVehiclesJobs; the entry guard is signed (`ble`),
    // the loop-back unsigned (`cmplw`). No clamp against the host array extent, matching the
    // producer side in _wT2_04.cpp (muNumUpdateVehiclesJobs is only ever set to KU_MAX_JOBS).
    for (u32 luJob = 0; luJob < muNumUpdateVehiclesJobs; ++luJob)
    {
        // Console: maJobs[luJob].GetNewPhysicalRequests(), with THREE `!mbRunningJob` asserts
        // (BrnTrafficJob.h:97) -- one before the size read, one before each element read, one
        // before the clear. The host split runs the worker inline inside TrafficJobStub::
        // Execute, so no job is ever running here and TrafficJobStub is not a module member;
        // the asserts have no host counterpart.
        PhysicalRequestInfoList& lrRequests = gaHostNewPhysicalRequests[luJob];

        // GetLength() re-evaluated per iteration, exactly as the console re-loads the count
        // word (0x8274C5B4 / 0x8274C5D8) and re-fires the CgsArray.h:336 assert.
        for (u32 luRequest = 0; luRequest < lrRequests.GetLength(); ++luRequest)
        {
            const PhysicalRequestInfo& lrInfo = lrRequests[luRequest];

            SafeRequestMakeVehiclePhysical(lrInfo.muVehicle,
                                           static_cast<PhysicalReason>(lrInfo.miReason),
                                           lrInfo.mTargetEntityId,
                                           BrnPhysics::Vehicle::E_TRAFFIC_TYPE_PHYSICAL,
                                           BrnPhysics::Vehicle::eCrashTrafficType_Spontaneous,
                                           lpOutput,
                                           lpMadePhysical);
        }

        lrRequests.Clear();   // 0x8274C670 `stw r20, 0x548(r29)` == the count word
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::SafeRequestMakeVehiclePhysical  @ 0x8274AFD0 (234)  DWARF :1572
//
// Every reason a promotion request is refused, in console order. Hex-Rays prints "local
// variable allocation has failed" on this one, so the whole body is read off the asm.
// Prologue 0x8274AFDC..0x8274AFFC: r3 this, r4 luVehicle, r5 leReason, r6 lTargetEntityId,
// r7 leTrafficType, r8 leCrashType, r9 lpOutput, r10 lpCreatedBodies.
// ----------------------------------------------------------------------------
void TrafficEntityModule::SafeRequestMakeVehiclePhysical(
        u32 luVehicle,
        PhysicalReason leReason,
        EntityId lTargetEntityId,
        BrnPhysics::Vehicle::ETrafficType leTrafficType,
        BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        TotalTrafficBitArray* lpMadePhysical)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput");                 // 0x8274B00C, .cpp:2434
    CGS_ASSERT(lpMadePhysical != 0, "lpCreatedBodies");    // 0x8274B030, .cpp:2435

    // 0x8274B04C `lbzx r11, r27, 0x725E8` -- mbTrafficIsHidden (DWARF :789; its neighbour
    // mfJunctionFUP :785 is the header's attested +0x725E0). Hidden traffic never promotes.
    if (mbTrafficIsHidden)
    {
        return;
    }

    // 0x8274B060..0x8274B144 -- the inlined BitArray<601>::IsBitSet with its streamed bounds
    // message ("invalid index : " << luVehicle << " < " << 600).
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "invalid index : ");   // CgsBitArray.h:203
    if (lpMadePhysical->IsBitSet(luVehicle))
    {
        return;
    }

    Vehicle* lpVehicle = GetVehicle(luVehicle);

    // 0x8274B158..0x8274B17C: one `lbz 5(vehicle)` (mxFlags) tested three ways -- bit0 ALIVE
    // and bit1 HASENTITY must be set, bit3 PHYSICAL must be clear.
    if (!lpVehicle->IsAlive() || !lpVehicle->HasEntity() || lpVehicle->IsPhysical())
    {
        return;
    }

    // 0x8274B184 `cmpwi r3, -1`. GetPhysicalReason is SIGN-extended at its source
    // (0x82705540 lbz + extsb), so == -1 IS correct here. Never copy this idiom to
    // GetPhysicalPartsIndex / GetCurrentManoeuvrePhase, which are zero-extended.
    if (lpVehicle->GetPhysicalReason() != E_PHYSICALREASON_INVALID)
    {
        return;
    }

    // 0x8274B198..0x8274B204 -- the free-slot budget: the inlined BitArray<25>
    // GetFirstZeroBit over maTrafficPhysicsInfoListBits must land in [0, 25).
    // GATE trailers @0x8274B208: when Vehicle::muOtherHalfIndex != KU_INVALID_VEHICLE the
    // console takes the OTHER budget -- CountSetBits() + 2 <= 25, two slots for the pair.
    // BLOCKER: muOtherHalfIndex is private with no unconditional accessor and
    // BrnTrafficVehicle.h is not this cluster's file. Unreachable today (generation is
    // InitialiseAsStandard only, which seeds it to KU_INVALID_VEHICLE).
    // DELETE-WHEN Vehicle gains the DWARF :338 GetTrailerIndex / other-half accessor.
    const s32 liFreeSlot = maTrafficPhysicsInfoListBits.GetFirstClearBit();
    if (liFreeSlot < 0 || liFreeSlot >= static_cast<s32>(KU_MAX_PHYSICAL_TRAFFIC_VEHICLES))
    {
        return;
    }

    MakeVehiclePhysical(luVehicle, lpOutput, lpMadePhysical, lTargetEntityId,
                        leTrafficType, leCrashType);

    lpVehicle->SetPhysicalReason(static_cast<s8>(leReason));   // 0x8274B2DC

    if (leReason == E_PHYSICALREASON_SYMPATHETIC_CRASHING)     // 0x8274B2E0 cmpwi r25, 3
    {
        lpVehicle->SetSympatheticCrashTarget(lTargetEntityId); // 0x8274B2F0

        // GATE DELETED 2026-08-29 (traffic-crash wave). Its blocker -- "Vehicle exposes no
        // setter for either member" -- was STALE: BrnTrafficVehicle.h has carried
        // SetSympCrashTime / SetSympCrashState since the Vehicle wave, and the sibling seed in
        // UpdateExtremeSwerving (_wT3_02.cpp, @0x8273EA84) has been calling both all along.
        // ⛔ CORRECTED 2026-08-29 (traffic-crash wave 2). This note used to say "meSympCrashState
        // is the ONLY field Vehicle::IsSympatheticallyCrashing() reads". IT IS NOT: that
        // predicate @0x82704B18 reads miPhysicalReason (+0x39) == 3, exactly like its twins
        // IsExtremeSwerving (== 4) and IsNormalPhysical (== 5) -- read out of the image, see the
        // banner on Vehicle::IsSympatheticallyCrashing in BrnTrafficVehicle.cpp.
        // ⭐ WHAT THE GATE ACTUALLY COST: the seed below is the ONLY thing that gives a car
        // promoted for reason 3 a VALID meSympCrashState. With it skipped the car still entered
        // UpdateSympatheticCrashing (reason 3 alone is the ticket), switched on
        // E_SYMPATHETIC_NONE(0), and fell into the console's `default:` arm -- "Invalid
        // sympathetic crashing state." every frame, with no steering and no commit. The arm was
        // reachable and useless, not unreachable.
        //
        // 0x8274B2F4..0x8274B36C, instruction for instruction: ONE mEffectRand LCG step
        // (`ld/std 0x1380(this)` -- +0x1380 is mEffectRand's seed, NOT mRand's +0x1350; the old
        // note named the wrong generator), the draw reduced mod 101 by the 0x446F8657
        // magic-multiply reciprocal, `stfs flt_82001CC0(0.0f), 0x4C(veh)` unconditionally, then
        // `< 50` picks ACCELERATE(2) and `>= 50` stores r30, which `li r30, 1` at 0x8274B114
        // pins as HEADON(1).
        const s32 liRoll =
            static_cast<s32>(mEffectRand.RandomUInt() % KU_SYMP_CRASH_PERCENT_MODULUS);
        lpVehicle->SetSympCrashTime(0.0f);
        lpVehicle->SetSympCrashState(liRoll < KI_SYMP_CRASH_ACCELERATE_PERCENT
                                         ? Vehicle::E_SYMPATHETIC_ACCELERATE
                                         : Vehicle::E_SYMPATHETIC_HEADON);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::MakeVehiclePhysical  @ 0x82747200 (162)   DWARF :1575
//
// Six baked asserts, then the two halves of "this car is now a physics body": post the spawn
// event, and record the world-side physics slot.
// Prologue 0x82747210..0x8274722C: r3 this, r4 luVehicle, r5 lpOutput, r6 lpCreatedBodies,
// r7 lTargetEntityId, r8 leTrafficType, r9 leCrashType.
// ----------------------------------------------------------------------------
void TrafficEntityModule::MakeVehiclePhysical(
        u32 luVehicle,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        TotalTrafficBitArray* lpMadePhysical,
        EntityId lTargetEntityId,
        BrnPhysics::Vehicle::ETrafficType leTrafficType,
        BrnPhysics::Vehicle::eCrashTrafficType leCrashType)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput");                 // 0x8274723C, .cpp:2522
    CGS_ASSERT(lpMadePhysical != 0, "lpCreatedBodies");    // 0x82747260, .cpp:2523

    // 0x827472AC: MakeTrafficEntityId inlined (its CgsEntityId.h:116 bound assert is baked).
    CGS_ASSERT(MakeTrafficEntityId(luVehicle).muValue != lTargetEntityId.muValue,
               "MakeTrafficEntityId( luVehicle ) != lCauserEntityId");   // .cpp:2524

    // 0x82747308 / 0x8274735C: the SAME mxFlags byte read twice, once per assert, each with
    // its own inlined GetVehicle bound assert (BrnTrafficEntityModule.h:2459).
    CGS_ASSERT(GetVehicle(luVehicle)->IsAlive(),
               "GetVehicle( luVehicle )->IsAlive()");                    // .cpp:2525
    CGS_ASSERT(!GetVehicle(luVehicle)->IsPhysical(),
               "!GetVehicle( luVehicle )->IsPhysical()");                // .cpp:2526

    // 0x8274738C: the export symbol `OutputBuffer_PrePhysics::G...` is truncated; it is
    // GetVehicleInputInterface (BrnTrafficEntityModuleIO.h:795).
    AddVehicleToPhysics(luVehicle, lTargetEntityId, lpOutput->GetVehicleInputInterface(),
                        leTrafficType, lpMadePhysical);

    // 0x827473AC re-fires the MakeTrafficEntityId bound assert: the id is built a second time.
    // f1/f2 are both flt_82001CC0 == 0.0f.
    RecordTrafficVehicleIsPhysical(luVehicle, MakeTrafficEntityId(luVehicle), lTargetEntityId,
                                   leCrashType, 0.0f, 0.0f);

    // GATE trailer partner @0x82747414..0x82747478: when Vehicle::muOtherHalfIndex !=
    // KU_INVALID_VEHICLE the console records the PARTNER too -- GetCabIndex() for a trailer,
    // GetTrailerIndex() otherwise, then RecordTrafficVehicleIsPhysical on that index.
    // BLOCKER: muOtherHalfIndex / GetTrailerIndex (DWARF :338) are not on Vehicle's host
    // surface and BrnTrafficVehicle.h is not this cluster's file. Unreachable today.
    // DELETE-WHEN the trailer wave lands.
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::AddVehicleToPhysics  @ 0x827425B0 (462)   DWARF :1405
//
// THE world -> physics hop. Builds the spawn event's payload and posts it on the
// VehicleInputInterface, then marks the vehicle in lpCreatedBodies.
// Prologue 0x827425BC..0x827425D4: r3 this, r4 luVehicle, r5 lTargetEntityId,
// r6 lpVehicleInputInterface, r7 leTrafficType, r8 lpCreatedBodies.
//
// The console displacements, all reached BY NAME below:
//   &v8[32*idx + 2720]               == &maVehicles[luVehicle]  (stride 128, base +10880)
//   v8 + 116240                      == mpData                  (X360 +0x71840)
//   TrafficData +0x2C / +0x34        == mpaVehicleTypes / mpaVehicleAssets
//   v8 + ((32*type)&0x1FE0) + 482156 == maTrafficVehiclePhysicsSpecs[type] + 0x14, the
//        {mpThis, muThreadId} pair BaseResourcePtr::CreateFromHandle seats the SOURCE handle
//        in -- i.e. GetResourceHandle(). Same seat _Render.cpp:681 already documents.
// ----------------------------------------------------------------------------
void TrafficEntityModule::AddVehicleToPhysics(
        u32 luVehicle,
        EntityId lTargetEntityId,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput,
        BrnPhysics::Vehicle::ETrafficType leTrafficType,
        TotalTrafficBitArray* lpMadePhysical)
{
    CGS_ASSERT(lpVehicleInput != 0, "lpVehicleInputInterface");   // 0x827425E4, .cpp:5632
    CGS_ASSERT(lpMadePhysical != 0, "lpCreatedBodies");           // 0x82742608, .cpp:5633

    const Vehicle* lpVehicle = GetVehicle(luVehicle);   // 0x8274262C bound assert .h:2459

    // GATE articulated arm @0x827428C4: when Vehicle::muOtherHalfIndex != KU_INVALID_VEHICLE
    // the console builds BOTH halves and posts VehicleInputInterface::CreateArticulatedTraffic
    // (31 arguments) instead, logging "CreateArticulatedTrafficEventQueue is full" when that
    // queue is full. BLOCKER: muOtherHalfIndex has no unconditional accessor
    // (BrnTrafficVehicle.h is another cluster's file) and CreateArticulatedTraffic is itself a
    // keystone gate. Unreachable today. DELETE-WHEN the trailer wave lands.

    // GATE queue-full check @0x82742674..0x8274269C: the console compares
    // mCreateTrafficEventQueue's length against its capacity and, when full, logs
    // "CreateTrafficEventQueue is full\n" to gpDebugPrint and posts NOTHING.
    // BLOCKER: that queue is private and VehicleInputInterface has no GetCreateTrafficEvents()
    // accessor yet (C2's drain needs the same one). It cannot fire today -- the free-slot
    // guard in SafeRequestMakeVehiclePhysical caps promotions at the 25 slots this 25-event
    // queue holds and the physics drain empties it every frame; BaseEventQueue::AddEvent's own
    // "Reached Max length" assert is the residual tripwire.
    // DELETE-WHEN VehicleInputInterface exposes the queue.

    // 0x827426BC / 0x827426DC. The transform is passed by value (large aggregate -> by
    // reference on PPC); the four outs are the stack locals var_190/var_180/var_1D0/var_140.
    Vector3        lInitialVelocity;
    Vector3        lAngularVelocity;
    u8             lu8AttribsId = 0;
    Matrix44Affine lInitialTransform;
    CalculateInitialPhysicalState(lpVehicle, GetVehicleTransform(luVehicle),
                                  lInitialVelocity, lAngularVelocity,
                                  &lu8AttribsId, lInitialTransform);

    // 0x827426EC..0x82742728 -- two mpData->operator-> hops: the vehicle TYPE record names an
    // asset id (VehicleTypeData +5), and the asset record holds the CgsID the event carries.
    const u32   luVehicleType = lpVehicle->GetVehicleType();
    const u8    lu8AssetId    = mpData->mpaVehicleTypes[luVehicleType].muAssetId;
    const CgsID lCgsID        = mpData->mpaVehicleAssets[lu8AssetId].GetVehicleId();

    // 0x8274272C..0x82742744 -- the console recomputes Vehicle::IsCab() here
    // (muOtherHalfIndex != KU_INVALID_VEHICLE && !IsOfTrailerSpecies()). On this arm the first
    // term is false by construction (the arm is entered only when muOtherHalfIndex ==
    // KU_INVALID_VEHICLE), so the value is constant-false. Same trailer gate as above.
    const bool lbIsCab = false;

    // 0x8274274C..0x82742778 -- a `std` of the folded 0x0200000000000000 (owner byte 2) then
    // the out-of-line index splice, spelled as the two field writes the compiler folded (the
    // idiom BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp:78 already uses).
    CgsSceneManager::VolumeInstanceId lVolumeInstanceId;
    lVolumeInstanceId.muId = 0;
    lVolumeInstanceId.SetEntityIDOwner(
        static_cast<u8>(BrnPhysics::Vehicle::KU_ENTITYTYPE_TRAFFIC_VEHICLE));
    lVolumeInstanceId.SetEntityIDEntityIndex(luVehicle);

    // 0x82742774 `ld 0x14(...)` -- the deformation-spec handle the physics side spawns from.
    // FLAG (extent, same as _Render.cpp:694): the DWARF declares this array
    // KU_MAX_VEHICLE_ASSETS long but the console indexes it by vehicle TYPE. Bounded here so a
    // type past the extent hands over a null handle rather than reading off the end.
    CgsResource::ResourceHandle lModelHandle;
    lModelHandle.Clear();
    if (luVehicleType < KU_MAX_VEHICLE_ASSETS)
    {
        lModelHandle = maTrafficVehiclePhysicsSpecs[luVehicleType].GetResourceHandle();
    }

    // 0x82742788 / 0x827427C0. r7 = the attrib key, r8 = the model handle, r9 = the traffic
    // type, r10 = lbIsCab, one 8-byte stack slot = lCgsID; v1/v2 carry the two Vector3s.
    lpVehicleInput->CreatePhysicalTraffic(lVolumeInstanceId,
                                          lTargetEntityId,
                                          lInitialTransform,
                                          lInitialVelocity,
                                          lAngularVelocity,
                                          GetCarAssetAttribKey(luVehicle),
                                          lModelHandle,
                                          leTrafficType,
                                          lbIsCab,
                                          lCgsID);

    // 0x827427C4..0x82742888 -- the inlined BitArray<601>::SetBit with its streamed bounds
    // message ("Index: " << luVehicle << ", Number of bits: " << 600).
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "Index: ");   // CgsBitArray.h:222
    lpMadePhysical->SetBit(luVehicle);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::RecordTrafficVehicleIsPhysical  @ 0x82720EC0 (188)  DWARF :1170
//
// The world-side half: claim a maTrafficPhysicsInfoList slot, Construct the record, and flip
// the vehicle's physical flags.
// Prologue 0x82720ED4..0x82720EEC: r3 this, r4 luVehicle, r5 lVictimId, r6 lCauserId,
// r7 leCrashType, f1/f2 the two direction seeds (both 0.0f from MakeVehiclePhysical).
//
// ⚠️ THE TWO FLOAT SEATS ARE NOT THE STUCK TIMERS. The stores land at record +4060 / +4064
// (`stfsx f31, r31, 0x591EC` / `stfsx f30, r31, 0x591F0`, array base +360976), and the
// console layout puts mfStuckTimeFront/Back at +4044/+4048 -- pinned independently by
// UpdateVehicleStuckTimers @0x82708D90/@0x82708DA8 (`addi r8, r31, 0xFCC` / `0xFD0`, with
// muContactSideFlags read at `0x1008`). Walking the DWARF tail down from
// muOwningVehicleIndex @+0x100A gives +4060 = mfSteeringDirection (:190) and
// +4064 = mfDrivingDirection (:191), which is what a freshly promoted car zeroes.
// ----------------------------------------------------------------------------
void TrafficEntityModule::RecordTrafficVehicleIsPhysical(
        u32 luVehicle,
        EntityId lEntityId,
        EntityId lTargetEntityId,
        BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
        f32 lfSteeringDirection,
        f32 lfDrivingDirection)
{
    Vehicle* lpVehicle = GetVehicle(luVehicle);   // 0x82720EF8 bound assert .h:2459

    CGS_ASSERT(lEntityId.muValue != lTargetEntityId.muValue,
               "lVictimId != lCauserId");                                // .cpp:6988

    // 0x82720F50..0x82720F6C: four explicit equality tests, not a range check.
    CGS_ASSERT(leCrashType == BrnPhysics::Vehicle::eCrashTrafficType_Checked ||
               leCrashType == BrnPhysics::Vehicle::eCrashTrafficType_Standard ||
               leCrashType == BrnPhysics::Vehicle::eCrashTrafficType_Spontaneous ||
               leCrashType == BrnPhysics::Vehicle::eCrashTrafficType_Slammed,
               "leCrashingTrafficType == BrnPhysics::Vehicle::eCrashTrafficType_Checked || ...");
                                                                         // .cpp:6992

    // 0x82720F8C..0x82720FDC. mbNeedsToBeSentToCrashModule is
    // `cntlzw(leCrashType - 3) bit 5, inverted` == (leCrashType != Slammed).
    if (!lpVehicle->IsPhysical() || lpVehicle->IsRecoveringFromSlam())
    {
        TrafficCrashInfo lCrashInfo;
        lCrashInfo.mVictimId            = lEntityId;
        lCrashInfo.mCauserId            = lTargetEntityId;
        lCrashInfo.muCrashTrafficType   = static_cast<u32>(leCrashType);
        lCrashInfo.mbNeedsToBeSentToCrashModule =
            (leCrashType != BrnPhysics::Vehicle::eCrashTrafficType_Slammed);
        // TRIPWIRE, not a gate: both console drains of this 160-slot array
        // (GenerateCrashedVehicleEvents @0x82720030, GenerateVehicleCrashedEvents @0x82727768)
        // are still gated, so only Reset clears it. Bounded below 160 -- the
        // free-slot guard caps live promotions at 25 and demotion/recycling has not landed.
        maNewCrashedVehicles.Append(lCrashInfo);
    }

    // 0x82720FE0: the SAME flag re-read. An already-physical vehicle claims no second slot.
    if (!lpVehicle->IsPhysical())
    {
        // 0x82720FF0..0x82721050 -- inlined BitArray<25>::GetFirstZeroBit then `extsb`: an
        // index of 25 or more (or a full array) becomes -1, which the next assert catches.
        s32 liPartsIndex = maTrafficPhysicsInfoListBits.GetFirstClearBit();
        if (liPartsIndex >= static_cast<s32>(KU_MAX_PHYSICAL_TRAFFIC_VEHICLES))
        {
            liPartsIndex = -1;
        }

        CGS_ASSERT(liPartsIndex >= 0, "liPartsIndex >= 0");                    // .cpp:7025
        CGS_ASSERT(static_cast<u32>(liPartsIndex) < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES,
                   "Index: ");                                                 // CgsBitArray.h:222

        maTrafficPhysicsInfoListBits.SetBit(static_cast<u32>(liPartsIndex));   // 0x8272115C

        TrafficPhysicsInfo* lpPhysicsInfo = &maTrafficPhysicsInfoList[liPartsIndex];
        lpPhysicsInfo->Construct(static_cast<s32>(luVehicle));                 // 0x82721160

        // 0x82721184 / 0x8272118C -- record +4060 / +4064 (DWARF :190 / :191); see the banner.
        lpPhysicsInfo->mfSteeringDirection = lfSteeringDirection;
        lpPhysicsInfo->mfDrivingDirection  = lfDrivingDirection;

        // 0x82721190 -- r6 is this + 164560 == &mVehicleSoaData.
        lpVehicle->SetPhysical(static_cast<s8>(liPartsIndex), luVehicle, mVehicleSoaData);
    }

    lpVehicle->OnPhysical(leCrashType);   // 0x8272119C -- runs on BOTH paths
}

}
