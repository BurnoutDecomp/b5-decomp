#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h
//
// BrnWorld::PropZoneManager  -- owns the loaded prop/part instance pools and forwards
// per-prop scene/sim/contact operations to its embedded PropCellManager. Also homes
// BrnWorld::PropGraphicsManager (the per-zone prop-graphics reference table).
//
// Reconstructed from:
//   - DecFIGS DWARF (BrnPropZoneManager.h)  -> member set / order / method shapes (X360-gated).
//   - BURNOUT_X360_ARTIST.XEX asm           -> behaviour + member offsets (authoritative).
//
// This is the SHIPPED X360/FIGS layout, which is heavily refactored from the older
// Feb-2007 PropZoneManager (that version kept a per-instance InstanceIndex table and a
// single combined instance pool; the shipped version splits prop and part pools into
// per-zone "slots", embeds a PropCellManager, and adds respawn/hit/rotation bookkeeping).
//
// LAYOUT (member offsets are X360-console facts pinned by Construct @0x822F0568 and the
// reconstructed bodies; on our x64 PC compile pointer-width widening shifts absolute
// offsets, so these are provenance, not host asserts -- we model the PC layout and do
// not byte-match).
//
// ⚠️ CONSOLE-vs-HOST (corrected 2026-08-12): every absolute offset in the table below is
// an X360 (4-byte-pointer) fact. The embedded PropCellManager's console sizeof is 2432,
// but its PC form is WIDER (mpaProps/mpaPropParts widen 4->8 each), so on the host
// maProps does NOT start at +2432 and none of the following offsets hold. Nothing in
// this TU may transcribe them: every body indexes by named member / sizeof, and the
// only host-pinned facts are the ones _AssertLayout() below states.
//   mCellManager            @ +0        (PropCellManager, console sizeof 2432; PC wider)
//   maProps[5418]           @ +2432     (PropEntityInstance, 80B each; 9 slots * 602)
//   maParts[4518]           @ +435968   (PropPartEntityInstance, 80B each; 9 slots * 502)
//     ⚠️ CORRECTED 2026-08-12: this line used to read +435872 (== 2432 + 5418*80, i.e. the
//     assumption that maParts starts immediately after maProps). The asm says otherwise --
//     BOTH GetPart overloads build the part address as `80*idx + this + 435968`
//     (0x822A437C / 0x822CDCC0: `addis 7` then `addi -0x5900`), and the very next entry
//     below already agrees (435968 + 4518*80 == 797408 == maUsedProps, whereas the old
//     435872 would leave it 96 short). So there are 96 CONSOLE bytes between the two pools
//     that this table does not model. Harmless here -- nothing in this TU addresses either
//     pool by absolute offset -- but do not "derive" one base from the other.
//   maUsedProps             @ +797408   (BitArray<9>  -- which of the 9 prop-pool slots are in use)
//   maUsedParts             @ +797416   (BitArray<9>  -- which of the 9 part-pool slots are in use)
//   maDontRespawnProps      @ +797424   (BitArray<5400>, 680B)
//   maRespawnDifferentProps @ +798104   (BitArray<5400>, 680B)
//   maPreviouslyHitProps    @ +798784   (Profile::HitPropsBitArray == BitArray<300000>, 37504B)
//   mu16NumberOfLoadedProps @ +836288
//   mauStartIndexOfZone[500]      @ +836290  (u16; KU_UNLOADED_ZONE sentinel)
//   mauNumberOfPropsInZone[500]   @ +837290  (u16)
//   mauStartIndexOfParts[500]     @ +838290  (u16)
//   mauNumberOfPartsInZone[500]   @ +839290  (u16)
//   mUsedRotationParams           @ +840296  (BitArray<100>)
//   maRotationParams[100]         @ +840312  (PropEntityRotationParams)
//   mauTrafficLightsToRestore     @ +841232  (Array<u32,80>)
//
// The four per-zone u16 arrays are indexed by zone id (0..499). Each loaded zone is
// assigned ONE prop-pool slot (start index = slot * KU_SIZE_OF_PROP_ZONE_SLOT) and ONE
// part-pool slot (start index = slot * KU_SIZE_OF_PART_ZONE_SLOT); UnloadZone frees them.
#include <cstddef>               // offsetof (_AssertLayout host pins)

#include "types.hpp"
#include "BrnCommonTypes.h"      // Matrix44Affine, Vector3
#include "GameShared/GameClasses/Containers/CgsBitArray.h"  // CgsContainers::BitArray
#include "GameShared/GameClasses/Containers/CgsArray.h"     // CgsContainers::Array
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"    // PropEntityID, PropVolumeInstanceID
#include "BrnPropEntityInstance.h"   // PropEntityInstance, PropPartEntityInstance, PropEntityRotationParams
#include "BrnPropCellManager.h"      // PropCellManager (embedded by value)

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h" // InSceneUpdateInterface
#include "SharedClasses/Physics/Props/PropRespawnTypesEnum.h"            // BrnPhysics::Props::eRespawnType (GetRespawnType return)

namespace BrnPhysics { namespace Props {
    class  PropPhysicsDataHeader;
    class  PropTypeData;
    struct PropZoneData;
    struct PropInstanceData;
    struct PropGraphics;
    struct PropPartGraphics;
    class  PropInputInterface;
}}

// The shipped X360 build threads the prop-entity replay serialiser through the whole
// load/unload/scene path (LoadZone -> LoadProp -> PropCellManager::AddPropToScene), so
// every one of those signatures carries a PropEntitySerialiser*. Pointer-only here --
// the complete type (GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h) is
// included by the .cpp, which actually calls GetStaticLayout(). Same forward declaration
// BrnPropCellManager.h uses, so the two agree.
namespace BrnReplays { class PropEntitySerialiser; }

namespace BrnWorld
{
    namespace PropEntityIO
    {
        class OutputBuffer_PreScene;
        class OutputBuffer_PrePhysics;
        class OutputBuffer_PostPhysics;
    }

    // ---- per-zone slot geometry (DWARF BrnPropZoneManager.h:356/361; X360-confirmed) ----
    static const u32 KU_SIZE_OF_PROP_ZONE_SLOT = 602; // maProps span per zone slot (600 + 2 overhead)
    static const u32 KU_SIZE_OF_PART_ZONE_SLOT = 502; // maParts span per zone slot (500 + 2 overhead)
    static const u16 KU_UNLOADED_ZONE          = 65535; // mauStartIndexOfZone sentinel
    static const u32 KU_NUM_ZONE_SLOTS         = 9;     // simultaneously-loadable zones

    // pool / table capacities (BrnPropConstants; KU_MAX_LOADED_PROPS = 5400 etc.)
    static const u32 KU_MAX_ZONES                 = 500;
    static const u32 KU_MAX_PROP_TYPES            = 500;
    static const u32 KU_MAX_LOADED_PROP_INSTANCES = 5400;
    static const u32 KU_MAX_LOADED_PART_INSTANCES = 4500;
    static const u32 KU_MAX_PROP_INSTANCES_PER_ZONE = 600; // BrnPropConstants.h:30
    static const u32 KU_MAX_PROP_PARTS_PER_ZONE     = 500; // BrnPropConstants.h:33
    // LoadProp's E_DONT_RESPAWN tripwire names this one explicitly
    // ("liZoneDataIndex < static_cast< int32_t >( BrnPhysics::Props::
    //   KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE ) || !HasPropBeenHit( liZoneId, liZoneDataIndex )",
    // X360 @0x822F375C `cmpwi r14, 0x258`). Same value (600) as the per-zone prop cap.
    static const u32 KU_MAX_NON_PERSISTENT_PROPS_PER_ZONE = 600;
    static const u32 KU_PROPS_POOL_SIZE           = 5418; // 9 * KU_SIZE_OF_PROP_ZONE_SLOT
    static const u32 KU_PARTS_POOL_SIZE           = 4518; // 9 * KU_SIZE_OF_PART_ZONE_SLOT
    static const u32 KU_MAX_ROTATION_PARAMS       = 100;
    static const u32 KU_MAX_TRAFFIC_LIGHTS_TO_RESTORE = 80;

    // Profile::HitPropsBitArray == BitArray<300000> (DWARF BrnProfile.h:555).
    typedef CgsContainers::BitArray<300000u> HitPropsBitArray;

    // DWARF BrnPropZoneManager.h:737 -- `const VecFloat KVF_MIN_DIST_FROM_PLAYER`, the
    // radius of the "do not pop a prop in on top of the player" exclusion sphere that
    // LoadProp's tail applies. The X360 reads it as the broadcast vector unk_82FAD760 and
    // forms `(KVF_MIN_DIST_FROM_PLAYER + type->GetBoundingRadius())^2` to compare against
    // the squared player->prop distance.
    //
    // MAGNITUDE RECOVERED 2026-08-12 (conductor, targeted IDA export on a DB copy). The
    // earlier FLAG assumed 0x82FAD760 was an undumped .rodata range; it is actually .bss,
    // so it reads zero in the shipped image and is filled at runtime by a C++ dynamic
    // initialiser that IDA never named as a function -- which is why a pseudocode scan
    // found no writer. The initialiser is at 0x82C4B9B8:
    //     lis  r11, flt_82004270@ha
    //     lfs  f0,  flt_82004270@l(r11)   ; the scalar
    //     vspltw v0, v0, 0                ; broadcast to all four lanes (hence VecFloat)
    //     stvx128 v0, r0, r11             ; -> unk_82FAD760
    // and flt_82004270 = 0x40400000 = 3.0f. So the exclusion sphere is 3 metres, grown by
    // the prop's own bounding radius. (The 2.0f placeholder that stood here was a safe
    // guess in the right direction -- deliberately small so it could not suppress spawning
    // -- but it was still a guess; this is the measured value.)
    static const f32 KF_MIN_DIST_FROM_PLAYER = 3.0f; // unk_82FAD760 <- flt_82004270

    // ========================================================================
    // BrnWorld::PropZoneManager
    // ========================================================================
    class PropZoneManager
    {
    public:
        typedef BrnPhysics::Props::PropZoneData          PropZoneData;
        typedef BrnPhysics::Props::PropPhysicsDataHeader PropPhysicsDataHeader;
        typedef BrnPhysics::Props::PropTypeData          PropTypeData;
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;

        void Construct();        // 0x822F0568
        bool Prepare();          // 0x...  (DWARF; own pass)
        bool Release();          // 0x...
        void Destruct();         // 0x...

        u32  GetNumberOfPropsInZone(u16 lu16ZoneId) const;
        u32  GetNumberOfPartsInZone(u16 lu16ZoneId) const;

        // 0x822A41A8 / 0x822A4298 (both emitted OUT OF LINE; DWARF :114 / :124).
        // Resolve the prop / part instance at (zone, index-within-zone).
        //  * GetProp   asserts luZoneId < KU_MAX_ZONES, mauStartIndexOfZone[zone] !=
        //    KU_UNLOADED_ZONE, luPropIndex < mauNumberOfPropsInZone[zone] and
        //    start+index < KU_MAX_LOADED_PROP_INSTANCES, then returns &maProps[start+index].
        //    ⚠️ CORRECTED: the previous banner claimed it "may return null for an empty
        //    slot" -- the X360 body has no null path at all, it always returns the slot
        //    address (the emptiness checks are the asserts).
        //  * GetPart resolves the owning prop first and returns
        //    &maParts[prop->mu16PartsIndex + luPartIndex] -- so it takes THREE ids, not two
        //    (DWARF: GetPart(uint16_t, uint16_t, uint16_t)); the committed 2-arg
        //    declaration did not match either the DWARF or the asm.
        //  * Both are NON-const in the DWARF and in the asm (they hand out mutable slots).
        PropEntityInstance*     GetProp(u16 lu16ZoneId, u32 luPropIndex);
        PropPartEntityInstance* GetPart(u16 lu16ZoneId, u16 lu16PropIndex, u16 lu16PartIndex);

        // 0x822CDA28 / 0x822CDB90 -- the PropEntityID-keyed overloads (both emitted OUT OF
        // LINE, both left unnamed by IDA; identified by their assert texts
        // "!lEntityId.IsPart()" @BrnPropZoneManager.h:534 and
        // "lEntityId.GetEntityIndex() < BrnPhysics::Props::KU_MAX_LOADED_PROP_INSTANCES"
        // @:578). Unlike the (zone, index) pair above these take a GLOBAL entity index:
        //   GetProp -> &maProps[ lEntityId.GetEntityIndex() ]
        //              (asm 0x822CDAF8: idx*5 then <<4 == idx*80, added to this + 0x980)
        //   GetPart -> &maParts[ (maProps[idx].mu16PartsIndex
        //                         + lEntityId.GetPartIndex() - 1) & 0xFFFF ]
        //              (asm 0x822CDCA4: addis 1 / addi -1 / clrlwi 16 == (x-1) mod 65536)
        // Both assert the owner byte is E_ENTITYTYPE_PROP and that GetZone(id) != -1 and
        // IsZoneLoaded(zone). Needed by PropEntityModule::GenerateDispatchLists @0x822FB4F0.
        //
        // BODIED 2026-08-12 (link-closure pass). The two index computations above are
        // asm-attested; the strides come from sizeof(), NOT from the console's literal 80
        // (PropEntityInstance is host-laid-out -- see _AssertLayout()).
        PropEntityInstance*     GetProp(PropEntityID lEntityId);
        PropPartEntityInstance* GetPart(PropEntityID lEntityId);

        // ADDITIVE GROW (link-closure pass, 2026-08-12). 0x822C5FF0 -- the entity-id keyed
        // zone lookup, emitted OUT OF LINE and NAMED by IDA. It fires the owner tripwire and
        // then returns maProps[lEntityId.GetEntityIndex()].mu16ZoneIndex as a SIGNED 16-bit
        // value (`lhz r3, 0x9C6(r11)` == maProps base 0x980 + 70; every caller does
        // `extsh r11, r3; cmpwi r11, -1`, so -1 means "no zone"). It is the assert the two
        // id-keyed accessors above run, and RecordHitProp/WasPropPreviouslyHit call it too.
        // Declaration is additive: no member, layout or sizeof change.
        s16 GetZone(PropEntityID lEntityId) const;

        // 0x822FC168 -- allocate a prop slot + a part slot for the zone, load every prop
        // (LoadProp, unrolled x4 in the X360 build), and register the zone's cells.
        //
        // ⚠️ SIGNATURE CORRECTED 2026-08-12 (prop-spawn wave). The committed declaration
        // carried only the DWARF's four PS3 parameters and returned s32. The shipped X360
        // build passes SIX GPR args plus one vector, and returns nothing:
        //     r3 = this
        //     r4 = lpZoneData                       (lhz 0x18 == muZoneId, lwz 0x14/0x10)
        //     r5 = lpTypes                          (forwarded verbatim as LoadProp's r8)
        //     r6 = lpOutput                         (`mr r3,r6; bl sub_822B9738` ==
        //                                            OutputBuffer_PreScene::GetSceneInputInterface)
        //     r7 = lbReplayActive                   (forwarded verbatim as LoadProp's r10)
        //     r8 = lpSerialiser                     (`stw r27, var_FC` == LoadProp's stack arg)
        //     v1 = lPlayerPosition                  (`vmr128 v127,v1`, replayed into every
        //                                            LoadProp call as `vmr128 v1,v127`)
        //   and the epilogue sets no return value (DWARF :131 agrees: `void LoadZone(...)`).
        // The Vector3 keeps its DWARF position (3rd); the two extra pointers are appended
        // in GPR order after lpOutput, exactly as LoadProp's own save-area slots show.
        void LoadZone(const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                      Vector3 lPlayerPosition, PropEntityIO::OutputBuffer_PreScene* lpOutput,
                      bool lbReplayActive, BrnReplays::PropEntitySerialiser* lpSerialiser);

        // 0x82303790 -- remove every prop/part of the zone from scene/sim/contact-gen,
        // free its rotation-params slots, and deallocate its prop/part pool slots.
        // The X360 body takes SIX GPR args (r4..r9); the replay serialiser is the one this
        // recon adds, because UnloadZone forwards it to PropCellManager::RemoveCells and to
        // the three scene forwarders below (all of which take it in the shipped build).
        void UnloadZone(u16 lu16ZoneId, const PropPhysicsDataHeader* lpTypes,
                        PropCellManager::RecentlyBrokenPropsArray* lpRecentlyBrokenProps,
                        PropEntityIO::OutputBuffer_PreScene* lpOutput,
                        BrnReplays::PropEntitySerialiser* lpSerialiser);

        // 0x822F0920 -- per-frame physics-result apply for one prop or part instance.
        // The trailing serialiser is asm-attested: `stw r10, 0x220+arg_74` at the prologue,
        // reloaded as `lwz r8, arg_74` for the RemovePropFromScene forward at 0x822F14CC.
        void UpdateInstance(PropEntityID lEntityId, Matrix44Affine lTransform,
                            Vector3 lLinearVelocity, Vector3 lAngularVelocity, bool lbFrozen,
                            const PropPhysicsDataHeader* lpTypeData, f32 lfTimeStep,
                            PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                            BrnReplays::PropEntitySerialiser* lpSerialiser);

        bool IsZoneLoaded(u16 lu16ZoneId) const;   // 0x822A4390 (out-of-line)

        // 0x822BC920 -- has this prop (zone + index-within-zone) already been hit?
        // DWARF BrnPropZoneManager.h:181 overload HasPropBeenHit(uint32_t,uint32_t) const.
        bool HasPropBeenHit(u32 luZoneIndex, u32 luPropIndex) const;

        // 0x822BC4D0 -- classify respawn behaviour for a prop from the two per-prop respawn
        // bit sets. DWARF BrnPropZoneManager.h:176 attests the eRespawnType return.
        BrnPhysics::Props::eRespawnType GetRespawnType(PropEntityID lId) const;

        // 0x822BCA60 -- extract this zone's 600-bit previously-hit run into a 10-word (u64[10])
        // LSB-aligned output buffer. (Private helper in the X360; asm-gated, not in DWARF.)
        void GetHitPropsFromZone(u64* lpaHitProps, u32 luZoneIndex) const;

        // 0x822CDDE0 -- drain mauTrafficLightsToRestore into the output buffer's prop->traffic
        // interface as restore requests, then clear the list. Called by
        // PropEntityModule::PrePhysicsUpdate.
        void SendTrafficLightRestoreEvents(PropEntityIO::OutputBuffer_PrePhysics* lpOutput);

        // ---- thin forwarders to mCellManager (with a "prop index in zone" precheck) ----
        // ⚠️ All three scene forwarders carry the replay serialiser in the shipped build
        // (asm 0x822F06A0 / 0x822F0740 / 0x822F0880 all do `mr r24, r8` then `mr r8, r24`
        // straight into the PropCellManager call, whose r8 is the PropEntitySerialiser the
        // cell manager hands to SetPropAddedToScene). RemovePropFromSim does NOT -- it takes
        // only r4..r7 and derives the prop-input interface from lpOutput.
        void AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 BrnReplays::PropEntitySerialiser* lpSerialiser); // 0x822F06A0
        void RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                 BrnReplays::PropEntitySerialiser* lpSerialiser); // 0x822F0740
        void RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                      PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene,
                                      BrnReplays::PropEntitySerialiser* lpSerialiser); // 0x822F0880
        void RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                               PropVolumeInstanceID lVolumeInstanceID,
                               PropEntityIO::OutputBuffer_PreScene* lpOutput);    // 0x822F07E0

        // 0x822DEF50 (64 insns; DWARF :248) -- the streaming "drop everything" path
        // (PropEntityModule::UpdateInstanceStreaming). Clears every pool/respawn/rotation
        // bit array, resets the cell manager's live counters, marks all 500 zones unloaded,
        // queues a whole-scene entity flush on the scene-update interface and raises the
        // physics side's remove-all request.
        void RemoveAllPropsAndParts(PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // 0x822BCC00 (DWARF :194 overload RecordHitProp(int32_t,int32_t)) -- flag one prop
        // (zone + index-within-zone) as hit in the persistent 300000-bit progression array.
        void RecordHitProp(s32 liZoneIndex, s32 liPropIndex);

        // Never called -- host layout tripwires (offsetof inside a member function is legal
        // for private members). Pins the facts the bodies in this TU actually rely on.
        static void _AssertLayout();

    private:
        // 0x822DF050 / 0x822DF1F8 -- own TU.
        s32  AllocatePropInstancesBlock(u32 luSizeOfBlock);
        s32  AllocatePartInstancesBlock(u32 luSizeOfBlock);
        // 0x822C5C40 / 0x822C5E18 -- defined in this TU.
        void DeallocatePropInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock);
        void DeallocatePartInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock);
        // 0x822F2EF0 (947 insns) -- turn one serialised PropInstanceData record into a live
        // prop entity: validate its cell, claim a rotation-params slot, InitialiseFromData,
        // apply its respawn class, build its part instances, run the "too close to the
        // player" gate and push it into the scene.
        //
        // SIGNATURE (asm-derived; Hex-Rays renders 32 bogus params). The parameter save
        // area (8-byte slots) pins the C++ order exactly, and it matches DWARF :419 plus
        // the two X360-only replay parameters:
        //     slot0  r3  this
        //     slot1  r4  lpiPropPoolIndex   (`lwz r30,0(r4)` in / `stw v+1,0(r10)` out)
        //     slot2  r5  lpiPartPoolIndex   (`lwz r11,0(r5)` in / `stw r28,0(r11)` out)
        //     slot3  r6  liZoneDataPropIndex
        //     slot4  r7  lpZoneData
        //     slot5  r8  lpTypes
        //     slot6+7    lPlayerPosition    <-- the 16-byte VECTOR arg (v1) consumes TWO
        //                                      slots here; that gap is what proves the
        //                                      Vector3 sits BEFORE the trailing pointers
        //                                      (and why r9/r10 home at 0x50/0x58, not
        //                                      0x40/0x48).
        //     slot8  r9  lpScene            (InSceneUpdateInterface*, DWARF :419's last)
        //     slot9  r10 lbReplayActive     (bool; selects WasPropPreviouslyHit over
        //                                    HasPropBeenHit)
        //     slot10 stk lpSerialiser       (`lwz r3, arg_64` -> GetStaticLayout, and the
        //                                    r8 of PropCellManager::AddPropToScene)
        void LoadProp(s32* lpiPropPoolIndex, s32* lpiPartPoolIndex, s32 liZoneDataPropIndex,
                      const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                      Vector3 lPlayerPosition, InSceneUpdateInterface* lpScene,
                      bool lbReplayActive, BrnReplays::PropEntitySerialiser* lpSerialiser);

    private:
        // ---- DWARF-faithful member layout (see header banner for console offsets) ----
        PropCellManager  mCellManager;                                  // +0
        PropEntityInstance     maProps[KU_PROPS_POOL_SIZE];             // +2432
        PropPartEntityInstance maParts[KU_PARTS_POOL_SIZE];            // +435872
        CgsContainers::BitArray<KU_NUM_ZONE_SLOTS> maUsedProps;        // which prop slots loaded
        CgsContainers::BitArray<KU_NUM_ZONE_SLOTS> maUsedParts;        // which part slots loaded
        CgsContainers::BitArray<5400u> maDontRespawnProps;
        CgsContainers::BitArray<5400u> maRespawnDifferentProps;
        HitPropsBitArray maPreviouslyHitProps;                         // BitArray<300000>
        u16  mu16NumberOfLoadedProps;
        u16  mauStartIndexOfZone[KU_MAX_ZONES];
        u16  mauNumberOfPropsInZone[KU_MAX_ZONES];
        u16  mauStartIndexOfParts[KU_MAX_ZONES];
        u16  mauNumberOfPartsInZone[KU_MAX_ZONES];
        CgsContainers::BitArray<100u> mUsedRotationParams;
        PropEntityRotationParams maRotationParams[KU_MAX_ROTATION_PARAMS];
        // Array<T,N> is the project's unqualified fixed-array container (CgsArray.h); the
        // DWARF spells it CgsContainers::Array<u32,80u>.
        ::Array<u32, KU_MAX_TRAFFIC_LIGHTS_TO_RESTORE> mauTrafficLightsToRestore;

        friend class PropEntityDebugComponent;
    };

    // ========================================================================
    // BrnWorld::PropGraphicsManager -- a 500-entry table mapping a prop *type id* to its
    // ref-counted PropGraphics record. Reconstructed from the DWARF (BrnPropZoneManager.h:436)
    // and the X360 Register body (0x822A9DE8).
    // ========================================================================
    class PropGraphicsManager
    {
    public:
        typedef BrnPhysics::Props::PropGraphics     PropGraphics;
        typedef BrnPhysics::Props::PropPartGraphics PropPartGraphics;

        // BrnPropZoneManager.h:465 -- one table slot: a graphics pointer + a ref count.
        //
        // ⚠️ BANNER CORRECTED 2026-08-12. The old comment claimed an "8-byte slot (X360
        // `8*type + base`)" and hand-padded to it. 8 is the CONSOLE stride (4-byte pointer
        // + refcount + 3 pad); on our x64 host the pointer is 8 bytes, so the real slot is
        // 16 bytes. The explicit tail padding is therefore wrong on both targets and is
        // dropped -- the compiler's own alignment gives the host stride, indexing goes
        // through maPropGraphicsReferences[type] (i.e. sizeof), and _AssertLayout() below
        // states the host fact instead of a transcribed console constant.
        struct PropGraphicsReference
        {
            const PropGraphics* mpPropGraphics;  // console +0, host +0
            u8                  mu8RefCount;      // console +4, host +sizeof(void*)
        };

        bool Prepare();          // 0x... own pass
        bool Release();          // 0x...

        // ADDITIVE (prop-spawn wave phase 2, 2026-08-12) -- the whole-table reset the X360
        // folds INLINE into the head of PropEntityModule::CachePropGraphicsLists
        // @0x822DBF28: 500 iterations of `stw 0, 0(slot); stb 0, 4(slot); slot += 8` over
        // module+0xD21C4, which is this object. De-inlined here per the project convention
        // so the caller does not index a foreign private table -- and, critically, so the
        // console's 8-byte slot stride does not travel into the host build (the host slot
        // is 16 bytes; the loop below goes through sizeof). Header inline: the console
        // emits no out-of-line body. Pure addition -- no member or layout change.
        void Reset()
        {
            for (u32 luType = 0; luType < KU_MAX_PROP_TYPES; ++luType)
            {
                maPropGraphicsReferences[luType].mpPropGraphics = 0;
                maPropGraphicsReferences[luType].mu8RefCount    = 0;
            }
        }

        // 0x822A9DE8 -- register a prop-graphics record under its type id; bump ref count
        // if the slot is already populated, else install it with ref count 1. Returns true.
        bool Register(const PropGraphics* lpPropGraphics);
        bool UnRegister(u32 luType);  // 0x... own pass

        // DWARF :455 / :460. The X360 emits neither out of line (they are header inlines
        // folded at every call site), so only their DECLARATION SHAPE is attested; the
        // bodies below are the table lookups the shape and the member set force.
        const PropGraphics*     GetPropGraphics(u32 luType) const;
        const PropPartGraphics* GetPropPartGraphics(u32 luPropType, u32 luPropPartType) const;

    private:
        // DWARF :473. The install half of Register @0x822A9DE8, which the X360 folded into
        // its caller (`stw r29,0(slot); li r11,1; stb r11,4(slot)`); de-inlined here per the
        // project convention and called from Register.
        void AddPropGraphics(const PropGraphics* lpPropGraphics);
        // DWARF :477. PARKED -- no body. Its only caller, UnRegister, is not emitted in the
        // X360 image either (nothing in the ledger references it), so the refcount-decrement
        // / slot-clear policy has no attestation at all. Declaration retained (DWARF shape);
        // inventing a body would be fabrication.
        void RemovePropGraphics(u32 luType);

        PropGraphicsReference maPropGraphicsReferences[KU_MAX_PROP_TYPES]; // +0

    public:
        static void _AssertLayout();
    };
}
