#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"   // AIDriver::ResetPIDTuningState / GetRacingLine (UpdateInRangeData / OnModeStart)
#include "GameSource/World/AI/Route/BrnRoute.h"                          // BrnAI::Route, RouteNode
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"   // RaceBalancingManager::CalculateScheduleOffset
#include "GameSource/World/AI/BrnAIPortal.h"                             // BrnAI::Portal (GetCurrentNodeY reads the portal height)
#include "SharedClasses/AI/AISectionsResourceType.h"                     // BrnAI::AISectionsData / AISection
#include "GameSource/Math/BrnMathUtils.h"                                // BrnMath::IsNormal / Flatten

#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"              // SetDriver: key -> car asset
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"       // SetDriver: handling record
#include "GameSource/AttribSys/Generated/classes/physicsvehicleboostattribs.h"   // SetDriver: MaxBoostSpeed

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (streamed assert messages)
#include "rw/math/vpu/vector3_operation.h"                     // rw::math::vpu Dot / Magnitude / Normalize / Cross / IsValid / IsZero

#include <cmath>   // std::sqrt (the de-SIMD'd rsqrt sequences)

// BrnAI::AICar -- the per-car AI BRAIN TICK and the module-facing state writers.
// Partfile of BrnAICar.cpp (which owns the speed + route-progress core). This unit bodies
// AICar::Update @0x82798F68 and every callee of it that was absent from the tree, plus the
// writers AIModule / ResetOnTrackManager / AIDriver use to feed a car (UpdateInRangeData,
// UpdateOutOfRangeData, UpdateResetOnTrackSection, Reset, OnModeStart, OnModeStartRacing,
// SetDriver, SetRoadRageMadness). Reconstructed store-for-store from the X360 asm; the
// inlined SIMD sequences are reversed into the rw::math::vpu calls they were compiled from.
//
// ---- AICar::Update @0x82798F68 -- callee order (445 insns, 19 callees) --------------------
//   0x82798FFC  BrnAI::AICar::GetPosition                      @0x8276B1F0  (player car)
//   0x827990E0  BrnAI::AICar::UpdateShortcut                   @0x82765D70  (this UNIT; no IDA export -- image.bin disasm)
//   0x827990EC  BrnAI::AICar::UpdateRelativePositionToPlayer   @0x8276FA88  (BrnAICar.cpp)
//   0x8279910C  BrnAI::AICar::CalcDesiredSpeed                 @0x82796078  (BrnAICar.cpp)      [!lbIsOnline]
//   0x82799170  BrnAI::RaceBalancingManager::CalculateScheduleOffset @0x82789E00              [!lbIsOnline && opponent && !player]
//   0x82799218  BrnAI::AICar::GetDirection                     @0x8276B488  (validity asserts, OUT_OF_RANGE arm)
//   0x827992B0  BrnAI::AICar::GetRight                         @0x8276B998  (this UNIT; no IDA export -- image.bin disasm)
//   0x8279934C  BrnAI::AICar::UpdatePositionOutOfRange         @0x8276F060  (this UNIT)        [OUT_OF_RANGE]
//   0x827993DC / 0x8279946C  GetDirection / GetRight again (post-move validity asserts)
//   0x827994FC  BrnAI::AICar::CheckForSectionChange            @0x8277BFD0  (BrnAICar.cpp)     [not OUT_OF_RANGE]
//   0x82799530..0x82799574  streamed "AI car in unknown state " assert (BasePriorityQueue::Clear = StrStream ctor, operator<<)
//   0x827995C8  BrnAI::AICar::UpdateRouteFinding               @0x8278ADB8  (BrnAICar.cpp)
//   0x82799620  BrnAI::AICar::UpdateRaceDistance               @0x8278AEB8  (BrnAICar.cpp)     [style != FREE_ROAM && HasValidRoute && dest valid]
//   0x82799648  BrnAI::AICar::CheckForFreeRoamSwapToPursuit    @0x8276EE68  (this UNIT)
//   (+ CgsDev::Assert::BeginAssert/FireAssert/EndAssert, __savegprlr_22)
//
// Register map @0x82798F7C: r29=this, r31=r4 lpRaceBalancingManager, f31=f1 lfTimeStep,
// r5 = the PPC float-arg skip slot, r23=r6 lpPlayerCar, r22=r7 lpAISectionsData, r28=r8 lpRoute,
// r30=r9 lbIsOnline. (DWARF BrnAICar.h:95: Update(const RaceBalancingManager*, float32_t,
// const AICar*, AISectionsData*, const Route*, bool).) The pseudocode's a4 is the skip slot.
//
// ---- second-hand callees that had NO IDA export (recovered from image.bin with capstone) ----
//   UpdateShortcut          @0x82765D70 (38 insns)   GetRight             @0x8276B998
//   MoveToSectionOnRoute    @0x8276ED40 (74 insns)   SetNextRouteNodeIndex @0x82764C48 (bl target of UpdatePositionOutOfRange 0x8276F288)
//   UpdateRouteFindingFreeRoam @0x8277C0C0 (22 insns)
//
// ---- rodata constants read from image.bin (BE, VA-0x82000000) ----------------------------
//   flt_820C41C0 = 4.0   (wrong-way limit)       flt_820C3B70 = 1.1920929e-7 (FLT_EPSILON, IsZero tolerance)
//   flt_820C3FAC = 100.0 (free-roam -> pursuit)  flt_820C5E90 = 750.0 (pursuit -> free-roam)
//   flt_820C4318 = 200.0 (AI route-old distance) flt_820C4244 = 50.0 (player route-old distance)
//   flt_820C5E94 = 1/130 (section speed -> buzz ratio)   flt_82F31928 = 0.44704 (mph -> m/s)
//   flt_820C3D90 = 1.1   (max-player-speed scale)        flt_82F302F4 = FLT_MAX
//   flt_820C41A0[2] = {0.0, 0.5} (Reset: per-personality base aggression)
//   flt_82F30170 / flt_82F30174 = 0.2 / 1.0 (OnModeStart MARKED_MAN aggression band)
//   flt_820C4188[2] = {0.1, 0.1} / flt_820C4190[2] = {0.6, 0.6} (OnModeStart per-personality band)
//   unk_82181510 / unk_82181520 = gJVector (0,1,0,0) / gKVector (0,0,1,0); gIVector @0x82181500 = (1,0,0,0)

namespace BrnAI
{
    namespace vpu = rw::math::vpu;

    // ===== file-local constants (every value read from the image; addresses in the banner) =====
    const f32 KF_AICAR_FLOAT_EPSILON              = 1.1920929e-7f;   // flt_820C3B70
    const f32 KF_WRONG_WAY_TIME_LIMIT             = 4.0f;            // flt_820C41C0
    const f32 KF_FREE_ROAM_TO_PURSUIT_DISTANCE    = 100.0f;          // flt_820C3FAC
    const f32 KF_PURSUIT_TO_FREE_ROAM_DISTANCE    = 750.0f;          // flt_820C5E90
    const f32 KF_FREE_ROAM_ROUTE_OLD_DISTANCE     = 300.0f;          // IsFreeRoamingCarsRouteOld literal (Hex-Rays decoded)
    const f32 KF_ROUTE_OLD_DISTANCE_AI            = 200.0f;          // flt_820C4318
    const f32 KF_ROUTE_OLD_DISTANCE_PLAYER        = 50.0f;           // flt_820C4244
    const f32 KF_SECTION_SPEED_TO_BUZZ_RATIO      = 0.0076923077f;   // flt_820C5E94 (1/130)
    const f32 KF_MPH_TO_MPS                       = 0.44704f;        // flt_82F31928
    const f32 KF_MAX_PLAYER_SPEED_SCALE           = 1.1f;            // flt_820C3D90
    const f32 KF_AICAR_FLT_MAX                    = 3.4028235e+38f;  // flt_82F302F4 (0x7F7FFFFF)
    const f32 KAF_PERSONALITY_BASE_AGGRESSION[E_PERSONALITY_TYPE_COUNT] = { 0.0f, 0.5f };   // flt_820C41A0[personality]
    const f32 KF_MARKED_MAN_AGGRESSION_LO         = 0.2f;            // flt_82F30170
    const f32 KF_MARKED_MAN_AGGRESSION_HI         = 1.0f;            // flt_82F30174
    const f32 KAF_RANK_AGGRESSION_LO[E_PERSONALITY_TYPE_COUNT] = { 0.1f, 0.1f };   // unk_820C4188[personality]
    const f32 KAF_RANK_AGGRESSION_HI[E_PERSONALITY_TYPE_COUNT] = { 0.6f, 0.6f };   // unk_820C4190[personality]
    // GetUsefulDirection's "velocity is trustworthy above this speed" threshold.
    // [FLAG PC bring-up] flt_8300D964 is .data and reads 0 in image.bin -- a dynamic-initialiser
    // value with no ARTIST writer export (the readers 0x82770028 are the only xrefs). 0.0 would
    // make every stationary car Normalize a zero velocity, so a NON-ZERO placeholder is shipped.
    // DELETE-WHEN the console value is recovered (PS3 DecFIGS asm of GetUsefulDirection).
    const f32 KF_USEFUL_DIRECTION_MIN_SPEED       = 1.0f;            // flt_8300D964 -- PLACEHOLDER
    const f32 KF_USEFUL_DIRECTION_MIN_LENGTH      = 0.0099999998f;   // literal (Hex-Rays decoded)

    // The console's file strings (the unity .cpp path for .cpp-line asserts, the relative path
    // for the inlined BrnAICar.h accessors).
    static const char* const KPC_AICAR_CPP = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAICar.cpp";
    static const char* const KPC_AICAR_H   = "..\\..\\..\\GameSource\\World/AI/BrnAICar.h";

    // Reset @0x82792800 also (re)stores an 11-word block at .data 0x8300D5D0 on every call
    // (stw x9 + std x1: 1.0f, 0x3FE43E6C, 0x3F98B09C, 0x3FDA23E0, 0x3FE21EDC, 0x3FDDEB96,
    // 0x3F9C9A72, 0x3F923D76, 0x00000001, 0x2EC654DA, 0). [FLAG PC bring-up] no ARTIST function
    // reads 0x8300D5D0 by symbol (xref grep over the export), so the block's owner/consumer is
    // unknown; the stores are reproduced into a file-local twin so the write is not silently
    // dropped. DELETE-WHEN the owning global is identified and homed.
    static u32 gauResetSeededBlock_8300D5D0[11];

    // ==================================================================================
    // Update @0x82798F68 -- the per-frame AI brain tick. See the banner for the callee order.
    // ==================================================================================
    void AICar::Update(const RaceBalancingManager* lpRaceBalancingManager, f32 lfTimeStep,
                       const AICar* lpPlayerCar, AISectionsData* lpAISectionsData,
                       const Route* lpRoute, bool lbIsOnline)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :866

        // 0x82798FE8  mfBehaviourTime += dt
        mfBehaviourTimer = mfBehaviourTimer + lfTimeStep;

        // 0x82798FFC..0x82799068  mfDistanceToPlayer = |mPosition - playerPos| (zero-guarded rsqrt)
        const Vector3 lPlayerPos = lpPlayerCar->GetPosition();
        mfBuzzDistanceToPlayer = vpu::Magnitude(mPosition - lPlayerPos);

        // 0x8279906C  the console tests lane X only (vspltw v0,v0,0 ; vcmpeqfp.)
        CGS_ASSERT(mPosition.x == mPosition.x, "AI car with nonsense position");            // :871

        // 0x827990A4  race timer: held at 0 while starting, else accumulates (not in free roam)
        if (meRouteFindingStyle != E_ROUTE_FINDING_FREE_ROAM)
        {
            if (mbForceStandardRoute)            // == DWARF mbIsStartingRace @+0x1548
                mfRaceTimer = 0.0f;
            else
                mfRaceTimer = mfRaceTimer + lfTimeStep;
        }

        UpdateShortcut(lpAISectionsData);                                                   // 0x827990E0
        UpdateRelativePositionToPlayer(lpPlayerCar);                                        // 0x827990EC

        if (!lbIsOnline)                                                                    // 0x827990F0
        {
            const f32 lfDesiredSpeed = CalcDesiredSpeed(lpRaceBalancingManager, lpAISectionsData, lpPlayerCar);
            mfSpeedOutOfRange = lfDesiredSpeed;   // == DWARF mfDesiredSpeed @+0x14E8 (stfs f1,0x14E8)
            CGS_ASSERT(lfDesiredSpeed >= 0.0f, "mfDesiredSpeed >= 0.0f");                   // :897

            // 0x82799138  opponents only (miOpponentIndex != -1 && !mbIsPlayer)
            if (miOpponentIndex != -1 && !mbIsPlayer)
                lpRaceBalancingManager->CalculateScheduleOffset(this, &mfScheduleOffset0);  // &mafScheduleOffsets[0] (+0x1518)
        }

        const EAICarState leCarState = meCarState;                                          // 0x82799174 (read BEFORE the store below)
        mbUseChosenDistanceFunction = lbIsOnline ? 1 : 0;   // == DWARF mbIsOnline @+0x1550 (stb r30,0x1550)

        if (leCarState == E_AI_CAR_STATE_OUT_OF_RANGE)
        {
            CGS_ASSERT(vpu::IsValid(mPosition), "RwMath::IsValid( mPosition )");            // :912
            CGS_ASSERT(vpu::IsValid(GetDirection()), "RwMath::IsValid( GetDirection() )");   // :913
            CGS_ASSERT(vpu::IsValid(GetRight()), "RwMath::IsValid( GetRight() )");           // :914

            UpdatePositionOutOfRange(lfTimeStep, lpAISectionsData);                         // 0x8279934C

            CGS_ASSERT(vpu::IsValid(mPosition), "RwMath::IsValid( mPosition )");            // :919
            CGS_ASSERT(vpu::IsValid(GetDirection()), "RwMath::IsValid( GetDirection() )");   // :920
            CGS_ASSERT(vpu::IsValid(GetRight()), "RwMath::IsValid( GetRight() )");           // :921
        }
        else
        {
            CheckForSectionChange(lfTimeStep, lpAISectionsData, lpRoute);                   // 0x827994FC

            if (meCarState != E_AI_CAR_STATE_IN_RANGE)                                      // 0x82799500
            {
                // 0x8279950C..0x82799588  streamed assert through the shared message buffer
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "AI car in unknown state " << static_cast<s32>(meCarState) << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_AICAR_CPP, 928);
                CgsDev::Assert::EndAssert();
            }
        }

        // 0x8279958C  last good position: on the ground and not crashing, or in showtime
        if ((!mbIsCrashing && !mbIsInAir) || mbIsInShowtime)
            mLastGoodPosition = mPosition;

        UpdateRouteFinding(lfTimeStep, lpPlayerCar);                                        // 0x827995C8

        // 0x827995CC  race-distance bookkeeping: any real (non-free-roam) style with a live route
        // and a valid destination section
        if (meRouteFindingStyle != E_ROUTE_FINDING_FREE_ROAM &&
            HasValidRoute() &&
            muDestinationSectionIndex != KI_INVALID_SECTION_INDEX)
        {
            UpdateRaceDistance(lpAISectionsData, lpPlayerCar, lfTimeStep);                  // 0x82799620

            // 0x82799624  wrong way for more than 4 s -> drop the route (it will be re-requested)
            if (mfWrongWayTime > KF_WRONG_WAY_TIME_LIMIT)
            {
                mfWrongWayTime = 0.0f;
                Route* lpThisRoute = reinterpret_cast<Route*>(this);
                lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;   // stw 0,0x1408
                lpThisRoute->miNodeCount = 0;                                // stw 0,0x1400
            }
        }

        CheckForFreeRoamSwapToPursuit();                                                    // 0x82799648
    }

    // ==================================================================================
    // HasValidRoute -- DWARF BrnAICar.h:262. X360-inlined at every site as
    //   (mRoute.meStatus != E_STATUS_UNINITIALISED && mRoute.miNodeCount > 0)
    // (e.g. Update @0x827995D8: lwz 0x1408 / lwz 0x1400 / cmpwi ,0 / bgt). No standalone symbol.
    // ==================================================================================
    bool AICar::HasValidRoute() const
    {
        const Route* lpThisRoute = GetRoute();
        return lpThisRoute->GetStatus() != Route::E_STATUS_UNINITIALISED &&
               lpThisRoute->GetNodeCount() > 0;
    }

    // ==================================================================================
    // GetPosition @0x8276B1F0 / GetDirection @0x8276B488 / GetRight @0x8276B998
    //
    // The sret transform accessors. Each runs the per-lane rw::math::vpu::IsValid NaN test on
    // the stored vector, asserts, (direction/right also assert BrnMath::IsNormal) and copies the
    // vector into the return slot. GetRight has no IDA export; its body was disassembled from
    // image.bin (0x8276B998..0x8276BA7C) and is the exact twin of GetDirection over mUnitRight
    // (assert strings @0x820C5C20 / @0x820C5C00, BrnAICar.h:1320 / :1321).
    // ==================================================================================
    Vector3 AICar::GetPosition() const
    {
        CGS_ASSERT(vpu::IsValid(mPosition), "Invalid car position");                        // BrnAICar.h:839
        return mPosition;
    }

    Vector3 AICar::GetDirection() const
    {
        CGS_ASSERT(vpu::IsValid(mDirection), "Invalid car direction");                      // BrnAICar.h:856
        CGS_ASSERT(BrnMath::IsNormal(mDirection), "BrnMath::IsNormal( mDirection )");        // BrnAICar.h:857
        return mDirection;
    }

    Vector3 AICar::GetRight() const
    {
        CGS_ASSERT(vpu::IsValid(mRight), "RwMath::IsValid( mUnitRight )");                  // BrnAICar.h:1320
        CGS_ASSERT(BrnMath::IsNormal(mRight), "BrnMath::IsNormal( mUnitRight )");            // BrnAICar.h:1321
        return mRight;
    }

    // ==================================================================================
    // SetDirection @0x8276B2C0
    //
    // Stores the car's facing (mDirection @+0x1440). Asserts the input is a valid (non-NaN)
    // vector; a non-unit input streams its components into the assert message
    // ("Diretion is  (x,y,z\n" -- the console's own spelling). The store is UNCONDITIONAL.
    // ==================================================================================
    void AICar::SetDirection(Vector3 lDirection)
    {
        CGS_ASSERT(vpu::IsValid(lDirection), "RwMath::IsValid( lDirection )");              // BrnAICar.h:847

        if (!BrnMath::IsNormal(lDirection))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Diretion is  (" << lDirection.x << "," << lDirection.y << "," << lDirection.z << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_AICAR_H, 848);
            CgsDev::Assert::EndAssert();
        }

        mDirection = lDirection;                                                            // stvx128 v127,r26,0x1440
    }

    // ==================================================================================
    // GetUsefulDirection @0x82770028
    //
    // A direction the aggression geometry can trust: the normalised velocity once the car is
    // moving faster than KF_USEFUL_DIRECTION_MIN_SPEED (vcmpgefp. |vel| >= threshold, zero-
    // guarded magnitude); otherwise the stored direction if it has any length (> 0.01);
    // otherwise Cross(direction, right) if THAT has length (vpermwi 0x63 = yzx twice around a
    // vmulfp/vnmsubfp = the SDK cross), and finally the X axis (1,0,0).
    // ==================================================================================
    Vector3 AICar::GetUsefulDirection() const
    {
        const Vector3 lVelocity = GetVelocity();
        if (vpu::Magnitude(lVelocity) >= KF_USEFUL_DIRECTION_MIN_SPEED)
            return vpu::Normalize(GetVelocity());

        const Vector3 lDirection = GetDirection();
        if (vpu::Magnitude(lDirection) > KF_USEFUL_DIRECTION_MIN_LENGTH)
            return GetDirection();

        const Vector3 lRight = GetRight();
        const Vector3 lCross = vpu::Cross(GetDirection(), lRight);
        if (vpu::Magnitude(lCross) > KF_USEFUL_DIRECTION_MIN_LENGTH)
            return lCross;                                                                  // NOT normalised on the console

        return Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    }

    // ==================================================================================
    // UpdateShortcut @0x82765D70 (no IDA export; image.bin disasm, 38 insns)
    //
    //   0x82765D88  lhz best(0x1534) ; 0x7FFF -> lhz default(0x1532)   == GetBestSectionIndex()
    //   0x82765DA4  == 0x7FFF -> mbIsInShortcut = 0
    //   0x82765DC4  bl AISectionsData::GetAISection(sections, best)
    //   0x82765DC8  lbz flags(section+0x17) ; bit0 (clrlwi 31) || bit 0x40 (rlwinm mask 25..25)
    //   0x82765DEC  stb -> mbIsInShortcut (0x154C)
    // The flag test is the same pair AISection::IsUnsuitableForResetOnTrackLink reads.
    // ==================================================================================
    void AICar::UpdateShortcut(AISectionsData* lpAISectionsData)
    {
        const u16 luSection = GetBestSectionIndex();
        if (luSection == KI_INVALID_SECTION_INDEX)
        {
            mbIsInShortcut = false;
            return;
        }

        const AISection* lpSection = lpAISectionsData->GetAISection(luSection);
        const u8 lu8Flags = lpSection->mx8Flags;
        mbIsInShortcut = ((lu8Flags & 0x01) != 0) || ((lu8Flags & 0x40) != 0);
    }

    // ==================================================================================
    // CheckForFreeRoamSwapToPursuit @0x8276EE68
    //
    // FREE_ROAM -> PURSUIT when the player is within 100 m and this car has a proximity slot
    // (miProximityIndex >= 0); PURSUIT -> FREE_ROAM when the player is beyond 750 m OR the
    // proximity slot is gone. Either swap drops the route (status/count), zeroes the wrong-way
    // timer, bumps the route time stamp and sets the speed-selection method to match
    // (PERSONALITY(3) for pursuit, FREE_ROAM(0) for free roam).
    // ==================================================================================
    void AICar::CheckForFreeRoamSwapToPursuit()
    {
        Route* lpThisRoute = reinterpret_cast<Route*>(this);

        if (meRouteFindingStyle == E_ROUTE_FINDING_FREE_ROAM)
        {
            if (mfBuzzDistanceToPlayer >= KF_FREE_ROAM_TO_PURSUIT_DISTANCE)   // 0x8276EE80 bgelr
                return;
            if (miProximityIndex < 0)                                          // 0x8276EE90 bltlr
                return;

            mfWrongWayTime          = 0.0f;
            lpThisRoute->miNodeCount = 0;
            lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;
            ++miRouteTimeStamp;
            meRouteFindingStyle     = E_ROUTE_FINDING_PURSUIT;
            meSpeedSelectionMethod  = E_AI_SPEED_SELECTION_METHOD_PERSONALITY;
            return;
        }

        if (meRouteFindingStyle != E_ROUTE_FINDING_PURSUIT)                    // 0x8276EECC bnelr
            return;

        // 0x8276EED0  swap back when far away, or (else) when the proximity slot is gone
        if (!(mfBuzzDistanceToPlayer > KF_PURSUIT_TO_FREE_ROAM_DISTANCE) && miProximityIndex >= 0)
            return;

        mfWrongWayTime          = 0.0f;
        meRouteFindingStyle     = E_ROUTE_FINDING_FREE_ROAM;
        ++miRouteTimeStamp;
        meSpeedSelectionMethod  = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;
        lpThisRoute->miNodeCount = 0;
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;
    }

    // ==================================================================================
    // GetCurrentNodeY @0x8276EFD8
    //
    // The height of the current route node: the node's section portal's Y. The route node's
    // w-high half-word (node+0xE) is the portal index within the node's section.
    // (BrnRoute.h names that field muPad0x0E; here it IS the portal index -- see the report.)
    // ==================================================================================
    f32 AICar::GetCurrentNodeY(AISectionsData* lpAISectionsData)
    {
        const RouteNode*  lpNode    = GetRoute()->GetNode(miNextRouteNodeIndex);            // lwz 0x1524
        const AISection*  lpSection = lpAISectionsData->GetAISection(lpNode->muSectionIndex);   // lhz node+0xC
        const Portal*     lpPortal  = lpSection->GetPortal(static_cast<u8>(lpNode->muPad0x0E)); // lhz node+0xE
        return lpPortal->GetPositionY();                                                    // Portal[1]
    }

    // ==================================================================================
    // SetNextRouteNodeIndex @0x82764C48 (no IDA export; image.bin disasm)
    //
    // Range-checked setter for miNextRouteNodeIndex. An index >= the node count streams
    // "Trying to set <i> of <count>\n" (:968); a negative one asserts (:969). The store is
    // unconditional.
    // ==================================================================================
    void AICar::SetNextRouteNodeIndex(s32 liNodeIndex)
    {
        const s32 liNodeCount = GetRoute()->GetNodeCount();
        if (liNodeIndex >= liNodeCount)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to set " << liNodeIndex << " of " << liNodeCount << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_AICAR_H, 968);
            CgsDev::Assert::EndAssert();
        }
        CGS_ASSERT(liNodeIndex >= 0, "liNextRouteNodeIndex >= 0");                          // BrnAICar.h:969

        miNextRouteNodeIndex = liNodeIndex;                                                 // stw r28,0x1524
    }

    // ==================================================================================
    // MoveToSectionOnRoute @0x8276ED40 (no IDA export; image.bin disasm, 74 insns)
    //
    // Two searches for luSectionIndex:
    //   1. (player car only, not yet on the master route, master route non-empty) scan the
    //      MASTER route (lpRoute) from node 0; on a hit copy it into mRoute (Route::Construct
    //      @0x82767B28), set mbIsInMasterRoute, seed the next node and zero the wrong-way timer.
    //   2. otherwise scan this car's own route in the window
    //      [max(0, next-4) .. min(count-1, next+32)]; on a hit just seed the next node.
    // Returns whether a section was found.
    // ==================================================================================
    bool AICar::MoveToSectionOnRoute(u16 luSectionIndex, const Route* lpRoute)
    {
        if (!mbIsInMasterRoute && mbIsPlayer && lpRoute->GetNodeCount() > 0)                // 0x8276ED58..0x8276ED7C
        {
            for (s32 liNode = 0; liNode < lpRoute->GetNodeCount(); ++liNode)
            {
                if (lpRoute->GetNode(liNode)->GetSectionIndex() == luSectionIndex)          // 0x8276ED90
                {
                    Route* lpThisRoute = reinterpret_cast<Route*>(this);
                    lpThisRoute->Construct(*lpRoute);                                       // 0x8276EE24 bl Route::Construct
                    mbIsInMasterRoute = true;                                               // 0x8276EE30
                    SetNextRouteNodeIndex(liNode);                                          // 0x8276EE34
                    mfWrongWayTime = 0.0f;                                                  // 0x8276EE44
                    return true;
                }
            }
        }

        // 0x8276EDAC  the local window around the current next node
        const s32 liNext  = miNextRouteNodeIndex;
        s32 liFirst = liNext - 4;
        if (liFirst < 0)
            liFirst = 0;
        s32 liLast = GetRoute()->GetNodeCount() - 1;
        if (liLast >= liNext + 0x20)
            liLast = liNext + 0x20;
        if (liFirst > liLast)
            return false;

        for (s32 liNode = liFirst; liNode <= liLast; ++liNode)
        {
            if (GetRoute()->GetNode(liNode)->GetSectionIndex() == luSectionIndex)           // 0x8276EDF8
            {
                SetNextRouteNodeIndex(liNode);                                              // 0x8276EE58
                return true;
            }
        }
        return false;
    }

    // ==================================================================================
    // UpdatePositionOutOfRange @0x8276F060
    //
    // The out-of-range car is not simulated: it is walked along its route. With a live route
    // and a next node still ahead, the target is (node.x, portalY, node.z); the node's section
    // becomes both the default and best section. Unless the car is already on the node
    // (|delta| <= FLT_EPSILON on every lane), it turns to face the node and moves along its
    // PREVIOUS facing by desiredSpeed*dt; if that step covers the remaining distance (or the
    // car was already on the node) the next node is advanced -- past the last node the route
    // is dropped (status/count/wrong-way).
    // ==================================================================================
    void AICar::UpdatePositionOutOfRange(f32 lfTimeStep, AISectionsData* lpAISectionsData)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :1218

        if (!HasValidRoute())                                                               // 0x8276F0DC
            return;
        Route* lpThisRoute = reinterpret_cast<Route*>(this);
        if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount())                            // 0x8276F110
            return;

        const RouteNode* lpNode = lpThisRoute->GetNode(miNextRouteNodeIndex);               // 0x8276F11C
        const f32 lfNodeX = lpNode->GetX();
        const f32 lfNodeZ = lpNode->GetY();                                                 // lfs f31,4(node)
        const f32 lfNodeY = GetCurrentNodeY(lpAISectionsData);                              // 0x8276F138
        const Vector3 lTarget{ lfNodeX, lfNodeY, lfNodeZ, 0.0f };

        muDefaultSectionIndex = lpNode->GetSectionIndex();                                  // sth 0x1532
        muBestSectionIndex    = lpNode->GetSectionIndex();                                  // sth 0x1534

        const Vector3 lDelta = lTarget - mPosition;                                         // vsubfp (target - mPosition)
        bool lbAdvance = true;
        if (!vpu::IsZero(lDelta, KF_AICAR_FLOAT_EPSILON))                                   // 0x8276F194 vcmpgtfp. |delta| > eps
        {
            const f32 lfDistanceSq = vpu::Dot(lDelta, lDelta);                              // vmsum3fp128 v127
            const Vector3 lStep = mDirection * (mfSpeedOutOfRange * lfTimeStep);            // OLD facing (lvx 0x1440 before the call) * desired speed * dt
            SetDirection(lDelta * (1.0f / std::sqrt(lfDistanceSq)));                        // 0x8276F240 -- refined rsqrt, no zero guard (delta is non-zero here)
            mPosition = mPosition + lStep;                                                  // stvx128 0x1430
            lbAdvance = vpu::Dot(lStep, lStep) >= lfDistanceSq;                             // 0x8276F258 vcmpgefp.
        }

        if (lbAdvance)                                                                      // 0x8276F26C
        {
            if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount() - 1)
            {
                lpThisRoute->miNodeCount = 0;                                               // stw 0,0x1400
                lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                   // stw 0,0x1408
                mfWrongWayTime = 0.0f;                                                      // stfs 0x14F4
            }
            else
            {
                SetNextRouteNodeIndex(miNextRouteNodeIndex + 1);                            // 0x8276F288
            }
        }
    }

    // ==================================================================================
    // IsExtrapolatedRouteGettingOld @0x8276FD50
    //
    // True when the (pursuit/road-rage/extrapolated) route should be re-requested: the car
    // has drifted more than 200 m (50 m for the player) from where the route was built
    // (mLastRoutePosition), or it has no usable route, or the node after the current one
    // (clamped to the last node) is BEHIND the car (dot(direction, node - position) < 0).
    // The pseudocode dropped the `min(next+1, count-1)` node choice (0x8276FE7C..0x8276FE88).
    // ==================================================================================
    bool AICar::IsExtrapolatedRouteGettingOld()
    {
        const f32 lfDrift = vpu::Magnitude(mLastRoutePosition - mPosition);                 // lvx 0x1480 - lvx 0x1430
        const f32 lfLimit = mbIsPlayer ? KF_ROUTE_OLD_DISTANCE_PLAYER : KF_ROUTE_OLD_DISTANCE_AI;
        if (lfDrift > lfLimit)                                                              // 0x8276FDFC
            return true;

        const Route* lpThisRoute = GetRoute();
        if (!(HasValidRoute() && lpThisRoute->GetNodeCount() > 1))                          // 0x8276FE0C..0x8276FE64
            return true;
        if (lpThisRoute->GetNodeCount() - miNextRouteNodeIndex < 1)                         // 0x8276FE74
            return true;

        s32 liNode = miNextRouteNodeIndex + 1;                                              // 0x8276FE7C
        if (liNode >= lpThisRoute->GetNodeCount())
            liNode = lpThisRoute->GetNodeCount() - 1;
        const RouteNode* lpNode = lpThisRoute->GetNode(liNode);

        const Vector3 lNodePos{ lpNode->GetX(), 0.0f, lpNode->GetY(), 0.0f };
        const Vector3 lToNode = lNodePos - GetPosition();                                   // vsubfp128 v0 = node - pos
        const f32 lfAhead = vpu::Dot(GetDirection(), lToNode);                              // vmsum3fp128
        return 0.0f > lfAhead;                                                              // vcmpgtfp. zero > dot
    }

    // ==================================================================================
    // IsFreeRoamingCarsRouteOld @0x8276FBD0
    //
    // A free-roaming AI car's route is "old" when it has no usable route (count <= 1), or
    // when it is within 300 m of the route's LAST node -- in which case the destination
    // section is also invalidated so a new one gets picked. A crashing car never re-asks.
    // ==================================================================================
    bool AICar::IsFreeRoamingCarsRouteOld()
    {
        if (mbIsCrashing)
            return false;

        const Route* lpThisRoute = GetRoute();
        if (!(HasValidRoute() && lpThisRoute->GetNodeCount() > 1))
            return true;

        const RouteNode* lpLast = lpThisRoute->GetNode(lpThisRoute->GetNodeCount() - 1);
        const Vector3 lLastPos{ lpLast->GetX(), 0.0f, lpLast->GetY(), 0.0f };
        const f32 lfDistance = vpu::Magnitude(lLastPos - GetPosition());
        if (lfDistance >= KF_FREE_ROAM_ROUTE_OLD_DISTANCE)
            return false;

        muDestinationSectionIndex = KI_INVALID_SECTION_INDEX;                               // sth 0x7FFF,0x1536
        return true;
    }

    // ==================================================================================
    // UpdateRouteFindingFreeRoam @0x8277C0C0 (no IDA export; image.bin disasm, 22 insns)
    //
    // The player's car uses the extrapolated-route test, every other car the free-roam one;
    // either "old" verdict raises mbRouteRequested. The DWARF's player-car argument is not
    // read (the body only reads `this`).
    // ==================================================================================
    void AICar::UpdateRouteFindingFreeRoam(const AICar* lpPlayerCar)
    {
        const bool lbRouteOld = mbIsPlayer ? IsExtrapolatedRouteGettingOld()               // 0x8277C0E0
                                           : IsFreeRoamingCarsRouteOld();                   // 0x8277C0E8
        if (lbRouteOld)
            mbRouteRequested = true;                                                        // stb 1,0x154D
        (void)lpPlayerCar;
    }

    // ==================================================================================
    // ComputeDistanceToCheckpoint @0x8277C118
    //
    // Distance from this car to the current checkpoint along a route. The route used is the
    // PLAYER's (when this is not the player, a player route exists and has > 1 node, and one
    // of its nodes is this car's best section -- that node), else this car's own route at its
    // next node (when it has > 1 node). Distance = node.distanceToCheckpoint + the car's
    // projection back along the segment behind the node (clamped at 0; segment = previous
    // node - node, or node0 - node1 for the first node; all in the (x, z) ground plane -- the
    // vperm mask @0x82CDA450 = (pos.x, pos.z, .., ..)). For a PARTIAL route with a valid
    // destination section the flat distance from the route's last node to that section's
    // middle is added. Asserts the result is non-negative and writes it; returns "found".
    // ==================================================================================
    bool AICar::ComputeDistanceToCheckpoint(const AISectionsData* lpAISectionsData,
                                            const AICar* lpPlayerCar, f32* lpfOutDistance) const
    {
        bool         lbFound = false;
        const Route* lpRoute = 0;
        s32          liNode  = 0;

        // 0x8277C158..0x8277C1D0  search the player's route for this car's best section
        if (!mbIsPlayer && lpPlayerCar != 0 && lpPlayerCar->GetRoute()->GetNodeCount() > 1)
        {
            const u16    luSection     = GetBestSectionIndex();
            const Route* lpPlayerRoute = lpPlayerCar->GetRoute();
            for (liNode = 0; liNode < lpPlayerRoute->GetNodeCount(); ++liNode)
            {
                if (lpPlayerRoute->GetNode(liNode)->GetSectionIndex() == luSection)
                {
                    lbFound = true;
                    lpRoute = lpPlayerRoute;
                    break;
                }
            }
        }

        // 0x8277C1EC  fall back to this car's own route at its next node
        if (!lbFound && GetRoute()->GetNodeCount() > 1)
        {
            liNode  = miNextRouteNodeIndex;
            lpRoute = GetRoute();
            lbFound = true;
        }

        if (!lbFound)                                                                       // 0x8277C214
            return false;

        CGS_ASSERT(lpRoute != 0, "lpRoute != NULL");                                        // :1927

        const RouteNode* lpNode = lpRoute->GetNode(liNode);                                 // 0x8277C24C
        f32 lfSegX;
        f32 lfSegZ;
        if (liNode <= 0)
        {
            const RouteNode* lpNext = lpRoute->GetNode(1);                                  // 0x8277C27C
            lfSegX = lpNode->GetX() - lpNext->GetX();                                       // node0 - node1
            lfSegZ = lpNode->GetY() - lpNext->GetY();
        }
        else
        {
            const RouteNode* lpPrev = lpRoute->GetNode(liNode - 1);                         // 0x8277C2AC
            lfSegX = lpPrev->GetX() - lpNode->GetX();                                       // prev - node
            lfSegZ = lpPrev->GetY() - lpNode->GetY();
        }

        // 0x8277C2F0..0x8277C3B8  along = max(0, dot2D(pos.xz - node, seg / |seg|))
        const f32 lfToNodeX = mPosition.x - lpNode->GetX();
        const f32 lfToNodeZ = mPosition.z - lpNode->GetY();
        const f32 lfInvSegLength = 1.0f / std::sqrt(lfSegX * lfSegX + lfSegZ * lfSegZ);      // vrsqrtefp + 2 refinements, no zero guard
        f32 lfAlong = (lfToNodeX * lfSegX + lfToNodeZ * lfSegZ) * lfInvSegLength;
        if (-lfAlong >= 0.0f)                                                               // fneg / fsel -> 0
            lfAlong = 0.0f;
        f32 lfDistance = lfAlong + lpNode->GetDistanceToCheckpoint();                       // fadds f31,f0,f13(node+8)

        // 0x8277C3BC  partial route: add the flat gap from the last node to the destination
        if (lpRoute->GetStatus() == Route::E_STATUS_PARTIAL &&
            muDestinationSectionIndex != KI_INVALID_SECTION_INDEX)
        {
            // AISectionsData::GetMiddle(dest) is inlined on the console as
            // GetAISection(dest)->GetMiddle() (0x8277C3D0 / 0x8277C3DC), then BrnMath::Flatten.
            const AISection* lpDestination = lpAISectionsData->GetAISection(muDestinationSectionIndex);
            const Vector2 lMiddle = BrnMath::Flatten(lpDestination->GetMiddle());           // 0x8277C3E4
            const RouteNode* lpLast = lpRoute->GetNode(lpRoute->GetNodeCount() - 1);        // 0x8277C3F8
            const f32 lfGapX = lMiddle.x - lpLast->GetX();
            const f32 lfGapZ = lMiddle.y - lpLast->GetY();
            const f32 lfGapSq = lfGapX * lfGapX + lfGapZ * lfGapZ;
            lfDistance = lfDistance + (lfGapSq > 0.0f ? std::sqrt(lfGapSq) : 0.0f);         // vsel zero guard
        }

        CGS_ASSERT(lfDistance >= 0.0f, "lfDistance >= 0.0f");                               // :1966
        *lpfOutDistance = lfDistance;
        return true;
    }

    // ==================================================================================
    // UpdateInRangeData @0x82792A18 -- the physics-driven car feeds its frame data in.
    //
    // Register map: r31=this, r4=lpAISectionsData (unused by the body), r30=r5 &transform,
    // v127=v1 velocity, f31=f1 speed, r28=r7 under-car section (r6 = float skip slot),
    // r26/r25/r27 = r8/r9/r10 = in-air / crashing / in-showtime, then the six stack bools
    // arg_6F on-start-line, arg_77 is-player, arg_7F driven-by-player, arg_87 drifting,
    // arg_8F touching-race-car, arg_97 touching-player.
    // ==================================================================================
    void AICar::UpdateInRangeData(AISectionsData* lpAISectionsData, const Matrix44Affine& lrTransform,
                                  Vector3 lVelocity, f32 lfSpeed, u16 luUnderCarSectionIndex,
                                  bool lbIsInAir, bool lbIsCrashing, bool lbIsInShowtime,
                                  bool lbIsOnStartLine, bool lbIsPlayer, bool lbIsDrivenByPlayer,
                                  bool lbIsDrifting, bool lbIsTouchingRaceCar, bool lbIsTouchingPlayer)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :1372

        const Vector3& lrPosition = lrTransform.wAxis;                                      // lvx128 v0,r30,0x30 -> stvx128 0x1430
        const Vector3& lrAt       = lrTransform.zAxis;                                      // lvx128 v1,r30,0x20
        const Vector3& lrRight    = lrTransform.xAxis;                                      // lvx128 v1,r0,r30
        mPosition = lrPosition;
        SetDirection(lrAt);                                                                 // 0x82792AB8
        SetRight(lrRight);                                                                  // 0x82792AC4

        muBestSectionIndex = luUnderCarSectionIndex;                                        // sth r28,0x1534 (== muUnderCarSectionIndex)
        mVelocity = lVelocity;                                                              // stvx128 v127,0x1470

        CGS_ASSERT(lbIsPlayer || !lbIsInShowtime, "lbIsPlayer || !lbIsInShowtime");        // :1382

        mfSpeedInRange      = lfSpeed;                                                      // stfs f31,0x14E4 (== mfActualInRangeSpeed)
        mbIsInAir           = lbIsInAir;                                                    // stb 0x1541
        mbIsCrashing        = lbIsCrashing;                                                 // stb 0x1542
        mbIsInShowtime      = lbIsInShowtime;                                               // stb 0x1543
        mbIsDrifting        = lbIsDrifting;                                                 // stb 0x1544
        mbIsTouchingRaceCar = lbIsTouchingRaceCar;                                          // stb 0x1545
        mbIsTouchingPlayer  = lbIsTouchingPlayer;                                           // stb 0x1546
        mbIsOnStartLine     = lbIsOnStartLine;                                              // stb 0x1547
        mbIsDrivenByPlayer  = lbIsDrivenByPlayer;                                           // stb 0x154A

        if (meCarState == E_AI_CAR_STATE_IN_RANGE)                                          // 0x82792B4C
        {
            // 0x82792B50  lwz r3,0x14B0(this) ; bl BrnAI::AIDriver::ResetPIDTuningState @0x8277DA48
            // The console has NO null guard here (`lwz r3,0x14B0 ; bl`); the [GUARD] is the
            // host's, because AIModule::Construct never runs the console's AICar::Construct.
            if (mpDriverHost != 0)
            {
                mpDriverHost->ResetPIDTuningState();
            }
        }

        mbIsPlayer = lbIsPlayer;                                                            // stb r30,0x1549
        (void)lpAISectionsData;
    }

    // ==================================================================================
    // UpdateResetOnTrackSection @0x82765EA0 -- the reset-on-track manager tells the car where
    // it will be put back (section + the two portals bounding the reset spot) and how fast.
    // ==================================================================================
    void AICar::UpdateResetOnTrackSection(EResetSpeedType leResetSpeedType, u16 luResetOnTrackSectionIndex,
                                          u8 luStartPortal, u8 luEndPortal)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :1415
        CGS_ASSERT(!IsInAir() || mbIsInShowtime, "!IsInAir() || mbIsInShowtime");            // :1416
        CGS_ASSERT(luResetOnTrackSectionIndex != KI_INVALID_SECTION_INDEX,
                   "luResetOnTrackSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");     // :1417

        meResetSpeedType           = leResetSpeedType;                                      // stw 0x14D0
        muResetOnTrackSectionIndex = luResetOnTrackSectionIndex;                            // sth 0x1530
        muResetOnTrackStartPortal  = luStartPortal;                                         // stb 0x1538
        muResetOnTrackEndPortal    = luEndPortal;                                           // stb 0x1539
    }

    // ==================================================================================
    // UpdateOutOfRangeData @0x8276F398 -- the module places an out-of-range car.
    //
    // v126=v1 position, v127=v2 at vector, r25=r4 section index, r24=r5 section speed byte.
    // Stores the position, asserts the at vector is valid (streamed "Bad At vector"), sets
    // the facing, derives the right vector as Normalize(Cross(Y axis, at)) -- falling back to
    // the Z axis when that cross is degenerate (every |lane| <= FLT_EPSILON) -- and seeds the
    // default/best section and the buzz-frequency ratio min(1, sectionSpeed / 130).
    // ==================================================================================
    void AICar::UpdateOutOfRangeData(Vector3 lPosition, Vector3 lAtVector, u16 luSectionIndex,
                                     u8 luSectionSpeed)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :1440

        mPosition = lPosition;                                                              // stvx128 v126,0x1430

        if (!vpu::IsValid(lAtVector))                                                       // 0x8276F41C..0x8276F484 (per-lane vcmpeqfp.)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Bad At vector";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_AICAR_CPP, 1444);
            CgsDev::Assert::EndAssert();
        }

        SetDirection(lAtVector);                                                            // 0x8276F500

        // 0x8276F504..0x8276F5B0  right = Cross(gJVector, at) (vpermwi 0x63 = yzx on both
        // operands around vmulfp/vnmsubfp), degenerate -> gKVector, else Normalize.
        const Vector3 lCross = vpu::Cross(vpu::GetVector3_YAxis(), lAtVector);
        Vector3 lRight;
        if (vpu::IsZero(lCross, KF_AICAR_FLOAT_EPSILON))
            lRight = vpu::GetVector3_ZAxis();                                               // lvx128 unk_82181520
        else
            lRight = vpu::Normalize(lCross);
        SetRight(lRight);                                                                   // 0x8276F5B8

        muDefaultSectionIndex = luSectionIndex;                                             // sth r25,0x1532
        muBestSectionIndex    = luSectionIndex;                                             // sth r25,0x1534

        // 0x8276F5BC..0x8276F5F4  ratio = min(1.0, float(speed) * (1/130))  (fsel on ratio-1)
        const f32 lfRatio = static_cast<f32>(luSectionSpeed) * KF_SECTION_SPEED_TO_BUZZ_RATIO;
        mfBuzzFrequencyRatio = (lfRatio - 1.0f >= 0.0f) ? 1.0f : lfRatio;                   // stfs 0x1510
    }

    // ==================================================================================
    // SetRoadRageMadness @0x8276ECB0 -- road-rage aggression tuning (madness in [0,1]).
    //   aggression = madness * 0.6 (flag set), proximity 0.5, time 1.0, relative speed 0.666,
    //   acceleration 1.0. Called by OnModeStart (ROAD_RAGE) and AIModule::HandleGameActions.
    // ==================================================================================
    void AICar::SetRoadRageMadness(f32 lfMadness)
    {
        mAggressiveness.SetAggression(lfMadness * 0.6f);                                    // stfs 0x140C ; stb 1,0x1410
        mAggressiveness.SetProximityToSpeedMatch(0.5f);
        mAggressiveness.SetTimeForSpeedMatch(1.0f);
        mAggressiveness.SetRelativeSpeedForMatch(0.666f);                                   // stfs 0x141C (0.66600001)
        mAggressiveness.SetAcclerationRateForSpeedMatch(1.0f);
    }

    // ==================================================================================
    // OnModeStartRacing @0x82765C50 -- the race-start gun: behaviour -> CRUISING (the inlined
    // SetBehaviour: previous <- current, timer <- 0) and the starting-race flag drops.
    // ==================================================================================
    void AICar::OnModeStartRacing()
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :491
        CGS_ASSERT(mbIsInGameMode, "mbIsInGameMode");                                       // :492

        SetBehaviour(E_AI_BEHAVIOUR_CRUISING);                                              // 0x14E0=0 ; 0x14B8<-0x14B4 ; 0x14B4=3
        mbForceStandardRoute = 0;                                                           // stb 0,0x1548 (== mbIsStartingRace)
    }

    // ==================================================================================
    // OnModeStart @0x8277BD20 -- a game mode starts for this car.
    //
    // r4 speed-selection method, r5 opponent index, r6 route-finding style, r7/r8 the two
    // route bools (-> +0x153F mbCanDeviateFromRoute / +0x153E mbCanUseAIShortcuts), r9 the
    // destination section, f1 the progression rank ratio. Resets the timers/route/checkpoint
    // state, enters game mode, marks the car as starting, sets STOP behaviour, then tunes the
    // aggression block by style: ROAD_RAGE -> SetRoadRageMadness(0); MARKED_MAN -> a 0.2..1.0
    // band by rank; everything else -> a per-personality 0.1..0.6 band by rank with 0.7s.
    // ==================================================================================
    void AICar::OnModeStart(EAISpeedSelectionMethod leSpeedSelectionMethod, s32 liOpponentIndex,
                            ERouteFindingStyle leRouteFindingStyle, bool lbCanDeviateFromRoute,
                            bool lbCanUseAIShortcuts, u16 luDestinationSectionIndex,
                            f32 lfProgressionRankAsRatio)
    {
        CGS_ASSERT(IsActive(), "IsActive()");                                              // :396
        CGS_ASSERT(static_cast<u32>(liOpponentIndex) < 8u,
                   "liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS"); // :397 (cmplwi ,8)
        CGS_ASSERT(lfProgressionRankAsRatio >= 0.0f && lfProgressionRankAsRatio <= 1.0f,
                   "lfProgressionRankAsRatio >= 0.0f && lfProgressionRankAsRatio <= 1.0f");  // :398

        Route* lpThisRoute = reinterpret_cast<Route*>(this);

        mfRaceTimer             = 0.0f;                                                     // 0x14FC
        mfAlternativeRouteTimer = 0.0f;                                                     // 0x1500
        mfWrongWayTime          = 0.0f;                                                     // 0x14F4
        mfBehaviourTimer        = 0.0f;                                                     // 0x14E0
        meRouteFindingStyle     = leRouteFindingStyle;                                      // 0x14C0
        mbWantsAlternativeRoute = lbCanDeviateFromRoute ? 1 : 0;                            // 0x153F (== mbCanDeviateFromRoute)
        ++miRouteTimeStamp;                                                                 // 0x1528
        mfDistanceToCheckpoint  = KF_AICAR_FLT_MAX;                                         // 0x14F0 <- flt_82F302F4
        mbUseAIShortcuts        = lbCanUseAIShortcuts ? 1 : 0;                              // 0x153E (== mbCanUseAIShortcuts)
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                           // 0x1408
        lpThisRoute->miNodeCount = 0;                                                       // 0x1400
        mbIsInGameMode          = true;                                                     // 0x154B
        muDefaultSectionIndex   = KI_INVALID_SECTION_INDEX;                                 // 0x1532
        mbHasBlockCheckpoints   = (leRouteFindingStyle == E_ROUTE_FINDING_RACE) ? 1 : 0;    // 0x153D (== mbUseBlockSections)
        miCurrentCheckpoint     = 0;                                                        // 0x1520
        mbForceStandardRoute    = 1;                                                        // 0x1548 (== mbIsStartingRace)
        mePreviousBehaviour     = meBehaviour;                                              // 0x14B8 <- 0x14B4
        meBehaviour             = E_AI_BEHAVIOUR_STOP;                                      // 0x14B4
        meSpeedSelectionMethod  = leSpeedSelectionMethod;                                   // 0x14BC
        muDestinationSectionIndex = luDestinationSectionIndex;                              // 0x1536
        miOpponentIndex         = static_cast<s8>(liOpponentIndex);                         // 0x153A
        // stb 1,0x1410 -- mAggressiveness.mbAggressionLevelSet; every arm below re-sets it
        // through SetAggression, so the console's early store is subsumed.

        if (mbIsPlayer)                                                                     // 0x8277BE28
        {
            // 0x8277BE30  if (mpDriver) { driver+0x1B24 = 0.9975f (0x3F7F5C29); driver+0x1B28 = 400.0004f }
            // driver+0x1B24 / +0x1B28 fall inside the driver's embedded RacingLine (@driver+0xF20),
            // at RacingLine +0xC04 / +0xC08 == mfCentreLineAhead / mfCentreLineAheadRecip
            // (BrnRacingLine.h :130 / :133); 400.0004 == 1 / (1 - 0.9975), which is what pins the
            // pairing. On the host the RacingLine object lives in AIDriver's trailing member, so
            // this is the de-inlined form of the two console stores.
            if (mpDriverHost != 0)
            {
                mpDriverHost->GetRacingLine().mfCentreLineAhead      = 0.9975f;
                mpDriverHost->GetRacingLine().mfCentreLineAheadRecip = 400.0004f;
            }
        }

        if (meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE)                               // 0x8277BE58
        {
            SetRoadRageMadness(0.0f);
            return;
        }

        f32 lfRelativeSpeed;
        f32 lfAcceleration;
        if (meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)                              // 0x8277BE84
        {
            const f32 lfLo = KF_MARKED_MAN_AGGRESSION_LO;
            const f32 lfHi = KF_MARKED_MAN_AGGRESSION_HI;
            mAggressiveness.SetAggression((lfHi - lfLo) * lfProgressionRankAsRatio + lfLo);
            mAggressiveness.SetProximityToSpeedMatch(1.0f);
            mAggressiveness.SetTimeForSpeedMatch(1.0f);
            lfRelativeSpeed = 0.5f;
            lfAcceleration  = 1.0f;
        }
        else
        {
            // 0x8277BEE0  lvlx unk_820C4190[personality] / unk_820C4188[personality] ; lerp by rank
            const f32 lfLo = KAF_RANK_AGGRESSION_LO[mePersonalityType];
            const f32 lfHi = KAF_RANK_AGGRESSION_HI[mePersonalityType];
            mAggressiveness.SetAggression((lfHi - lfLo) * lfProgressionRankAsRatio + lfLo);  // vmaddfp (hi-lo)*ratio + lo
            mAggressiveness.SetProximityToSpeedMatch(0.7f);
            mAggressiveness.SetTimeForSpeedMatch(0.7f);
            lfRelativeSpeed = 0.7f;
            lfAcceleration  = 0.7f;
        }
        mAggressiveness.SetRelativeSpeedForMatch(lfRelativeSpeed);                          // stfs 0x141C
        mAggressiveness.SetAcclerationRateForSpeedMatch(lfAcceleration);
    }

    // ==================================================================================
    // Reset @0x82792800 -- the console's re-initialiser (AIModule::HandleManagementEvents).
    //
    // r4 personality, r5 lbKeepTransform (the reset-on-track/position block is skipped when
    // set). Facing -> gKVector (0,0,1), right -> gIVector (1,0,0), every timer/flag/section/
    // route field to its idle value, the driver unbound, INACTIVE, relative location UNKNOWN,
    // aggression = per-personality base with 1.0/1.0/0.5/1.0 speed-match knobs,
    // mfDistanceToCheckpoint = FLT_MAX, proximity index -100, mLastRoutePosition = 0.
    // ==================================================================================
    void AICar::Reset(EPersonalityType lePersonalityType, bool lbKeepTransform)
    {
        SetDirection(vpu::GetVector3_ZAxis());                                              // lvx128 unk_82181520
        SetRight(vpu::GetVector3_XAxis());                                                  // lvx128 gIVector

        Route* lpThisRoute = reinterpret_cast<Route*>(this);

        mfWrongWayTime           = 0.0f;                                                    // 0x14F4
        mbIsOnStartLine          = false;                                                   // 0x1547
        mfTimeInInvalidSection   = 0.0f;                                                    // 0x1514
        muBestSectionIndex       = KI_INVALID_SECTION_INDEX;                                // 0x1534
        muDefaultSectionIndex    = KI_INVALID_SECTION_INDEX;                                // 0x1532
        mbForceStandardRoute     = 0;                                                       // 0x1548 (== mbIsStartingRace)
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                           // 0x1408
        lpThisRoute->miNodeCount = 0;                                                       // 0x1400
        lpThisRoute->miDefaultStartNode = 0;                                                // 0x1404
        muDestinationSectionIndex = KI_INVALID_SECTION_INDEX;                               // 0x1536
        meRouteFindingStyle      = E_ROUTE_FINDING_FREE_ROAM;                               // 0x14C0
        mbUseAIShortcuts         = 0;                                                       // 0x153E
        mbWantsAlternativeRoute  = 0;                                                       // 0x153F
        mpDriverHost             = 0;                                                       // stw 0,0x14B0 (guest mpDriver)

        if (!lbKeepTransform)                                                               // 0x82792898
        {
            muResetOnTrackSectionIndex = KI_INVALID_SECTION_INDEX;                          // 0x1530
            muResetOnTrackStartPortal  = 1;                                                 // 0x1538
            meResetSpeedType           = E_RESET_SPEED_TYPE_COUNT;                          // 0x14D0 <- 20
            mPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };                                          // vspltisw 0 -> stvx128 0x1430
        }

        mbPlaceOnTrackRequested = false;                                                    // 0x153C

        // 0x827928E8..0x8279296C  the .data block re-store (see the banner of gauResetSeededBlock)
        gauResetSeededBlock_8300D5D0[0]  = 0x3F800000u;   // 1.0f
        gauResetSeededBlock_8300D5D0[1]  = 0x3FE43E6Cu;
        gauResetSeededBlock_8300D5D0[2]  = 0x3F98B09Cu;
        gauResetSeededBlock_8300D5D0[3]  = 0x3FDA23E0u;
        gauResetSeededBlock_8300D5D0[4]  = 0x3FE21EDCu;
        gauResetSeededBlock_8300D5D0[5]  = 0x3FDDEB96u;
        gauResetSeededBlock_8300D5D0[6]  = 0x3F9C9A72u;
        gauResetSeededBlock_8300D5D0[7]  = 0x3F923D76u;
        gauResetSeededBlock_8300D5D0[8]  = 0x00000001u;   // std: hi word
        gauResetSeededBlock_8300D5D0[9]  = 0x2EC654DAu;   // std: lo word
        gauResetSeededBlock_8300D5D0[10] = 0u;

        mfBuzzDistanceToPlayer = 0.0f;                                                      // 0x1508 (== mfDistanceToPlayer)
        mePersonalityType      = lePersonalityType;                                         // 0x14CC
        meCarState             = E_AI_CAR_STATE_INACTIVE;                                   // 0x14C8 <- 2
        meRelativeLocation     = E_RELATIVE_UNKNOWN;                                        // 0x14D4 <- 4

        mAggressiveness.SetAggression(KAF_PERSONALITY_BASE_AGGRESSION[lePersonalityType]);  // flt_820C41A0[personality] ; stb 1,0x1410
        mAggressiveness.SetProximityToSpeedMatch(1.0f);
        mAggressiveness.SetTimeForSpeedMatch(1.0f);
        mAggressiveness.SetRelativeSpeedForMatch(0.5f);                                     // 0x141C <- 0x3F000000
        mAggressiveness.SetAcclerationRateForSpeedMatch(1.0f);

        mfDistanceAheadOfPlayer = 0.0f;                                                     // 0x14F8
        mbIsInShortcut          = false;                                                    // 0x154C
        mfWrongWayTime          = 0.0f;                                                     // 0x14F4 (again)
        mbRouteRequested        = false;                                                    // 0x154D
        mfDistanceToCheckpoint  = KF_AICAR_FLT_MAX;                                         // 0x14F0
        mfRouteTimer            = 0.0f;                                                     // 0x14EC
        mbIsInMasterRoute       = false;                                                    // 0x154E
        mfScheduleOffset0       = 0.0f;                                                     // 0x1518
        mfScheduleOffset1       = 0.0f;                                                     // 0x151C
        miProximityIndex        = -100;                                                     // 0x152C
        mLastRoutePosition      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };                                // stfs f31 x3 + stw 0 -> stvx128 0x1480
    }

    // ==================================================================================
    // SetDriver @0x8276EF20 -- bind the driver and derive the car's top speed from its attribs.
    //
    //   stw r4 -> +0x14B0 (mpDriver)
    //   burnoutcarasset(mCarAssetAttribKey @+0x14D8, NULL)                   (sub_82204998)
    //   handling = physicsvehiclehandling(RefSpec(carData+0x158).GetCollection(), NULL)
    //   boost    = physicsvehicleboostattribs(RefSpec(handlingData+0x78).GetCollection(), NULL)
    //   mfMaxPlayerSpeed = boostData[0] (MaxBoostSpeed, mph) * 0.44704 * 1.1
    // (boostData+0x00 == MaxBoostSpeed per VehicleAttribs::SetupAttribs' BP_VA_SRC_F(lpData, 0x00).)
    // ==================================================================================
    void AICar::SetDriver(AIDriver* lpDriver)
    {
        mpDriverHost = lpDriver;                                                            // stw r11,0x14B0

        Attrib::Gen::burnoutcarasset lCarAsset(mCarAssetAttribKey, 0);                      // ld r4,0x14D8 ; bl sub_82204998
        Attrib::Gen::physicsvehiclehandling lHandling(
            const_cast<Attrib::Collection*>(lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), 0);
        Attrib::Gen::physicsvehicleboostattribs lBoost(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lHandling.PhysicsVehicleBoostAttribs()).GetCollection()), 0);

        const f32* lpBoostData = static_cast<const f32*>(lBoost.GetLayoutPointer());
        // [GUARD] the console dereferences boostData unconditionally (lfs f13,0(r11)); a car
        // whose attrib chain is unresolved on this host would fault, so the read is guarded
        // and the speed left untouched -- CalcRoadRageSpeed then floors at 0 (GetDecentSpeed).
        if (lpBoostData != 0)
            mfMaxPlayerSpeed = lpBoostData[0] * KF_MPH_TO_MPS * KF_MAX_PLAYER_SPEED_SCALE;   // stfs 0x1504
    }

    // ==================================================================================
    // [FLAG relocate] Aggressiveness::SetAggression (DWARF BrnAIAggressiveness.h:39) and
    // SetRelativeSpeedForMatch (:68) are declared in BrnAIAggressiveness.h but have NO body in
    // the tree and NO X360 symbol -- the console inlines both as `stfs level,+0 ; stb 1,+4`
    // and `stfs +0x10` at every call site (Reset 0x8279297C, OnModeStart 0x8277BEC8/0x8277BF60,
    // SetRoadRageMadness 0x8276ECCC..0x8276ED14). Their canonical home is
    // BrnAIAggressiveness.cpp (no lane owns it this wave); bodied here so this unit links.
    // DELETE-WHEN moved to BrnAIAggressiveness.cpp.
    // ==================================================================================
    void Aggressiveness::SetAggression(f32 lfAggression)
    {
        mfAggressionLevel    = lfAggression;   // stfs +0x00
        mbAggressionLevelSet = true;           // stb 1,+0x04
    }

    void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue)
    {
        mfRelativeSpeedForSpeedMatch = lfValue;   // stfs +0x10
    
}

    // ==================================================================================
    // (AISectionsData::GetAISection @0x8230F6D0 -- the [FLAG relocate] copy that stood here was removed
    //  2026-09-03: the canonical body is SharedClasses/AI/AISectionsResourceType.cpp:74, mounted -- LNK2005.)
    // ---- the four declared-only accessors the route / drive lanes link against (conductor, 2026-09-03).
    // DWARF BrnAICar.h :185 / :371 / :275 / :326; each is the console's inlined member read (no export).
    s32         AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
    s8          AICar::GetOpponentIndex() const      { return miOpponentIndex; }
    EAICarState AICar::GetState() const              { return meCarState; }
    bool        AICar::IsPlayerCar() const           { return mbIsPlayer; }


    // ---- three more declared-only accessors (DWARF :182 / :287 / :332), RaceBalancingManager links against them.
    // GetNextRouteNode: Route::GetNode(miNextRouteNodeIndex), null when out of range (the console's
    // `cmpw next, nodeCount ; bge -> li r3,0` at RaceBalancingManager::ComputeParSpeed @0x82789EC0).
    const RouteNode* AICar::GetNextRouteNode() const
    {
        if (miNextRouteNodeIndex < 0 || miNextRouteNodeIndex >= GetRoute()->GetNodeCount())
        {
            return 0;
        }
        return GetRoute()->GetNode(miNextRouteNodeIndex);
    }
    f32  AICar::GetMaxPlayerSpeed() const { return mfMaxPlayerSpeed; }
    bool AICar::IsAheadOfPlayer() const   { return mbIsAheadOfPlayer; }

}
