// ============================================================================
// BrnTrafficEntityModule_ProcessDeformationData.cpp -- the physics -> traffic DEFORMATION
// readback (traffic-deformation wave, 2026-09-02).
//
//   TrafficEntityModule::ProcessDeformationData   @0x8271DEB0 (883)   COMPLETE
//   BrnTraffic::SetGlassFractureConstants         @0x82714848 (53)    COMPLETE
//
// Source of truth: the X360 ARTIST asm (every offset below is the console's), the PS3 DecFIGS
// twin @0x960C84 for the signature and the two constant NAMES (its plain-AltiVec body is the
// same code: `vmaddfp` with a zero addend where the X360 has `vmulfp128`), and the DWARF for
// every member name. The Feb-2007 leak predates traffic deformation and has no such function.
//
// WHAT IT DOES. The deformation system publishes, per frame and per deformable model, a
// parallel set of tables in DeformationOutputInterfaceForEntityModules -- skin (verlet)
// offsets, wheel physical states, light-locator tables -- plus a detached-part event queue and
// a glass smash/crack event queue. The race car's ReadUpdatedActiveRaceCarDataFromPhysics
// consumes the RACECAR-owned entries (legs L2..L6); THIS is the traffic module's consumer of
// the TRAFFIC_VEHICLE-owned ones, and it writes everything into the promoted car's
// TrafficPhysicsInfo, which RenderTrafficCar reads. Without it a traffic car's physics body
// dents (its sensors move) but the skinned mesh never receives the offsets -- "traffic doesn't
// really deform".
//
// CONSOLE SHAPE (one function, five legs, in this order):
//   A. StartMonitor(miPerfMon_ProcessDeformation)                            0x8271DEF0
//   B. for i in [0, miNumSkinnedModels):                                     0x8271DFF0..0x8271E970
//        B1. skin entry i owned by a traffic vehicle and that vehicle IsPhysical():
//            copy 128 offsets into maSkinningOffsets_Scratch, lane-wise max;   0x8271E240..0x8271E280
//            if IsAlive(): fatal test against the type's bbox               0x8271E284..0x8271E340
//        B2. base id i owned by a traffic vehicle, IsPhysical(), parts index >= 0:
//            four wheel transforms + mabWheelExists; exists && !attached => fatal
//                                                                           0x8271E430..0x8271E5FC
//        B3. locator entry i owned by a traffic vehicle and IsPhysical():
//            miNumLightLocators, the light-locator positions and tag types   0x8271E8E8..0x8271E958
//   C. zero every record's detached-part count                              0x8271E974..0x8271E990
//   D. detached-part render events -> the owning record's compact queue     0x8271E99C..0x8271EB20
//   E. glass smash/crack events -> mafGlassPaneFractureAmounts + flags       0x8271EB24..0x8271EC54
//   F. StopMonitor                                                           0x8271EC60
//
// THE TWO CONSTANTS. unk_8300CF80 / unk_8300CB10 are .bss vectors written by two dyn-init
// thunks in the XEX2 basefile (the same recovery the render TU's four constants went
// through): @0x82C689E8 and @0x82C68A10 each `lfs f0, 0x4014(0x82000000)` -> `vspltw` ->
// `stvx128` to their .bss slot, and 0x82004014 reads 0x3DCCCCCD == 0.1f. The PS3 names them
// BrnTraffic::KF_MAX_DEFORMATION_FOR_FATAL_WIDTH_PERC / _LENGTH_PERC. Both are 0.1 -- a
// traffic car whose largest skin offset reaches a tenth of its full width (x) or length (z)
// is fatally crashing.
//
// WHAT IS DELIBERATELY NOT HERE. Nothing: every store the asm makes has a line below. The
// assert texts are the console's (the streamed "Traffic vehicle N is physical but has no
// parts data" / "invalid index : N < 25" messages are spelled as their static prefixes; the
// CGS_ASSERT macro carries no formatter).
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // DeformationOutputInterfaceForEntityModules, SkinData, VehicleLocatorOutput, the two queues
#include "GameSource/Physics/DeformationManager/SharedIO/BrnVehicleLocatorData.h"        // VehicleLocatorData
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"           // WheelPhysicalStates
#include "GameSource/World/BrnEntityTypes.h"                                             // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"                          // ShaderConstantTable
#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"                                               // rw::math::vpu::Max
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // CgsDev::Log::gpDebugPrint ([tdef] witness, opt-in)

#include <cstdlib>   // getenv / atoi ([tdef] witness)
#include <cmath>     // sqrtf ([tdef] witness)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied by the
// CgsShaderConstants TU). Same extern the traffic render TU carries.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

namespace BrnTraffic
{

// ----------------------------------------------------------------------------------------
// FILE-SCOPE CONSTANTS
// ----------------------------------------------------------------------------------------

// The two fatal-deformation fractions (see the file banner for the recovery). PS3 DecFIGS
// names; X360 .bss unk_8300CF80 (width, lane x) and unk_8300CB10 (length, lane z), each a
// splat of flt_82004014 == 0.1f.
static const f32 KF_MAX_DEFORMATION_FOR_FATAL_WIDTH_PERC  = 0.1f;
static const f32 KF_MAX_DEFORMATION_FOR_FATAL_LENGTH_PERC = 0.1f;

// Scene EntityId geometry, exactly as every reader in this module spells it
// (`srwi r10, id, 24` for the owner byte; `extrwi idx, id, 14, 8` == (id >> 10) & 0x3FFF).
static const u32 KU_ENTITY_TYPE_SHIFT  = 24u;
static const u32 KU_ENTITY_INDEX_SHIFT = 10u;
static const u32 KU_ENTITY_INDEX_MASK  = 0x3FFFu;

// The glass panes are body parts 16..23 (`addi r31, part, -0x10 ; cmplwi 8`).
static const s32 KI_FIRST_GLASS_BODY_PART = 16;

// The value a SMASHED pane's fracture amount takes (flt_820BA86C == 2.0f, held in f29 across
// the glass loop) and the CRACKED pane's ceiling (flt_82001C98 == 1.0f, f30).
static const f32 KF_GLASS_SMASHED_FRACTURE_AMOUNT = 2.0f;
static const f32 KF_GLASS_CRACKED_FRACTURE_MAX    = 1.0f;

// ----------------------------------------------------------------------------------------
// BrnTraffic::SetGlassFractureConstants  @0x82714848 (53 instructions)  -- COMPLETE
//
// The traffic module's copy of BrnWorld::SetGlassFractureConstants @0x822BD280: identical
// store-for-store (constant 30 = {(1-s)*inv, inv, 0, 0} with inv = s > 0 ? 1/s : 0; constant
// 31 = the UV-offset vector verbatim, held in v127; constant 32 = {uv.x, uv.x*eq, uv.y,
// uv.y*eq}). The fdivs/fmuls/fsubs are single precision; IDA renders the two f32 arguments
// as doubles.
// ----------------------------------------------------------------------------------------
void SetGlassFractureConstants( f32 lfFractureStrength,
                                f32 lfEqualisationFactor,
                                const Vector2& lvUVScale,
                                Vector4 lvUVOffsets )
{
    // 0x82714870 `fcmpu cr6, f1, 0.0 ; ble` -> the 0.0 arm, else `fdivs f13, 1.0, f1`.
    const f32 lfInverseStrength =
        ( lfFractureStrength > 0.0f ) ? ( 1.0f / lfFractureStrength ) : 0.0f;

    const Vector4 lv4Fracture = { ( 1.0f - lfFractureStrength ) * lfInverseStrength,
                                  lfInverseStrength,
                                  0.0f,
                                  0.0f };
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 30, lv4Fracture );

    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 31, lvUVOffsets );

    const Vector4 lv4UVScale = { lvUVScale.x,
                                 lvUVScale.x * lfEqualisationFactor,
                                 lvUVScale.y,
                                 lvUVScale.y * lfEqualisationFactor };
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 32, lv4UVScale );
}

// ----------------------------------------------------------------------------------------
// TrafficEntityModule::ProcessDeformationData  @0x8271DEB0 (883 instructions)  -- COMPLETE
// ----------------------------------------------------------------------------------------
void TrafficEntityModule::ProcessDeformationData(
    const BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules* lpDefInterface )
{
    using BrnPhysics::Deformation::SkinData;
    using BrnPhysics::Deformation::VehicleLocatorData;
    using BrnPhysics::Deformation::VehicleLocatorOutput;
    using BrnPhysics::Deformation::WheelPhysicalStates;
    using BrnPhysics::Deformation::DetachedPartRenderEvent;
    using BrnPhysics::Deformation::GlassSmashOrCrackEvent;
    using BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules;
    using BrnPhysics::Deformation::DeformationOutputInterface;

    // A. `lwz r3, 0x72A10(this) ; bl StartMonitor` -- module +469520 == miPerfMon_ProcessDeformation.
    CgsDev::PerfMonCpu::StartMonitor( miPerfMon_ProcessDeformation );

    // ====================================================================================
    // B. THE PER-MODEL PASS. One index walks three parallel tables of the interface: the
    // skin records (miNumSkinnedModels bounds the loop; the :276 assert is inside
    // GetSkinData), the wheel-state entries (maBaseIDs / maWheelStates) and the locator
    // records (the :292 assert is inside GetLocatorOutput). The producer, DeformationManager::
    // OutputData, pushes all three per model in the same order, which is what makes the
    // shared index valid.
    // ====================================================================================
    const s32 liNumSkinnedModels = lpDefInterface->GetNumSkinnedModels();
    for ( s32 liModel = 0; liModel < liNumSkinnedModels; ++liModel )
    {
        // ---- B1. THE SKIN OFFSETS (0x8271DFF0..0x8271E340) --------------------------------
        {
            const SkinData& lrSkin = lpDefInterface->GetSkinData( liModel );   // :276
            const u32 luSkinEntityWord = lrSkin.mEntityId.muValue;

            if ( ( luSkinEntityWord >> KU_ENTITY_TYPE_SHIFT ) == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
            {
                const u32 luVehicle = ( luSkinEntityWord >> KU_ENTITY_INDEX_SHIFT ) & KU_ENTITY_INDEX_MASK;
                CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" ); // header :2459

                const Vehicle* lpVehicle = GetVehicle( luVehicle );
                CGS_ASSERT( lpVehicle != 0, "lpVehicle" );                                       // :3145

                if ( lpVehicle->IsPhysical() )
                {
                    // The console reads the raw s8 here (`GetPhysicalPartsIndex` @0x8270F928
                    // returns it) and asserts on the sign itself -- the index is used below
                    // even when the assert fires, so it is read once and kept.
                    const s32 liPhysicalIndex = static_cast< s8 >( lpVehicle->GetPhysicalPartsIndex() );   // `extsb` after every call
                    CGS_ASSERT( liPhysicalIndex >= 0,
                                "Traffic vehicle  is physical but has no parts data" );         // :3149
                    CGS_ASSERT( static_cast< u32 >( liPhysicalIndex ) < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES,
                                "invalid index :  < 25" );                                       // CgsBitArray.h:203
                    CGS_ASSERT( maTrafficPhysicsInfoListBits.IsBitSet( static_cast< u32 >( liPhysicalIndex ) ),
                                "maTrafficPhysicsInfoListBits.IsBitSet( liPhysicalIndex )" );   // :3150

                    TrafficPhysicsInfo* lpPhysInfo = &maTrafficPhysicsInfoList[ liPhysicalIndex ];
                    CGS_ASSERT( lpPhysInfo != 0, "lpPhysInfo" );                                 // :3153

                    // 128 quadwords: `lvx128 v0, r11, src ; stvx128 v0, dst ; vmaxfp acc, acc, v0`
                    // over 0x800 bytes (0x8271E254..0x8271E280). The accumulator starts at
                    // `vspltisw v0, 0`.
                    const Vector3Plus* lpSrc =
                        static_cast< const Vector3Plus* >( lrSkin.mpSkinOffsets_Scratch );
                    Vector4 lv4MaxOffset = { 0.0f, 0.0f, 0.0f, 0.0f };
                    for ( u32 luPoint = 0; luPoint < TrafficPhysicsInfo::KU_NUM_SKINNING_OFFSETS; ++luPoint )
                    {
                        const Vector3Plus& lrOffset = lpSrc[ luPoint ];
                        lpPhysInfo->maSkinningOffsets_Scratch[ luPoint ] = lrOffset;
                        lv4MaxOffset = rw::math::vpu::Max(
                            lv4MaxOffset, Vector4{ lrOffset.x, lrOffset.y, lrOffset.z, lrOffset.w } );
                    }

                    // ---- [tdef] NOT X360; opt-in on BRN_DEFORM_TRACE. The physics side's
                    // reading of THIS car's skin block at the instant it lands in
                    // TrafficPhysicsInfo: max / sum / non-zero rows plus the fatal inputs.
                    // Printed only when the sum changes; pairs with the render TU's
                    // [tdef-upload] line for the same vehicle. DELETE-WHEN banked.
                    {
                        static s32 siTraceOn = -1;
                        if ( siTraceOn < 0 )
                        {
                            const char* lpcEnv = getenv( "BRN_DEFORM_TRACE" );
                            siTraceOn = ( lpcEnv != 0 && atoi( lpcEnv ) > 0 ) ? 1 : 0;
                        }
                        if ( siTraceOn == 1 && CgsDev::Log::gpDebugPrint != 0 )
                        {
                            static f32 sfLastSum = -1.0f;
                            static u32 sluLines  = 0;
                            f32 lfMax = 0.0f;
                            f32 lfSum = 0.0f;
                            s32 liNnz = 0;
                            for ( u32 luRow = 0; luRow < TrafficPhysicsInfo::KU_NUM_SKINNING_OFFSETS; ++luRow )
                            {
                                const Vector3Plus& lrRow = lpPhysInfo->maSkinningOffsets_Scratch[ luRow ];
                                const f32 lfMag = sqrtf( lrRow.x * lrRow.x + lrRow.y * lrRow.y + lrRow.z * lrRow.z );
                                if ( lfMag > lfMax ) { lfMax = lfMag; }
                                if ( lfMag > 0.0f ) { lfSum += lfMag; ++liNnz; }
                            }
                            if ( lfSum != sfLastSum && sluLines < 400u )
                            {
                                ++sluLines;
                                sfLastSum = lfSum;
                                *CgsDev::Log::gpDebugPrint
                                    << "[tdef] veh " << luVehicle
                                    << " phys " << liPhysicalIndex
                                    << " maxVerlet " << lfMax
                                    << " sumVerlet " << lfSum
                                    << " nnz " << liNnz
                                    << " laneMax (" << lv4MaxOffset.x << ", " << lv4MaxOffset.y
                                    << ", " << lv4MaxOffset.z << ")"
                                    << " deforming " << ( lpPhysInfo->mbIsDeforming ? 1 : 0 )
                                    << " fatal " << ( lpPhysInfo->mbIsFatallyCrashing ? 1 : 0 )
                                    << " alive " << ( lpVehicle->IsAlive() ? 1 : 0 )
                                    << "\n";
                            }
                        }
                    }

                    // `lbz r11, 5(vehicle) ; clrlwi 31` -- E_FLAG_ALIVE.
                    if ( lpVehicle->IsAlive() )
                    {
                        const VehicleTypeRuntime* lpVehicleRuntime =
                            GetVehicleTypeRuntime( lpVehicle->GetVehicleType() );
                        CGS_ASSERT( lpVehicleRuntime != 0, "lpVehicleRuntime" );                 // :3169

                        // `lvx128 v11, runtime, 0x10 ; vmulfp128 v0, v11, splat(2.0)` -- the
                        // type's full extent (twice mBBoxHalfSize, element +0x10).
                        const Vector3 lBBoxHalfSize = lpVehicleRuntime->GetBBoxHalfSize();
                        const f32 lfVehicleWidth  = lBBoxHalfSize.x * 2.0f;
                        const f32 lfVehicleLength = lBBoxHalfSize.z * 2.0f;

                        // Two `vcmpgefp.` on the splatted lanes; the first true one stores
                        // `stb 1, 0xFE6(info)` (0x8271E300 / 0x8271E32C / 0x8271E340).
                        if ( lv4MaxOffset.x >= lfVehicleWidth  * KF_MAX_DEFORMATION_FOR_FATAL_WIDTH_PERC
                          || lv4MaxOffset.z >= lfVehicleLength * KF_MAX_DEFORMATION_FOR_FATAL_LENGTH_PERC )
                        {
                            lpPhysInfo->mbIsFatallyCrashing = true;
                        }
                    }
                }
            }
        }

        // ---- B2. THE WHEEL STATES (0x8271E344..0x8271E5FC) --------------------------------
        // The owner test is on the wheel-state entry's base id (`ld r11, 8(entry) ; srdi 32 ;
        // srwi 24`), the index on the SKIN record's id (`lwz r11, 0(skin) ; extrwi 14,8`) --
        // the console leans on the two tables being parallel. Reproduced as written.
        {
            const u32 luBaseEntityWord =
                static_cast< u32 >( lpDefInterface->GetBaseId( static_cast< u32 >( liModel ) ) >> 32 );

            if ( ( luBaseEntityWord >> KU_ENTITY_TYPE_SHIFT ) == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
            {
                const u32 luSkinEntityWord = lpDefInterface->GetSkinData( liModel ).mEntityId.muValue;
                const u32 luVehicle = ( luSkinEntityWord >> KU_ENTITY_INDEX_SHIFT ) & KU_ENTITY_INDEX_MASK;
                CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" ); // header :2459

                const Vehicle* lpVehicle = GetVehicle( luVehicle );
                if ( lpVehicle->IsPhysical() )
                {
                    // Here the sign IS the branch (`cmpwi cr6, r3, 0 ; blt` -> skip), unlike B1.
                    const s32 liPhysicalIndex = static_cast< s8 >( lpVehicle->GetPhysicalPartsIndex() );   // `extsb` after every call
                    if ( liPhysicalIndex >= 0 )
                    {
                        TrafficPhysicsInfo* lpPhysInfo = &maTrafficPhysicsInfoList[ liPhysicalIndex ];
                        CGS_ASSERT( lpPhysInfo != 0, "lpPhysInfo" );                             // :3195

                        const WheelPhysicalStates* lpWheelStates =
                            static_cast< const WheelPhysicalStates* >(
                                lpDefInterface->GetWheelStateSlot( static_cast< u32 >( liModel ) ) );

                        for ( u32 luWheel = 0; luWheel < TrafficPhysicsInfo::KU_NUM_WHEELS; ++luWheel )
                        {
                            // Both bit-array tripwires are re-run PER WHEEL on the console
                            // (0x8271E430..0x8271E59C) -- the IsBitSet inline was not hoisted.
                            CGS_ASSERT( static_cast< u32 >( liPhysicalIndex ) < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES,
                                        "invalid index :  < 25" );                               // CgsBitArray.h:203
                            CGS_ASSERT( maTrafficPhysicsInfoListBits.IsBitSet( static_cast< u32 >( liPhysicalIndex ) ),
                                        "maTrafficPhysicsInfoListBits.IsBitSet(liPhysicalIndex)" ); // :3199

                            // Four rows from entry+0xF0+0x60*w -> info+0xD30+0x40*w.
                            lpPhysInfo->maWheelTransforms[ luWheel ] =
                                lpWheelStates->maStates[ luWheel ].mWorldSpaceTransform;

                            // `lbzx exists ; stbx info+0xFC8+w` then, if it exists,
                            // `lbz attached (entry+0x184+w) ; beq -> stb 1, 0xFE6(info)`.
                            const bool lbWheelExists = lpWheelStates->mabWheelExists[ luWheel ];
                            lpPhysInfo->mabWheelExists[ luWheel ] = lbWheelExists;
                            if ( lbWheelExists && !lpWheelStates->mabWheelAttached[ luWheel ] )
                            {
                                lpPhysInfo->mbIsFatallyCrashing = true;
                            }
                        }
                    }
                }
            }
        }

        // ---- B3. THE LIGHT LOCATORS (0x8271E600..0x8271E958) -------------------------------
        {
            const VehicleLocatorOutput& lrLocator = lpDefInterface->GetLocatorOutput( liModel ); // :292
            const u32 luLocatorEntityWord = lrLocator.mEntityId.muValue;

            if ( ( luLocatorEntityWord >> KU_ENTITY_TYPE_SHIFT ) == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
            {
                const u32 luVehicle = ( luLocatorEntityWord >> KU_ENTITY_INDEX_SHIFT ) & KU_ENTITY_INDEX_MASK;
                CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" ); // header :2459

                const Vehicle* lpVehicle = GetVehicle( luVehicle );
                if ( lpVehicle->IsPhysical() )
                {
                    const s32 liPhysicalIndex = static_cast< s8 >( lpVehicle->GetPhysicalPartsIndex() );   // `extsb` after every call
                    CGS_ASSERT( liPhysicalIndex >= 0,
                                "Traffic vehicle  is physical but has no parts data" );         // :3224
                    CGS_ASSERT( static_cast< u32 >( liPhysicalIndex ) < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES,
                                "invalid index :  < 25" );                                       // CgsBitArray.h:203
                    CGS_ASSERT( maTrafficPhysicsInfoListBits.IsBitSet( static_cast< u32 >( liPhysicalIndex ) ),
                                "maTrafficPhysicsInfoListBits.IsBitSet( liPhysicalIndex )" );   // :3225

                    TrafficPhysicsInfo*      lpPhysInfo    = &maTrafficPhysicsInfoList[ liPhysicalIndex ];
                    const VehicleLocatorData* lpLocatorData = lrLocator.mpLocatorData;

                    // `lwz r10, 0x6B0(data) ; stbx info+0xFE4` -- the s32 count into the s8.
                    const s32 liNumLightLocators = lpLocatorData->miNumLightLocators;
                    lpPhysInfo->miNumLightLocators = static_cast< s8 >( liNumLightLocators );

                    // Per locator: the TRANSLATION row only (`lvx128 v0, data+0x80+0x40*i` is
                    // maLightLocators[i] row 3) and the tag type (`lwz data+0x650+4*i ; stbx`).
                    for ( s32 liLocator = 0; liLocator < liNumLightLocators; ++liLocator )
                    {
                        lpPhysInfo->maLightLocatorPositions[ liLocator ] =
                            lpLocatorData->maLightLocators[ liLocator ].wAxis;
                        lpPhysInfo->maLightTagPointTypes[ liLocator ] =
                            static_cast< s32 >( lpLocatorData->maLightLocatorTypes[ liLocator ] );
                    }
                }
            }
        }
    }

    // ====================================================================================
    // C. RESET THE DETACHED-PART RECORDS. `li r11, 25 ; stb 0, 0(record) ; addi 0x1010`
    // over every slot, live or not (0x8271E974..0x8271E990).
    // ====================================================================================
    for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES; ++luSlot )
    {
        maTrafficPhysicsInfoList[ luSlot ].mDetachedPartQueue.muNumParts = 0;
    }

    // ====================================================================================
    // D. THE DETACHED-PART RENDER EVENTS (0x8271E99C..0x8271EB20). The queue accessor
    // BrnPhysi @0x822ACC30 is BaseEventQueue<DetachedPartRenderEvent>::GetEvent (80-byte
    // stride). The four transform rows are held in v124..v127 across the owner test.
    // ====================================================================================
    {
        const DeformationOutputInterfaceForEntityModules::DetachedPartRenderQueue& lrPartQueue =
            lpDefInterface->GetDetachedPartRenderQueue();

        const s32 liNumParts = lrPartQueue.GetLength();
        for ( s32 liPart = 0; liPart < liNumParts; ++liPart )
        {
            const DetachedPartRenderEvent& lrEvent = lrPartQueue.GetEvent( liPart );
            const u32 luEntityWord = lrEvent.mVehicleEntityId.muValue;

            if ( ( luEntityWord >> KU_ENTITY_TYPE_SHIFT ) != BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
            {
                continue;
            }
            const u32 luVehicle = ( luEntityWord >> KU_ENTITY_INDEX_SHIFT ) & KU_ENTITY_INDEX_MASK;
            CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" );     // header :2459

            const Vehicle* lpVehicle = GetVehicle( luVehicle );
            if ( !lpVehicle->IsPhysical() )
            {
                continue;
            }

            // The inline GetPhysicalPartsIndex (`lbz 6(vehicle) ; cmplwi 0x80` -> the
            // BrnTrafficVehicle.h:1092 "miPhysicalPartsIndex >= 0" tripwire, then `extsb`).
            const s32 liPhysicalIndex = static_cast< s8 >( lpVehicle->GetPhysicalPartsIndex() );   // `extsb` after every call
            TrafficPhysicsInfo::DetachedPartRenderQueue& lrRecord =
                maTrafficPhysicsInfoList[ liPhysicalIndex ].mDetachedPartQueue;

            const u32 luSlot = lrRecord.muNumParts;
            CGS_ASSERT( luSlot < BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE,
                        "luSlot < BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE" ); // :3258
            CGS_ASSERT( lrEvent.miPartIndex <= 0xFF, "lEvent.miPartIndex <= 0xff" );             // :3259

            lrRecord.mau8PartIndex[ luSlot ] = static_cast< u8 >( lrEvent.miPartIndex );
            lrRecord.maTransforms[ luSlot ]  = lrEvent.mTransform;
            ++lrRecord.muNumParts;
        }
    }

    // ====================================================================================
    // E. THE GLASS SMASH / CRACK EVENTS (0x8271EB24..0x8271EC54). BrnPhysic @0x8227BF00 is
    // BaseEventQueue<GlassSmashOrCrackEvent>::GetEvent (192-byte stride). Event fields read:
    // +0xA0 mVehicleEntityId, +0xA4 meGlassPart, +0xA8 meNewState, +0xAC mfCrackAmount.
    // ====================================================================================
    {
        const DeformationOutputInterface::GlassSmashOrCrackQueue& lrGlassQueue =
            lpDefInterface->GetGlassSmashOrCrackQueue();

        const s32 liNumGlassEvents = lrGlassQueue.GetLength();
        for ( s32 liEvent = 0; liEvent < liNumGlassEvents; ++liEvent )
        {
            const GlassSmashOrCrackEvent& lrEvent = lrGlassQueue.GetEvent( liEvent );
            const u32 luEntityWord = lrEvent.mVehicleEntityId.muValue;

            if ( ( luEntityWord >> KU_ENTITY_TYPE_SHIFT ) != BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
            {
                continue;
            }
            const u32 luVehicle = ( luEntityWord >> KU_ENTITY_INDEX_SHIFT ) & KU_ENTITY_INDEX_MASK;
            CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" );     // header :2459

            // `lbz r11, 0x2A85(this + idx*0x80) ; rlwinm bit 3` -- GetVehicle(idx)->IsPhysical().
            if ( !GetVehicle( luVehicle )->IsPhysical() )
            {
                continue;
            }

            const s32 liPane = static_cast< s32 >( lrEvent.meGlassPart ) - KI_FIRST_GLASS_BODY_PART;
            CGS_ASSERT( static_cast< u32 >( liPane ) < TrafficPhysicsInfo::KU_NUM_GLASS_PANES,
                        "luOffset < 8" );                                                          // :3293

            TrafficPhysicsInfo* lpPhysInfo = GetTrafficPhysicsInfoForVehicl( luVehicle );

            if ( lrEvent.meNewState == BrnPhysics::Deformation::E_GLASS_STATE_CRACKED )
            {
                // fracture = min( max( old, crackAmount ), 1.0 ) -- the two fsel folds
                // (0x8271EBF8..0x8271EC10), then `or` the pane bit into the damage flags.
                const f32 lfOld = lpPhysInfo->mafGlassPaneFractureAmounts[ liPane ];
                f32 lfFracture = ( lrEvent.mfCrackAmount - lfOld >= 0.0f ) ? lrEvent.mfCrackAmount : lfOld;
                if ( lfFracture - KF_GLASS_CRACKED_FRACTURE_MAX >= 0.0f )
                {
                    lfFracture = KF_GLASS_CRACKED_FRACTURE_MAX;
                }
                lpPhysInfo->mafGlassPaneFractureAmounts[ liPane ] = lfFracture;
                lpPhysInfo->mu8RenderDamageFlags |= static_cast< u8 >( 1u << liPane );
            }
            else if ( lrEvent.meNewState == BrnPhysics::Deformation::E_GLASS_STATE_SMASHED )
            {
                // Flag first, then `stfsx f29` (2.0f) -- 0x8271EC2C..0x8271EC48.
                lpPhysInfo->mu8RenderDamageFlags |= static_cast< u8 >( 1u << liPane );
                lpPhysInfo->mafGlassPaneFractureAmounts[ liPane ] = KF_GLASS_SMASHED_FRACTURE_AMOUNT;
            }
            // Any other state (INTACT) falls through to the next event.
        }
    }

    // F. `lwz r3, 0(&miPerfMon_ProcessDeformation) ; bl StopMonitor`.
    CgsDev::PerfMonCpu::StopMonitor( miPerfMon_ProcessDeformation );
}

} // namespace BrnTraffic
