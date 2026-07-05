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
// 11 reconstructed bodies; on our x64 PC compile pointer-width widening shifts absolute
// offsets, so these are provenance, not host asserts -- we model the PC layout and do
// not byte-match):
//   mCellManager            @ +0        (PropCellManager, console sizeof 2432)
//   maProps[5418]           @ +2432     (PropEntityInstance, 80B each; 9 slots * 602)
//   maParts[4518]           @ +435872   (PropPartEntityInstance, 80B each; 9 slots * 502)
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
    static const u32 KU_PROPS_POOL_SIZE           = 5418; // 9 * KU_SIZE_OF_PROP_ZONE_SLOT
    static const u32 KU_PARTS_POOL_SIZE           = 4518; // 9 * KU_SIZE_OF_PART_ZONE_SLOT
    static const u32 KU_MAX_ROTATION_PARAMS       = 100;
    static const u32 KU_MAX_TRAFFIC_LIGHTS_TO_RESTORE = 80;

    // Profile::HitPropsBitArray == BitArray<300000> (DWARF BrnProfile.h:555).
    typedef CgsContainers::BitArray<300000u> HitPropsBitArray;

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

        // Resolve the prop / part instance at (zone, index-within-zone). Both assert
        // luZoneId < KU_MAX_ZONES and return a pointer into maProps / maParts at the zone's
        // start slot + the index (X360 GetProp 0x822DC3A8-class accessor inlined into the
        // debug overlay's render passes). GetProp may return null for an empty slot.
        PropEntityInstance*     GetProp(u16 lu16ZoneId, u32 luPropIndex) const;
        PropPartEntityInstance* GetPart(u16 lu16ZoneId, u32 luPartIndex) const;

        // 0x822FC168 -- allocate a prop slot + a part slot for the zone, load every prop
        // (LoadProp, unrolled x4 in the X360 build), and register the zone's cells.
        s32  LoadZone(const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                      Vector3 lPlayerPosition, PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // 0x82303790 -- remove every prop/part of the zone from scene/sim/contact-gen,
        // free its rotation-params slots, and deallocate its prop/part pool slots.
        void UnloadZone(u16 lu16ZoneId, const PropPhysicsDataHeader* lpTypes,
                        void* lpRecentlyBrokenProps, PropEntityIO::OutputBuffer_PreScene* lpOutput);

        // 0x822F0920 -- per-frame physics-result apply for one prop or part instance.
        void UpdateInstance(PropEntityID lEntityId, Matrix44Affine lTransform,
                            Vector3 lLinearVelocity, Vector3 lAngularVelocity, bool lbFrozen,
                            const PropPhysicsDataHeader* lpTypeData, f32 lfTimeStep,
                            PropEntityIO::OutputBuffer_PostPhysics* lpOutput);

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
        void AddPropPartsToScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene); // 0x822F06A0
        void RemovePropFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                 PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene); // 0x822F0740
        void RemovePropPartsFromScene(PropEntityInstance* lpProp, const PropTypeData* lpType,
                                      PropVolumeInstanceID lVolumeInstanceID, InSceneUpdateInterface* lpScene); // 0x822F0880
        void RemovePropFromSim(PropEntityInstance* lpProp, const PropTypeData* lpType,
                               PropVolumeInstanceID lVolumeInstanceID,
                               PropEntityIO::OutputBuffer_PreScene* lpOutput);    // 0x822F07E0

    private:
        // 0x822DF050 / 0x822DF1F8 -- own TU.
        s32  AllocatePropInstancesBlock(u32 luSizeOfBlock);
        s32  AllocatePartInstancesBlock(u32 luSizeOfBlock);
        // 0x822C5C40 / 0x822C5E18 -- defined in this TU.
        void DeallocatePropInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock);
        void DeallocatePartInstancesBlock(u32 luStartOfBlock, u32 luSizeOfBlock);
        // 0x822F2EF0 -- own TU (load one prop + its parts into the pools).
        void LoadProp(s32* lpiPropPoolIndex, s32* lpiPartPoolIndex, s32 liZoneDataPropIndex,
                      const PropZoneData* lpZoneData, const PropPhysicsDataHeader* lpTypes,
                      Vector3 lPlayerPosition, InSceneUpdateInterface* lpScene);

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
        struct PropGraphicsReference
        {
            const PropGraphics* mpPropGraphics;  // +0
            u8                  mu8RefCount;      // +4
            u8                  mu8Pad0[3];       // -> 8-byte slot (X360 `8*type + base`)
        };

        bool Prepare();          // 0x... own pass
        bool Release();          // 0x...
        // 0x822A9DE8 -- register a prop-graphics record under its type id; bump ref count
        // if the slot is already populated, else install it with ref count 1. Returns true.
        bool Register(const PropGraphics* lpPropGraphics);
        bool UnRegister(u32 luType);  // 0x... own pass
        const PropGraphics*     GetPropGraphics(u32 luType) const;
        const PropPartGraphics* GetPropPartGraphics(u32 luPropType, u32 luPropPartType) const;

    private:
        void AddPropGraphics(const PropGraphics* lpPropGraphics);
        void RemovePropGraphics(u32 luType);

        PropGraphicsReference maPropGraphicsReferences[KU_MAX_PROP_TYPES]; // +0 (8B slots)
    };
}
