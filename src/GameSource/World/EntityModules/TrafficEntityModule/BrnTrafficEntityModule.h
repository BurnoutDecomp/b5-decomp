#pragma once

// =============================================================================
// BrnTrafficEntityModule.h  (NEW OWNING HEADER -- partial: element-type home)
//
// DWARF home (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficEntityModule.h) of the BrnTraffic entity-module
// value types. This slice owns ONLY the two small per-frame record types that the
// module keeps in fixed-capacity Array<> collections (the Array<T,N> instantiation
// .cpps live alongside this header):
//
//   BrnTraffic::TrafficCrashInfo    (struct @ BrnTrafficEntityModule.h:129)
//       -> Array<TrafficCrashInfo,160>::Erase @ 0x8270B230, ::GetI @ 0x8270CF08
//   BrnTraffic::FiredKillZoneInfo   (struct @ BrnTrafficEntityModule.h:240)
//       -> Array<FiredKillZoneInfo,8>::Append @ 0x8270B548, ::Erase @ 0x8270B670
//
// The huge remainder of BrnTrafficEntityModule (the module class, its sibling
// record types, the IO interfaces) belongs to other not-yet-reconstructed slices;
// when they land they should GROW this header additively, never redefine these two.
//
// Element sizes are X360-authoritative:
//   * TrafficCrashInfo == 16 bytes: Array<TrafficCrashInfo,160>::Erase @ 0x8270B230
//     reads the live count at byte +0xA00 == 160 * 16, and shifts each element with a
//     16-byte stride (`slwi r11,r31,4`) copying 4 dwords; ::GetI @ 0x8270CF08 returns
//     16*index + base.
//   * FiredKillZoneInfo == 16 bytes: Array<FiredKillZoneInfo,8>::Append @ 0x8270B548
//     reads the live count at byte +0x80 == 8 * 16, and copies each element as two
//     qwords (`ld/std` x2) at a 16-byte stride (`slwi r11,r11,4`).
// =============================================================================

#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "types.hpp"        // u8/u32/s32/u64
#include "BrnCommonTypes.h" // EntityId, CgsID
#include "GameShared/GameClasses/Graphics/CgsModel.h" // CgsGraphics::Model::State (VehicleRenderInfo::mLOD)

namespace CgsModule { struct IOBufferStack; }
namespace BrnDirector { namespace Camera { class Camera; } }

namespace BrnTraffic
{
namespace BrnTrafficIO { class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_Dispatch; class InputBuffer_PreDispatch; class OutputBuffer_PreDispatch; }

namespace BrnTrafficIO { class OutputBuffer_Prepare; }

    // BrnTrafficEntityModule.h:129 -- one pending traffic-crash record. sizeof == 16
    // (X360-authoritative: Array<TrafficCrashInfo,160> count word sits at +0xA00 == 160*16,
    // and the per-element copy moves 4 dwords / a 16-byte stride).
    //
    // meCrashTrafficType is BrnPhysics::Vehicle::eCrashTrafficType
    // (BrnTrafficPhysicsConstants.h:32). That enum's home is not reconstructed in this
    // slice, so the field is stored here as its underlying 32-bit value (the X360 packs
    // the enum in a 4-byte slot); the enumerator names are documented for reference:
    //   Standard=0, Checked=1, Spontaneous=2, Slammed=3, Invalid=255.
    struct TrafficCrashInfo
    {
        EntityId mVictimId;                   // :123  +0x00
        EntityId mCauserId;                   // :124  +0x04
        u32      muCrashTrafficType;          // :125  +0x08  (eCrashTrafficType, 4-byte)
        bool     mbNeedsToBeSentToCrashModule;// :126  +0x0C  (+3 trailing pad -> 16)
    };

    // BrnTrafficEntityModule.h:240 -- a kill-zone the module fired and must remember for
    // a few frames. sizeof == 16 (X360-authoritative: Array<FiredKillZoneInfo,8> count word
    // sits at +0x80 == 8*16, and the per-element copy moves two qwords / a 16-byte stride).
    // mKillZoneId is TrafficData::KillZoneId == uint64_t (BrnTrafficData.h:40); its 8-byte
    // width + the trailing int32 + pad gives the proven 16-byte footprint.
    struct FiredKillZoneInfo
    {
        u64 mKillZoneId;            // :241  +0x00  (TrafficData::KillZoneId == uint64_t)
        s32 miFramesLeftToRemember; // :242  +0x08  (+4 trailing pad -> 16)
    };

    // DWARF home BrnTrafficMiscRuntimeClasses.h:94 -- one purgatory-list record: a vehicle
    // index plus a countdown of decision frames left. sizeof == 4 (X360-authoritative: the
    // Array<PurgatoryInfo,N> instantiations put their live-count word at byte N*4 and copy
    // each element as two halfwords -- Array<PurgatoryInfo,1>::Append @ 0x8270AAC0 stores
    // `sth muIndex@+0` then `sth muDecisionFramesLeft@+2`, count word @ +0x4 == 1*4;
    // Array<PurgatoryInfo,400> count word @ +0x640 == 400*4 (Erase @ 0x8270A770);
    // Array<PurgatoryInfo,1>::GetItem @ 0x8270CA28 returns 4*index + base). Homed here with
    // the module's other small record types; grow this header (never redefine) when the
    // BrnTrafficMiscRuntimeClasses slice lands.
    struct PurgatoryInfo
    {
        u16 muIndex;              // :96  +0x00
        u16 muDecisionFramesLeft; // :97  +0x02  (-> 4)
    };

    // DWARF home BrnTrafficVehicle.h:159 -- one per-frame vehicle-render record (the dispatch
    // list the module hands the renderer). sizeof == 12 (X360-authoritative:
    // Array<VehicleRenderInfo,64>::Append @ 0x8270A148 copies three dwords (`stw` x3) at a
    // 12-byte stride (index*12 == `slwi r,1; add; slwi r,2`), count word @ +0x300 == 64*12;
    // ::Get @ 0x827BA2A0 returns 12*index + base). mLOD is CgsGraphics::Model::State (a
    // 4-byte enum, committed in CgsModel.h). Homed here with the module's other record types.
    struct VehicleRenderInfo
    {
        u32                      muEntityIndex; // :161  +0x00
        f32                      mfDistanceSq;  // :162  +0x04
        CgsGraphics::Model::State mLOD;         // :163  +0x08  (4-byte enum -> 12)
    };

    // One predicted race-car-hull change record: the module keeps a list of pending hull
    // (collision-shape) swaps to apply to nearby race cars (producers:
    // TrafficEntityModule::AddPredictedHullChange / ::UpdateRaceCarHulls; consumer/debug dump
    // TrafficEntityModule::DEBUGDumpHullPredictions). sizeof == 8 (X360-authoritative): the
    // Array<HullChangeInfo,400> instantiations put their live-count word at byte N*8 and copy
    // each element as two dwords --
    //   Array<HullChangeInfo,400>::Append   @ 0x8270ACA8 stores `stw r10,0(slot)` then
    //     `stw r10,4(slot)`, count word @ +0xC80 == 400*8;
    //   Array<HullChangeInfo,400>::EraseFast @ 0x8270ADD0 overwrites slot[index] with the last
    //     live element via a two-dword copy at an 8-byte stride (`slwi r,count,3`);
    //   Array<HullChangeInfo,400>::GetItem   @ 0x8270CE00 returns 8*index + base.
    //
    // FLAG (opaque interior): the 8-byte record's internal field split is not recovered by this
    // slice -- every observed body (Append/EraseFast/GetItem) treats the element only as two
    // 4-byte words. Modelled as exactly two 4-byte words so the asm-attested 8-byte stride and the
    // +0xC80 inline-buffer offset are exact; the interior (the use sites suggest a target race-car
    // entity index plus a frame/hull-state word, but that is not asm-attested) is honestly opaque.
    // Grow this struct in place (never redefine/reorder) when the BrnTrafficHullRuntime slice
    // recovers the field names.
    struct HullChangeInfo
    {
        u32 muWord0; // +0x00  (interior opaque -- see FLAG)
        u32 muWord1; // +0x04  (-> 8)
    };

    // DWARF home BrnTrafficEntityModule.h:255 -- one "crashing thing" the module tracks while
    // building the per-frame list of things nearby traffic should react to (producers/consumers:
    // TrafficEntityModule::UpdateParams_BuildListOfCrashingThings / _TryStartSympatheticCrashing /
    // _TryAvoidCrashing, all operating on Array<CrashingThingData,168>). sizeof == 32
    // (X360-authoritative: the Array<CrashingThingData,168>::operator[] @ 0x8270BE68 returns
    // 32*index + base -- `slwi r,index,5` -- and reads the live-count word at byte +0x1500 ==
    // 168*32). The 32-byte footprint is the 16-byte/16-aligned Vector3 (mPosition) followed by
    // the 4-byte EntityId and the bool, rounded up to the Vector3's 16-byte alignment.
    // DWARF field order/types: Vector3 mPosition (:275), EntityId mEntityId (:276),
    // bool mbShowtimeCrashMagnet (:278).
    struct CrashingThingData
    {
        Vector3  mPosition;            // :275  +0x00  (16, 16-aligned)
        EntityId mEntityId;            // :276  +0x10
        bool     mbShowtimeCrashMagnet;// :278  +0x14  (+pad -> 32, 16-aligned)
    };

    // NB: BrnTraffic::PhysicalVehicleInfo (the Array<PhysicalVehicleInfo,33> element) is NOT
    // homed here -- it has its own committed home BrnTrafficPhysicalVehicleInfo.h; do not
    // redefine it (ODR). Its DWARF fields were grown into that header in this slice.

    // DWARF home BrnTrafficEntityModule.h:293 -- a SIMD "structure-of-arrays" packet holding four
    // collidable vehicles at once (the trailing "4" in the type name = four vehicles per record),
    // cached in mCachedCollidableList (Array<CollidableVehicleInfo4,16> -> up to 64 vehicles ==
    // KU_MAX_COLLIDABLE_CACHED_TRAFFIC). Each Vector4 lane holds the same field for all four
    // vehicles. sizeof == 128 (X360-authoritative: the Array<CollidableVehicleInfo4,16>::operator[]
    // @ 0x8270D260 returns (index<<7) + base -- `slwi r,index,7` == 128*index -- and reads the
    // live-count word at byte +0x800 == 16*128). The 128-byte footprint is exactly eight
    // 16-byte/16-aligned Vector4 registers. DWARF field order/types (:295-303):
    //   Vector4 mPosition_X / _Y / _Z, mLinearVelocity_X / _Y / _Z, mHalfLengths, mHalfWidths.
    // All eight lanes (128 bytes) are DWARF-named at BrnTrafficEntityModule.h:295-303.
    struct CollidableVehicleInfo4
    {
        Vector4 mPosition_X;       // :295  +0x00
        Vector4 mPosition_Y;       // :296  +0x10
        Vector4 mPosition_Z;       // :297  +0x20
        Vector4 mLinearVelocity_X; // :298  +0x30
        Vector4 mLinearVelocity_Y; // :299  +0x40
        Vector4 mLinearVelocity_Z; // :300  +0x50
        Vector4 mHalfLengths;      // :302  +0x60
        Vector4 mHalfWidths;       // :303  +0x70
    };

    // =========================================================================
    // BrnTraffic::TrafficEntityModule  (DWARF home BrnTrafficEntityModule.h:~470)
    //
    // KEYSTONE class home -- the TRAFFIC entity module: a CgsEntityModule subclass that owns and
    // ticks the traffic-vehicle fleet through the scene-update interface. This slice GROWS the
    // committed element-type header (above) ADDITIVELY with the module class itself.
    //
    // LAYOUT IS DWARF-OPAQUE (FOUNDATION / dossier-DWARF-gap):
    //   The X360 DWARF DIE for this class emits ONLY its nested enums (reproduced below); it emits
    //   NO member-field layout for the ~470KB module object. Every X360 method reaches the object
    //   through raw, asm-attested BYTE offsets (e.g. meState @ +0x300, meRunningState @ +0x308,
    //   the maStandard/maStatic vehicle pools at (index+85)<<7 / (index+485)<<7, the static-param
    //   pool at 6*(index+...) , etc.). Because no faithful member layout is recoverable, the object
    //   is modelled here as an OPAQUE fixed storage blob and the recoverable methods are bodied as
    //   asm-attested byte-offset arithmetic over `this` (the "opaque external field by attested
    //   offset" allowance) -- NOT as named members (which would require fabricating the layout).
    //   The mOpaque[] size is itself NOT asm-attested as a single object total, so it is FLAGGED:
    //   it is sized only large enough to cover the highest attested offset touched by a BODIED
    //   method, and pinned with a static_assert; it is NOT the true sizeof and MUST be re-homed
    //   (never trusted for placement-new of the real object) when a member layout is recovered.
    //
    // BODIED (asm-recoverable, attested-offset): the simple vehicle/param-pool address accessors
    //   (GetStandardVehicle / GetStaticVehicle / GetTrailerVehicle / GetVehicleAxles /
    //   GetStaticTrafficParam[Fro|FromFullV] / GetVehicleIndexFromStaticIndex / GetParamNeedToSlowData
    //   / GetParamPlan), the state/flag book-keeping (IsPaused / ShouldBeHollywoodAction /
    //   NeedToTakeActionAgainstJunctionFUP / EnterReplay / LeaveReplay / RestartTraffic), and the
    //   stuck-timer helpers (UpdateVehicleStuckSideTime / UpdateVehicleStuckTimers).
    //
    // DECLARATION-ONLY + FLAGGED (NOT bodied -- would require fabricating the 470KB layout, a
    //   multi-stage VMX pipeline, or reaching an un-homed sub-aggregate / BrnTraffic::Vehicle
    //   interior): the constructor (raw 470KB field init + un-homed sub-aggregate ctors), the
    //   Avoidance_* VMX scoring pipeline, CalculateAndSetSteeringUsingAvoidance, CalculateDriverGasBrake,
    //   the DEBUG_* helpers, GetCarAssetAttribKey, GetDeterministicParamPos, GetHullRuntimeSafe
    //   (reaches mpData->muNumHulls), GetSympCrashingTargetPos, GetTrafficPhysicsInfoForVehicl,
    //   HideAllTraffic / UnhideAllTraffic, IsVehiclesParamAZombie, the JunctionFUP_* pair,
    //   KillDyingVehicleEntities, PutParamInPurgatory, RebuildGeneratorList, SetVehicleTransform
    //   (RwMath transform copy w/ VMX validity asserts), UpdateNormalPhysical, the UpdateParams_*
    //   pipelines, and UpdateSerialiser.
    // =========================================================================
    struct TrafficEntityModule
    {
        // ---- ADDITIVE (attested by WorldModule::Construct @0x827CF540, which
        //      virtual-dispatches the fleet lifecycle) ----
        // Declaration-only; the body lands with this module's own TU.
        void Construct();
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // Declaration-only; the body lands with this module's own TU.
        void Destruct();
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Declaration-only; the body lands with this module's own TU.
        bool Release();

        // ---- ADDITIVE (WorldModule::EntityModulePrePhysicsUpdate @0x827BD5B8) ----
        void PrePhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                               CgsModule::IOBufferStack* lpOutputBufferStack,
                               BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
                               BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                               BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePostSceneUpdate @0x827C3C58) ----
        void PostSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                              CgsModule::IOBufferStack* lpOutputBufferStack,
                              BrnTrafficIO::InputBuffer_PostScene* lpInput,
                              BrnTrafficIO::OutputBuffer_PostScene* lpOutput,
                              BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
        void PreDispatchUpdate( BrnTrafficIO::InputBuffer_PreDispatch* lpInput,
                                BrnTrafficIO::OutputBuffer_PreDispatch* lpOutput );
        void GenerateDispatchLists( BrnTrafficIO::InputBuffer_Dispatch* lpInput,
                                    BrnTrafficIO::OutputBuffer_PreDispatch* lpRenderInfos,
                                    s32 liList, s32 liSortLayer, s32 liSortKey,
                                    const BrnDirector::Camera::Camera* lpCamera );

        // ---- ADDITIVE (attested by WorldModule::Prepare @0x827D53B0 stage 7) ----
        // Declaration-only; the body lands with this module's own TU.
        bool Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer );

        // --- DWARF-attested nested enums (BrnTrafficEntityModule.h:486-559) -------------------

        // :486
        enum EPrepareStage
        {
            E_PREPARESTAGE_START       = 0,
            E_PREPARESTAGE_MANAGER     = 1,
            E_PREPARESTAGE_LOADINGWORLD= 2,
            E_PREPARESTAGE_VOLUMES     = 3,
            E_PREPARESTAGE_DEBUG       = 4,
            E_PREPARESTAGE_DONE        = 5,
        };

        // :496
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // :503
        enum EResourceAcquireStage
        {
            E_RESOURCE_LOAD_BASEDATA_NOT_STARTED = 0,
            E_RESOURCE_LOAD_BASEDATA_REQUESTED   = 1,
            E_RESOURCE_LOAD_VEHICLELISTAQUIRE    = 2,
            E_RESOURCE_LOAD_WFVEHICLELISTAQUIRE  = 3,
            E_RESOURCE_LOAD_VEHICLES             = 4,
            E_RESOURCE_WFLOAD_VEHICLES           = 5,
            E_RESOURCE_LOAD_PHYSICS              = 6,
            E_RESOURCE_WFLOAD_PHYSICS            = 7,
            E_RESOURCE_LOAD_ATTRIBS              = 8,
            E_RESOURCE_WFLOAD_ATTRIBS            = 9,
            E_RESOURCE_LOAD_WHEELS               = 10,
            E_RESOURCE_WFLOAD_WHEELS             = 11,
            E_RESOURCE_ACQUIRE_COUNT             = 12,
        };

        // :521
        enum EState
        {
            E_STATE_INVALID      = -1,
            E_STATE_STARTING_UP  = 0,
            E_STATE_RUNNING      = 1,
            E_STATE_TEARING_DOWN = 2,
        };

        // :530
        enum EStartingUpState
        {
            E_STARTINGUPSTATE_INVALID              = -1,
            E_STARTINGUPSTATE_WAITING_FOR_PLAYER   = 0,
            E_STARTINGUPSTATE_POPULATING           = 1,
            E_STARTINGUPSTATE_WAITING_FOR_STREAMING= 2,
            E_STARTINGUPSTATE_FIRST                = 0,
            E_STARTINGUPSTATE_LAST                 = 2,
        };

        // :542
        enum ERunningState
        {
            E_RUNNINGSTATE_INVALID = -1,
            E_RUNNINGSTATE_NORMAL  = 0,
            E_RUNNINGSTATE_PAUSED  = 1,
        };

        // :550
        enum ETearingDownState
        {
            E_TEARINGDOWNSTATE_INVALID         = -1,
            E_TEARINGDOWNSTATE_WIPING          = 0,
            E_TEARINGDOWNSTATE_FLUSHING        = 1,
            E_TEARINGDOWNSTATE_WAITING_TO_RESET= 2,
        };

        // :559
        enum EEmptyTrafficPoolState
        {
            E_EMPTYTRAFFICPOOLSTATE_IDLE     = 0,
            E_EMPTYTRAFFICPOOLSTATE_EMPTYING = 1,
            E_EMPTYTRAFFICPOOLSTATE_EMPTY    = 2,
            E_EMPTYTRAFFICPOOLSTATE_FILLING  = 3,
        };

        // --- DWARF-attested capacity / index constants (used by the bodied accessors' asserts) ---
        static const u32 KU_MAX_STATIC_TRAFFIC   = 0xC7;  // 199
        static const u32 KU_MAX_STANDARD_TRAFFIC = 0x190; // 400
        static const u32 KU_MAX_TOTAL_TRAFFIC    = 0x258; // 600
        static const u32 KU_MAX_PARAMS           = 0x190; // 400
        static const u32 KU_PARAM_NUM_PLANS      = 2;
        static const u32 KU_STATIC_TRAFFIC_OFFSET= 0x190; // 400

        // ---------------------------------------------------------------------
        // BODIED methods (asm-attested offset arithmetic / flag book-keeping).
        // Pointer-returning accessors return a byte address into `this`; callers in other slices
        // reinterpret them as the (un-homed) BrnTraffic::Vehicle / Param / StaticParam interiors.
        // ---------------------------------------------------------------------
        void* GetStandardVehicle(u32 luIndex);                       // @ 0x82707A38
        void* GetStaticVehicle(u32 luIndex);                         // @ 0x827079D0
        void* GetTrailerVehicle(s32 liIndex);                        // @ 0x82707AA0
        void* GetVehicleAxles(u32 luIndex);                          // @ 0x82707C28
        void* GetStaticTrafficParam(u32 luIndex);                    // @ 0x82707858
        void* GetStaticTrafficParamFro(u32 luIndex);                 // @ 0x82707950
        void* GetStaticTrafficParamFromFullV(u32 luIndex);           // @ 0x827078D0
        u32   GetVehicleIndexFromStaticIndex(u32 luStaticVehicle);   // @ 0x82707D18
        void* GetParamNeedToSlowData(u32 luParam);                   // @ 0x827077D0
        void* GetParamPlan(u32 luParam, u32 luPlan);                 // @ 0x82707D70

        bool  IsPaused();                                            // @ 0x82707560
        bool  ShouldBeHollywoodAction();                             // @ 0x827075C8
        bool  NeedToTakeActionAgainstJunctionFUP();                  // @ 0x82707FD0
        void  EnterReplay();                                         // @ 0x827081D8
        void  LeaveReplay();                                         // @ 0x82708248
        void  RestartTraffic();                                      // @ 0x82708F98

        // ---------------------------------------------------------------------
        // DECLARATION-ONLY + FLAGGED (see header comment). Bodying these would require fabricating
        // the 470KB member layout, a multi-stage VMX pipeline, or reaching an un-homed
        // sub-aggregate / BrnTraffic::Vehicle interior. Declared (not bodied) so the keystone type
        // exposes its full API and unblocks the traffic cluster.
        // ---------------------------------------------------------------------
        void  ctor();                                                // @ 0x827E4880 (FLAG: 470KB raw init)
        void* Avoidance_CalculateDistancePosVelToOrig(void* lpResult);// @ 0x82708DD0 (FLAG: VMX)
        void  Avoidance_CalculatePassingScore();                     // @ 0x827199B8 (FLAG: VMX)
        void  CalculateAndSetSteeringUsingAvoidance();               // @ 0x8273D258 (FLAG: VMX)
        void  CalculateDriverGasBrake();                             // @ 0x82718CD8 (FLAG)
        void  DEBUG_AddFuzzyLogicData();                             // @ 0x82716040 (FLAG: debug)
        void  DEBUG_RenderContactPoint();                            // @ 0x827082B8 (FLAG: debug)
        u64   GetCarAssetAttribKey(u32 luVehicle);                   // @ 0x8273EFC8 (FLAG: Vehicle interior)
        void  GetDeterministicParamPos(u32 luParam);                 // @ 0x82714258 (FLAG: ParamTransform)
        void* GetHullRuntimeSafe(u32 luHull);                        // @ 0x8271DA70 (FLAG: mpData interior)
        void  GetSympCrashingTargetPos(u32 luParam, void* lpOut);    // @ 0x82708C10 (FLAG)
        void  GetTrafficPhysicsInfoForVehicl();                      // @ 0x82714500 (FLAG)
        void  HideAllTraffic();                                      // @ 0x8273F418 (FLAG)
        void  UnhideAllTraffic();                                    // @ 0x8274A500 (FLAG)
        bool  IsVehiclesParamAZombie(u32 luVehicle);                 // @ 0x82715D70 (FLAG: Vehicle interior)
        void  JunctionFUP_StopOffscreenTraffic(void* lpData, bool lbFlag); // @ 0x82719868 (FLAG)
        void  JunctionFUP_TryClearupNonMovingPhysical();            // @ 0x8273F2E8 (FLAG)
        void  KillDyingVehicleEntities();                            // @ 0x82741E40 (FLAG)
        void  PutParamInPurgatory(u32 luParam);                      // @ 0x82716510 (FLAG: Array interior)
        void  RebuildGeneratorList();                                // @ 0x82742DD0 (FLAG)
        void  SetVehicleTransform(u32 luIndex, const void* lpTransform); // @ 0x827142B8 (FLAG: VMX validity)
        void  UpdateNormalPhysical(u32 luIndex, void* lpDriverControls); // @ 0x8273EF08 (FLAG)
        void  UpdateParams_CalcDesiredSpeed();                       // @ 0x82717928 (FLAG: VMX)
        void  UpdateParams_TryAvoidCrashing();                       // @ 0x82716948 (FLAG: VMX)
        void  UpdateParams_TryToReinsertParam();                     // @ 0x827247F0 (FLAG)
        void  UpdateSerialiser();                                    // @ 0x8272DA80 (FLAG)
        // FLAG (un-recoverable rodata): the stuck-side timer floor constant flt_82001CC0 (the fsel
        // else-value at 0x82707FB0) is NOT attested in the dossier pseudocode, so the timer clamp
        // cannot be reconstructed without fabricating it -- declaration-only.
        void  UpdateVehicleStuckSideTime(s32 liFlags, s32 liMask, f32 lfReset,
                                         f32 lfThreshold, f32* lpfTimer);   // @ 0x82707F00 (FLAG)
        void  UpdateVehicleStuckTimers(void* lpPhysicsInfo, f32 lfReset, f32 lfThreshold); // @ 0x82708D48 (FLAG)

        // Out-of-line external deps the bodied methods call (NOT this slice -- declared so the
        // bodied .cpp links / compiles; their homes live in other slices):
        void  EnterTearingDownState();   // @ 0x82708F70-ish -- called by RestartTraffic (decl-only here)

        // FLAGGED opaque storage -- see header comment. Sized to cover the highest attested byte
        // offset a BODIED method touches: ShouldBeHollywoodAction reads +0x7286A and
        // NeedToTakeActionAgainstJunctionFUP reads +0x72875, so storage runs to 0x73000. This is
        // NOT the true object size (the real ~470KB+ layout is un-homed); do NOT use
        // sizeof(TrafficEntityModule) for the real object. mOpaque is the only storage so the type
        // is concrete and instantiable.
        u8 mOpaque[0x73000];   // FLAG: opaque, NOT asm-attested as the true total size

        static void _AssertLayout();
    };
}
