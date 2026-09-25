#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Matrix44Affine
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<8>
#include "GameShared/GameClasses/Algorithms/CgsBubbleSort.h" // CgsAlgorithms::BubbleSort
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the inline accessors' tripwires)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h" // BrnDirector::Camera::VehicleInfo (mpRaceCars' real pointee)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"      // Camera::Utils::CreateLookAt (the heading-space frame)
#include "rw/math/vpu/vector3_operation.h"                     // Cross / Normalize (the impact-space basis)
#include "rw/math/vpu/matrix44affine_operation.h"              // SLerp / IsValid (the heading blend + the NaN cascade)

#include <cfloat>   // FLT_MAX -- the no-opposing-car sentinel (XEX rodata flt_8200173C,
                    // read as 0x7F7FFFFF == 3.4028235e+38 == FLT_MAX)

// BrnDirector::AllVehicleData - the director's per-frame view of every live
// vehicle (player spaces, the race-car table, traffic, and the sorted
// nearest-cars-to-player list). Class shape / member names / method set verbatim
// from the DecFIGS DWARF (BrnDirectorAllVehicleData.h:45/:108/:121-:139); gated
// on the X360 ledger. This TU bodies the two nearest-car distance queries; the
// rest of the surface is declared-only (their own ledger functions).
//
// RECONCILED (2026-07) with the earlier minimal slice this header replaced: the
// slice's NearestCarInfo field names (miVehicleIndex/mfDistanceSquared/
// muTeamValue, roles read off the Update @0x8221D938 caller) are superseded by
// the DWARF names below (same 12-byte element, pinned by the static_asserts);
// its NearestCarInfoArray typedef and the Append-instantiation anchor
// (@0x821FBA48, the BrnDirectorAllVehicleData.cpp FILE TU) are KEPT; its
// GetNearestRaceCarIndexToPlayer (@0x82233380) / GetRaceCar (@0x82205DE8)
// decls fold into the DWARF-typed ones below (the RaceIntro consumer reads the
// race car's +0x220 position lane through the returned record).
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficDirectorEntity; } }

namespace BrnDirector
{
    // ⛔ RETIRED (2026-08-01) -- A NAMESPACE FORK, not a missing type. This header used to
    // forward-declare `BrnDirector::VehicleInfo`, but no such type exists: the real one is
    // BrnDirector::Camera::VehicleInfo (Camera/SharedIO/BrnPlayerInfo.h, included above).
    // The declared pointee was therefore an incomplete type that could never be completed, so
    // indexing mpRaceCars was a hard C2036 and GetPlayer/GetRaceCar could not be bodied at all.
    // Same fork BrnArbStateCarSelect.cpp documents for ArbStateSharedInfo::mpPlayerCar; fixed
    // once here for the whole surface rather than per call site with a reinterpret_cast.
    // (Spelled out as Camera::VehicleInfo everywhere below rather than typedef'd back into
    // BrnDirector -- a namespace-scope typedef of that name would clash with the surviving
    // `struct VehicleInfo;` forward declarations in the sibling director headers.)

    struct AllVehicleData
    {
        // DWARF :108 -- one sorted nearest-car row. X360-DIVERGENCE NOTE: the PS3
        // DWARF lists only {meRaceCarIndex, mfDistance}, but the X360 element
        // stride is 12 (the Array<...,8> count word sits at +0x60 == 8*12) and
        // SqDistanceOfNearestOpposingTeamMember compares a third word @+8 against
        // the caller-supplied team -- the X360 record carries the row car team id.
        struct NearestCarInfo
        {
            EActiveRaceCarIndex meRaceCarIndex;   // :116  +0x0
            f32                 mfDistance;       // :117  +0x4 (squared distance)
            s32                 miTeam;           // X360 +0x8 (FLAG: name inferred; absent from the PS3 DWARF)

            // :111 -- the sort order BubbleSort uses. BODIED INLINE 2026-08-01: the sort
            // (CgsAlgorithms::BubbleSort<T,N> @0x82213F80) compares element +0x04, i.e.
            // mfDistance, and produces the ASCENDING nearest-first order both distance
            // queries above rely on. It has no standalone X360 symbol -- the console inlines
            // the compare straight into the sort's inner loop -- so a one-line member
            // comparison IS the console shape.
            bool operator>(const NearestCarInfo& lrOther) const
            {
                return mfDistance > lrOther.mfDistance;
            }
        };

        // The 8-slot nearest-car container (inline buffer 8*12 = 0x60 bytes, live
        // count word at +0x60 -- the Append @0x821FBA48 instantiation's element
        // math). Kept from the earlier slice for the .cpp's instantiation anchor.
        typedef Array<NearestCarInfo, 8u> NearestCarInfoArray;

        // ---- DWARF :54-:103 -- declared-only (their own ledger functions) ----
        // ⭐ Construct @0x8221D760 -- BODIED INLINE 2026-08-01 (junkyard-fire wave), in the
        // header because BrnDirectorAllVehicleData.cpp IS NOT ON THE BUILD LIST (see that
        // file's own banner) and every consumer of this class is a header consumer.
        // The console body builds three 64-byte matrix images on the stack out of two rodata
        // floats -- flt_82001C98 on the diagonal, flt_82001CC0 elsewhere -- and stvx128's them
        // into +0x00 / +0x40 / +0x80, i.e. the three player spaces are seeded to IDENTITY;
        // then it clears the pointer, index, bitset, array and flag fields (`stw r11(0), 0xD0`
        // and the rest of the zero stores).
        void Construct()
        {
            mPlayerImpactSpace.SetIdentity();
            mPlayerHeadingSpace.SetIdentity();
            mPlayerLooseHeadingSpace.SetIdentity();

            mpRaceCars                       = 0;
            mePlayerRaceCarIndex             = static_cast<EActiveRaceCarIndex>(0);
            mUsedRaceCars.UnSetAll();
            mpTrafficVehicleArray            = 0;
            maNearestRaceCarsToPlayer.Clear();
            mbSorteddNearestRaceCarsToPlayer = false;
            mbShouldUpdateNearestRaceCars    = true;
        }

        // ====================================================================================
        // ⭐⭐ AllVehicleData::Update @0x8221D938 -- THE CONSOLE'S WHOLE BODY since 2026-09-25 (crash
        // parity FX-DIRECTOR2). It replaces UpdateRaceCarsBringUp, the extracted leg that had to run
        // without the last two arguments because the director input carried neither.
        //
        // SIGNATURE. MainDirector::PreSceneQueryUpdate passes FIVE arguments (0x8225BC54..0x8225BC70:
        // r4..r8), one more than the PS3 DWARF's :68 declaration. The fifth is the per-car team array:
        // the body stores r8 to a stack slot at entry (0x8221D948) and reloads it for the nearest-car
        // rows under its own tripwire "lpaVehicleTeams" (:134, 0x8221DE80..0x8221DE9C).
        //
        // THE BODY, in the console's order:
        //   0x8221D95C  stw r5, 0xC0   mpRaceCars           = lpRaceCars
        //   0x8221D960  stw r6, 0xC4   mePlayerRaceCarIndex = lePlayerIndex
        //   0x8221D96C  std r4, 0xC8   mUsedRaceCars        = lUsedRaceCars   (a 64-bit store)
        //   0x8221D974  assert "lpTrafficVehicleArray != NULL" (:66)
        //   0x8221D998  stw r30, 0xD0  mpTrafficVehicleArray = lpTrafficVehicleArray
        //   0x8221D994..0x8221D9E4  asserts "mpRaceCars != NULL" (:69), index < KI_MAX_VEHICLES (:70)
        //   0x8221D9E8..0x8221DDD0  the three player reference spaces (UpdatePlayerSpaces below)
        //   0x8221DDB0/0x8221DDD4  `li r10, 1 ; stb r10, 0x139` mbShouldUpdateNearestRaceCars = true
        //   0x8221DDD8  `stw 0, 0x134`  the nearest-car table emptied
        //   0x8221DDE0..0x8221E0DC  for every SET bit of mUsedRaceCars, ascending (a cntlzd scan):
        //               GetRaceCar(car) and GetPlayer() (both with their asserts), the squared
        //               distance (`vsubfp player - car ; vmsum3fp128`), the team
        //               `lwzx r10, 4 * car, lpaVehicleTeams` (0x8221DECC / 0x8221DEE4), and
        //               NearestCarInfo<8>::Append @0x821FBA48 of {car, distance, team}.
        //               THE PLAYER IS NOT SKIPPED: row 0 after the sort is the player at distance 0,
        //               which is why GetSqDistanceOfNearestCarToPlayer reads row 1.
        //   0x8221E0E0  `stb 0, 0x138`  mbSorteddNearestRaceCarsToPlayer = false
        // ====================================================================================
        void Update(CgsContainers::BitArray<8u> lUsedRaceCars,
                    const Camera::VehicleInfo*  lpRaceCars,
                    EActiveRaceCarIndex         lePlayerIndex,
                    const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpTrafficVehicleArray,
                    const u32*                  lpaVehicleTeams)
        {
            mpRaceCars           = lpRaceCars;
            mePlayerRaceCarIndex = lePlayerIndex;
            mUsedRaceCars        = lUsedRaceCars;

            CGS_ASSERT(lpTrafficVehicleArray != 0, "lpTrafficVehicleArray != NULL");              // h:66
            mpTrafficVehicleArray = lpTrafficVehicleArray;

            CGS_ASSERT(mpRaceCars != 0, "mpRaceCars != NULL");                                  // h:69
            CGS_ASSERT(static_cast<s32>(mePlayerRaceCarIndex) < 8,
                       "mePlayerRaceCarIndex < BrnPhysics::Vehicle::KI_MAX_VEHICLES");          // h:70

            UpdatePlayerSpaces();

            mbShouldUpdateNearestRaceCars = true;
            maNearestRaceCarsToPlayer.Clear();                       // console `stw 0, +0x134`

            for (u32 luCar = 0; luCar < 8u; ++luCar)
            {
                if (!mUsedRaceCars.IsBitSet(luCar))
                {
                    continue;
                }

                CGS_ASSERT(lpaVehicleTeams != 0, "lpaVehicleTeams");                         // h:134

                const Vector3& lrCarPos =
                    GetRaceCar(static_cast<EActiveRaceCarIndex>(luCar)).mRaceCarState.mTransform.wAxis;
                const Vector3& lrPlayerPos = GetPlayer().mRaceCarState.mTransform.wAxis;

                // `vsubfp` (player - car) then `vmsum3fp128` -- the SQUARED distance over x/y/z.
                const f32 lfDx = lrPlayerPos.x - lrCarPos.x;
                const f32 lfDy = lrPlayerPos.y - lrCarPos.y;
                const f32 lfDz = lrPlayerPos.z - lrCarPos.z;

                NearestCarInfo lRow;
                lRow.meRaceCarIndex = static_cast<EActiveRaceCarIndex>(luCar);
                lRow.mfDistance     = (lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz);
                lRow.miTeam         = static_cast<s32>(lpaVehicleTeams[luCar]);   // lwzx @0x8221DEE4

                maNearestRaceCarsToPlayer.Append(lRow);
            }

            mbSorteddNearestRaceCarsToPlayer = false;
        }

        // ====================================================================================
        // ⭐⭐ UpdatePlayerSpaces -- THE FIRST STAGE of AllVehicleData::Update @0x8221D938
        // (0x8221D9E8..0x8221DDD8), which the old UpdateRaceCarsBringUp extraction had never
        // taken. Added 2026-09-06, bug-test wave, lane `drivethru`. Update above calls it in the
        // console's position (2026-09-25), before the nearest-car rebuild.
        //
        // ⛔⛔ WHAT WAS WRONG, AND IT IS MEASURED, NOT ARGUED. Construct() seeds
        // mPlayerImpactSpace / mPlayerHeadingSpace / mPlayerLooseHeadingSpace to IDENTITY and
        // NOTHING in this tree ever wrote them again. Those three matrices are three of the
        // eight MainDirector::BuildBehaviourSharedInfo stages into every frame's
        // ICE::CameraSpaceHandler (mImpactToWorld / mHeadingToWorld / mLooseHeadingToWorld), and
        // ICE::CameraSpaceHandler::TransformToWorld @0x82533DE8 projects an authored eye/look
        // point through them. Through an IDENTITY matrix the projection is the identity: a
        // CAR-RELATIVE offset comes out as an ABSOLUTE WORLD POSITION a few metres from the
        // world origin.
        //   BurnoutDecomp/b5-decomp#6, body shop, RED run 20260906_100326:
        //     [ice]    eyeSpace 10 lookSpace 10 rawEye (-2.548, 0.079, -2.942)
        //                                        -> world (-2.548, 0.079, -2.942)
        //     [dt-cam] cam=(-2.548,0.079,-2.942) car=(917.011,22.506,-1378.163)
        //              dy=-22.426 dist=1654.486
        //   203 of 203 sampled drive-thru frames: the camera 1654-1694 m from the car and 22-24 m
        //   BELOW it -- "the camera goes to ground level and the car is not in frame".
        //   eICE_HEADING_SPACE (10) is what the shop take is authored in, for BOTH eye and look.
        //
        // ⭐ THE CONSOLE BODY, read off the ARTIST asm (the pseudocode renders the whole block
        // as inline __asm and names nothing):
        //
        //   0x8221D9E8  lbz r11, 0x44A(GetPlayer())            == mRaceCarState.mbCrashing
        //               bne -> 0x8221DCD0                      -- the IMPACT space FREEZES while
        //                                                         the player is crashing: that is
        //                                                         what makes it an impact frame.
        //   0x8221DA14  lbz r11, 0x1E8(GetPlayer())            == mAboveGroundTestResult.mbValid
        //                                                         (RaceCarState +448 +40)
        //     not valid -> 0x8221DCC0: refresh ONLY the translation row.
        //     valid     -> 0x8221DA28..0x8221DAF8, in this order:
        //                    +0x30 wAxis = mTransform.wAxis
        //                    +0x10 yAxis = mAboveGroundTestResult.mIntersectionNormal (+448 +16)
        //                    +0x20 zAxis = mTransform.zAxis
        //                    +0x00 xAxis = Normalize(Cross(yAxis, zAxis))
        //                    +0x20 zAxis = Cross(xAxis, yAxis)          (re-orthogonalised)
        //                  The two crosses are the standard `vpermwi128 ..., 0x63` (yzx swizzle)
        //                  pair `perm(a*perm(b) - perm(a)*b)`; the normalise is vrsqrtefp plus
        //                  two Newton-Raphson steps (written as the exact Normalize, the standing
        //                  convention of the rwmath vendor home).
        //   0x8221DAFC..0x8221DC8C  a four-row x three-lane `vcmpeqfp.` self-equality cascade,
        //                  ANDed; if ANY lane is NaN the console DISCARDS the frame it just built
        //                  and copies the car transform verbatim into all four rows
        //                  (0x8221DC90). rw::math::vpu::IsValid(Matrix44Affine) IS that cascade.
        //   0x8221DCD0..0x8221DDD0  the two heading frames. Both chase the SAME target: a look-at
        //                  from the car position along its FLATTENED forward axis. The vperm
        //                  control at unk_82CDA350 is {00 01 02 03 | 14 15 16 17 | 00 01 02 03 |
        //                  00 01 02 03}, i.e. splat(zAxis.x) in lanes 0/2/3 and ZERO in lane 1,
        //                  and the following `vrlimi128 v0, splat(zAxis.z), 2, 0` restores
        //                  zAxis.z in the Z lane -- so the vector added to the car position is
        //                  (zAxis.x, 0, zAxis.z). The console re-seats the FROM matrix's
        //                  translation row to the car position before each blend
        //                  (`stvx128 v0, r31, 0x70` / `..., 0xB0`).
        //
        // ⚠️ THE TWO BLEND RATES WERE SILENT-ZERO .data SLOTS (AGENTS.md rule 8). A literal read
        // of the image at unk_82FAA6D0 / unk_82FAA950 gives 0.0 -- which would make SLerp the
        // identity and freeze both frames for ever. They are written by CRT init thunks:
        //     0x82C48560  lfs f0, flt_82004744 (0.20) ; vspltw ; stvx128 -> 0x82FAA6D0  (heading)
        //     0x82C48538  lfs f0, flt_8200D528 (0.07) ; vspltw ; stvx128 -> 0x82FAA950  (loose)
        // Located with a lis/@l pair sweep of the image; every OTHER site whose low half is
        // 0xA6D0/0xA950 resolves to a different symbol (checked, not assumed).
        //
        // ORDER: the console runs this stage BEFORE the nearest-car rebuild, and Update above now
        // does too (the old extraction called it last -- harmless, since neither stage reads the
        // other's output, but no longer needed).
        // ====================================================================================
        void UpdatePlayerSpaces()
        {
            // The heading frame's per-frame blend towards the flattened look-at (unk_82FAA6D0).
            const f32 KF_HEADING_SPACE_BLEND = 0.20f;
            // The lagged flavour of the same frame (unk_82FAA950).
            const f32 KF_LOOSE_HEADING_SPACE_BLEND = 0.07f;

            const Camera::VehicleInfo& lrPlayer = GetPlayer();
            const Matrix44Affine&      lrCarToWorld = lrPlayer.mRaceCarState.mTransform;

            // ---- mPlayerImpactSpace ------------------------------------------------------
            if (!lrPlayer.mRaceCarState.mbCrashing)
            {
                const BrnPhysics::Vehicle::AboveGroundTestResult& lrGround =
                    lrPlayer.mRaceCarState.mAboveGroundTestResult;

                if (lrGround.mbValid)
                {
                    mPlayerImpactSpace.wAxis = lrCarToWorld.wAxis;
                    mPlayerImpactSpace.yAxis = lrGround.mIntersectionNormal;
                    mPlayerImpactSpace.zAxis = lrCarToWorld.zAxis;
                    mPlayerImpactSpace.xAxis = rw::math::vpu::Normalize(
                        rw::math::vpu::Cross(mPlayerImpactSpace.yAxis, mPlayerImpactSpace.zAxis));
                    mPlayerImpactSpace.zAxis = rw::math::vpu::Cross(
                        mPlayerImpactSpace.xAxis, mPlayerImpactSpace.yAxis);

                    if (!rw::math::vpu::IsValid(mPlayerImpactSpace))
                    {
                        mPlayerImpactSpace = lrCarToWorld;
                    }
                }
                else
                {
                    mPlayerImpactSpace.wAxis = lrCarToWorld.wAxis;
                }
            }

            // ---- mPlayerHeadingSpace / mPlayerLooseHeadingSpace --------------------------
            const Vector3 lvCarPosition = lrCarToWorld.wAxis;
            const Vector3 lvFlattenedTarget =
            {
                lvCarPosition.x + lrCarToWorld.zAxis.x,
                lvCarPosition.y,
                lvCarPosition.z + lrCarToWorld.zAxis.z,
                0.0f
            };
            const Matrix44Affine lHeadingLookAt =
                Camera::Utils::CreateLookAt(lvCarPosition, lvFlattenedTarget);

            // SLerp's fourth argument is its remaining-rotation OUT parameter; the console
            // passes a stack slot it never reads (the DWARF names such a local lUnusedAngle).
            Vector3 lUnusedAngle;

            mPlayerHeadingSpace.wAxis = lvCarPosition;
            mPlayerHeadingSpace = rw::math::vpu::SLerp(
                mPlayerHeadingSpace, lHeadingLookAt, KF_HEADING_SPACE_BLEND, &lUnusedAngle);

            mPlayerLooseHeadingSpace.wAxis = lvCarPosition;
            mPlayerLooseHeadingSpace = rw::math::vpu::SLerp(
                mPlayerLooseHeadingSpace, lHeadingLookAt, KF_LOOSE_HEADING_SPACE_BLEND,
                &lUnusedAngle);
        }

        // ⭐ GetPlayer @0x82205C58 / GetRaceCar @0x82205DE8 -- BODIED INLINE HERE, which is
        // where the CONSOLE had them: every assert in both cites this header
        // (BrnDirectorAllVehicleData.h :157/:158/:159 and :170/:171/:172), and a function whose
        // asserts cite a header was defined in that header. Each is three asserts then one
        // indexed read at the VehicleInfo stride (`mulli rN, rIdx, 0x4F0` + the mpRaceCars
        // base). ⚠️ The two index guards are deliberately DIFFERENT comparisons: the explicit
        // range assert is a SIGNED `cmpwi`, while the third is IsBitSet's OWN inlined
        // CgsBitArray.h:203 tripwire, an UNSIGNED `cmplwi`.
        const Camera::VehicleInfo& GetPlayer() const                             // :57
        {
            CGS_ASSERT(mpRaceCars != 0, "mpRaceCars != NULL");                                    // h:157
            CGS_ASSERT(static_cast<s32>(mePlayerRaceCarIndex) < 8, "mePlayerRaceCarIndex < 8");   // h:158
            CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(mePlayerRaceCarIndex)),
                       "mUsedRaceCars.IsBitSet(mePlayerRaceCarIndex)");                           // h:159
            return mpRaceCars[static_cast<s32>(mePlayerRaceCarIndex)];
        }
        const Camera::VehicleInfo& GetRaceCar(EActiveRaceCarIndex leIndex) const  // :61
        {
            CGS_ASSERT(mpRaceCars != 0, "mpRaceCars != NULL");                                    // h:170
            CGS_ASSERT(static_cast<s32>(leIndex) < 8, "leRaceCarIndex < 8");                      // h:171
            CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(leIndex)),
                       "mUsedRaceCars.IsBitSet(leRaceCarIndex)");                                 // h:172
            return mpRaceCars[static_cast<s32>(leIndex)];
        }

        // Update (:68) is bodied inline above, with the console's fifth argument.
        const Camera::VehicleInfo& GetNearestRaceCarToPlayer(u32 luRank) const;   // :75

        // ⭐ @0x82233380 -- BODIED INLINE HERE for the same reason: its own assert cites
        // BrnDirectorAllVehicleData.h:186. (It was previously bodied out-of-line in the
        // .cpp, which is NOT mounted, so every consumer saw it as an unresolved external.)
        // ⚠️ The clamp happens BEFORE the sort and the range compare is UNSIGNED (`cmplw`), so
        // a rank at or past the live count collapses onto the LAST row rather than wrapping.
        // The extra "Array used before Construct/Clear was called" (CgsArray.h:336) asserts in
        // the asm are GetLength()'s own inlined tripwire, one per call -- not separate logic.
        EActiveRaceCarIndex GetNearestRaceCarIndexToPlayer(u32 luRank) const      // :82
        {
            CGS_ASSERT(maNearestRaceCarsToPlayer.GetLength() > 0,
                       "maNearestRaceCarsToPlayer.GetLength() > 0");                              // h:186

            if (luRank >= maNearestRaceCarsToPlayer.GetLength())
            {
                luRank = maNearestRaceCarsToPlayer.GetLength() - 1u;
            }

            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }

            return maNearestRaceCarsToPlayer.GetItem(luRank).meRaceCarIndex;
        }
        // ⭐ BODIED INLINE 2026-08-01 (ICE-anim transform wave). None of the three has a
        // standalone X360 export -- the console inlines all three, and the ONE consumer that
        // proves what they hand back is MainDirector::UpdateCameraBehavioursPostScene
        // @0x8224FD30, which stages the ICE camera-space handler's impact / heading /
        // loose-heading matrices with four `lvx128/stvx128` pairs read straight off
        // `mAllVehicleData + 0x00`, `+ 0x40` and `+ 0x80` (0x8224FFC8..0x82250000). Those are
        // exactly the three members below, in this order, so each getter is the one-line
        // by-value member return the DWARF return type already declares. Nothing is fabricated.
        Matrix44Affine GetPlayerImpactSpace() const       { return mPlayerImpactSpace; }       // :85
        Matrix44Affine GetPlayerHeadingSpace() const      { return mPlayerHeadingSpace; }      // :88
        Matrix44Affine GetPlayerLooseHeadingSpace() const { return mPlayerLooseHeadingSpace; } // :91
        // :94 -- the traffic records Update stored. No standalone X360 export: its two readers,
        // VehicleCollisionPredictor::Update @0x822230D8 and FrustrumCollisionResolver::
        // ResolveVehicleCollisions @0x82223890, each load `lwz 0xD0` off the AllVehicleData
        // pointer themselves (0x822230FC / 0x822238C0) and assert it non-null.
        const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* GetTraffic() const  // :94
        {
            return mpTrafficVehicleArray;
        }
        // [PC diag accessor, NOT in the DWARF] the nearest-car rows as Update built them (car,
        // squared distance, team) -- read only by MainDirector's BRN_DIRECTOR_TRAFFIC_DIAG witness.
        const NearestCarInfoArray& GetNearestRaceCarRowsForDiag() const
        {
            return maNearestRaceCarsToPlayer;
        }
        // BODIED INLINE 2026-08-01, same one-line member read as the two accessors below it
        // (no standalone X360 export; the console inlines every reach of mpRaceCars).
        const Camera::VehicleInfo* GetRaceCars() const { return mpRaceCars; }     // :97
        // BODIED INLINE (2026-07-30). Neither of these two has a standalone X360 export --
        // the console inlines both at every call site (e.g. VehicleRef::IsValid @0x822336A8
        // reads *(world+196) and the 64-bit used-race-car word at world+200 directly, which is
        // exactly mePlayerRaceCarIndex @+0xC4 and mUsedRaceCars @+0xC8 below). An inline
        // one-line member read IS the console shape; nothing is fabricated.
        const CgsContainers::BitArray<8u>& GetUsedRaceCarsBitArray() const        // :100
        {
            return mUsedRaceCars;
        }
        EActiveRaceCarIndex GetPlayerRCIndex() const                             // :103
        {
            return mePlayerRaceCarIndex;
        }

        // ====================================================================
        // ⭐ MOVED HERE FROM BrnDirectorAllVehicleData.cpp (2026-08-01), for the SAME reason
        // the five siblings above were moved: that .cpp is NOT on the build list, so both
        // bodies -- written, and (re)verified against the X360 asm this wave -- were
        // unreachable, and every consumer of them saw an unresolved external for finished
        // code. (Sixth instance of that pattern in this project.)
        //
        // ⚠️ HONEST PROVENANCE, because rule 12's usual test does NOT decide these two.
        // Neither function contains an assert that cites a source file of its own:
        // GetSqDistanceOfNearestCarToPlayer has no assert at all, and the single assert
        // inside SqDistanceOfNearestOpposingTeamMember is GetLength()'s own inlined
        // CgsArray.h:336 tripwire. The header home rests instead on the DecFIGS DWARF's
        // file split for this class: BrnDirectorAllVehicleData.cpp contains EXACTLY TWO
        // definitions there (Construct @cpp:36 and Update @cpp:59) -- every one of the
        // class's twelve accessors is header-defined. Both queries are accessors of that
        // same shape (lazily sort, then read one row), and neither appears anywhere in
        // the PS3 DWARF (they are X360-revision additions), so no DWARF line cites a
        // .cpp for them either. Inline here is behaviourally identical to out-of-line and
        // costs the link nothing; it needs no build-list change.
        // ====================================================================

        // @0x82233488 -- the squared distance of the nearest OTHER car (sorted row 1; row 0
        // is the player itself). VERIFIED store-for-store against the X360 asm this wave:
        //   lbz +0x138 -> if unsorted: BubbleSort(&maNearest.. @+0xD4), stb 1 @+0x138;
        //   then operator[](array, 1) and `lfs f1, 4(r3)` == the row's mfDistance.
        // CONST + mutable sort state: the committed consumers reach these through a
        // `const AllVehicleData*` (ArbStateSharedInfo +0x38), so the lazy first-use sort is
        // modelled with mutable members.
        f32 GetSqDistanceOfNearestCarToPlayer() const
        {
            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }
            return maNearestRaceCarsToPlayer.GetItem(1u).mfDistance;
        }

        // @0x822334E0 -- the squared distance of the nearest car on a different team, or
        // FLT_MAX when every listed car shares liMyTeam.
        //
        // ⚠️ CORRECTED THIS WAVE: the count was read with GetCount(), which is the RAW
        // count accessor and fires no tripwire. The asm re-reads the count word at array
        // +0x60 EVERY iteration and each read carries the "Array used before
        // Construct/Clear was called" assert at CgsArray.h line 336 (`li r5, 0x150`) --
        // that is GetLength()'s inlined tripwire, not GetCount()'s (which has none). The
        // file's own comment already claimed the tripwire re-fired per iteration while the
        // code did not do it. GetLength() also returns u32, matching the console's UNSIGNED
        // `cmplw` bound test, where the old `static_cast<u32>(GetCount())` was a cast.
        //
        // The rest is VERIFIED as it stood: the team word is read at element +0x08 and
        // compared SIGNED (`cmpw`) against the argument; a mismatch re-enters operator[] a
        // SECOND time to read mfDistance (the console really does index twice); the
        // fall-through loads flt_8200173C.
        f32 SqDistanceOfNearestOpposingTeamMember(s32 liMyTeam) const
        {
            if (!mbSorteddNearestRaceCarsToPlayer)
            {
                CgsAlgorithms::BubbleSort(maNearestRaceCarsToPlayer);
                mbSorteddNearestRaceCarsToPlayer = true;
            }

            u32 luRow = 1;
            while (luRow < maNearestRaceCarsToPlayer.GetLength())   // CgsArray.h:336 tripwire per pass
            {
                if (maNearestRaceCarsToPlayer.GetItem(luRow).miTeam != liMyTeam)
                {
                    return maNearestRaceCarsToPlayer.GetItem(luRow).mfDistance;
                }
                ++luRow;
            }
            return FLT_MAX;   // flt_8200173C
        }

    private:
        // DWARF :121-:139 order (Matrix44Affine members keep the class 16-aligned).
        Matrix44Affine mPlayerImpactSpace;         // :121  X360 +0x00
        Matrix44Affine mPlayerHeadingSpace;        // :122  +0x40
        Matrix44Affine mPlayerLooseHeadingSpace;   // :123  +0x80
        const Camera::VehicleInfo* mpRaceCars;     // :125  +0xC0 (stride 0x4F0 == sizeof(VehicleInfo))
        EActiveRaceCarIndex mePlayerRaceCarIndex;  // :126  +0xC4
        CgsContainers::BitArray<8u> mUsedRaceCars; // :127  +0xC8
        const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>*
                       mpTrafficVehicleArray;      // :129  +0xD0
        // mutable: the two const distance queries lazily sort on first use (the
        // consumers hold a const pointer; see the query comment above).
        mutable NearestCarInfoArray
                       maNearestRaceCarsToPlayer;  // :133  +0xD4 (count word @+0x134)
        mutable bool   mbSorteddNearestRaceCarsToPlayer;   // :134  +0x138 (DWARF spelling)
        bool           mbShouldUpdateNearestRaceCars;      // :139
    };

    // Pin the X360 12-byte element (kept from the earlier slice; the Append
    // @0x821FBA48 asm copies exactly three 32-bit words at a 12-byte stride).
    static_assert(sizeof(AllVehicleData::NearestCarInfo) == 12, "NearestCarInfo is a 12-byte element");

    // ------------------------------------------------------------------------------------
    // ⭐ WHAT GetRaceCar HANDS BACK, PINNED. Added this wave because two consumer families
    // reach the returned record for the car's WORLD POSITION and had been parked waiting for
    // the return type to stop being an opaque `const void*`. It is not opaque and has not
    // been since the namespace fork above was fixed: the record IS
    // BrnDirector::Camera::VehicleInfo, its own committed home, and the position is reached
    // BY NAME as `GetRaceCar(leIndex).mRaceCarState.mTransform.wAxis`.
    //
    // The console leaves no room for doubt -- GetRaceCar's whole body, after its three
    // asserts, is one indexed add off the mpRaceCars base at +0xC0:
    //     lwz   r11, 0xC0(this)          ; mpRaceCars
    //     mulli rN,  rIndex, 0x4F0       ; the element stride
    //     add   r3,  rN, r11             ; &mpRaceCars[leIndex]
    // so the element stride IS sizeof(VehicleInfo) and the returned address IS the array
    // element -- there is no second record type and no director-side copy.
    //
    // The two offsets the position readers care about, in the record's own terms:
    //     +0x000  mRaceCarState                     (the physics publish, first member)
    //     +0x1F0  mRaceCarState.mTransform          (the car-to-world frame)
    //     +0x220  mRaceCarState.mTransform.wAxis    (the world POSITION lane)
    // These are the same offsets UpdatePlayerSpaces above already reads through this class
    // (the +0x44A crashing byte and the +0x1E8 ground-test valid byte land in the same
    // record), so the two agree by construction.
    //
    // Asserted rather than commented because the host record carries no pointers anywhere in
    // its embedded types, so the console strides survive the move to host widths unchanged --
    // and if a future edit ever breaks that, this fires at compile time instead of silently
    // handing every position reader the wrong lane.
    // ------------------------------------------------------------------------------------
    static_assert(sizeof(Camera::VehicleInfo) == 0x4F0,
                  "GetRaceCar indexes mpRaceCars at the VehicleInfo stride (mulli 0x4F0)");
    static_assert(offsetof(Camera::VehicleInfo, mRaceCarState) == 0x0,
                  "VehicleInfo leads with mRaceCarState");
    static_assert(offsetof(BrnPhysics::Vehicle::RaceCarState, mTransform) == 0x1F0,
                  "the returned race car's car-to-world frame sits at +0x1F0");
    static_assert(offsetof(Matrix44Affine, wAxis) == 0x30,
                  "the world position is the frame's wAxis lane (record +0x220)");
}
