#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h
//
// BrnWorld::PropEntityInstance / PropPartEntityInstance / InstanceIndex and the
// PropEntityRotationParams record + EPropState enum.
//
// Reconstructed from the DecFIGS DWARF (member names/types/order, X360-gated) and
// verified field-by-field against the BURNOUT_X360_ARTIST.XEX asm for the
// PropZoneManager TU that embeds these types by value:
//   - PropEntityInstance: mWorldTransform@0 (Matrix44Affine,64B), muInstanceID@64,
//     muTypeId@68(u16), mu16ZoneIndex@70, mu16PartsIndex@72, mu8Flags@74,
//     mi8RotationParamsIndex@75, mu8PhysicsIndex@76, mu8State@77, mu8NextState@78.
//     The asm reads exactly these offsets (e.g. RemovePropFromScene `lhz r,0x46(prop)`
//     == mu16ZoneIndex@70; RemovePropFromSim `lbz r,0x4D(prop)` == mu8State@77;
//     UnloadZone `lbz ...,74` == mu8Flags, `lbz ...,75` == mi8RotationParamsIndex).
//     sizeof == 80 (16-aligned, Matrix44Affine forces alignas(16)).
//   - PropPartEntityInstance: mWorldTransform@0, muTypeId@64, muPartId@66,
//     mu16ZoneIndex@68, mu8PhysicsIndex@70, mbPhysical@71. sizeof == 80.
//     (UpdateInstance writes `*(part+66)=muPartId`, `*(part+70)=physIndex`,
//     `*(part+71)=mbPhysical`.)
//   - InstanceIndex: a packed { isPart, index } pair the older Feb-2007 layout kept
//     in the manager. The shipped X360 build no longer stores a per-instance index
//     table in PropZoneManager (it derives prop/part slots from zone start indices),
//     so InstanceIndex is kept here only as the small helper the GetProp/GetPart
//     accessors used; it is not embedded by the X360 PropZoneManager layout.
//
// Method DECLARATIONS are gated on the X360 ledger (each Class::Fn the recon calls or
// the DWARF attests for this build). Bodies belong to their own TUs; the per-TU
// compile gate needs only the declarations.
#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine

namespace BrnPhysics { namespace Props { class PropInstanceData; class PropTypeData; } }

namespace BrnWorld
{
    struct PropEntityID;   // packed entity id (declared in the entity-types TU)

    // BrnPropEntityInstance.h:35 (DWARF) -- the prop simulation state machine.
    enum EPropState
    {
        E_NON_PHYSICAL                   = 0,
        E_STATIC                         = 1,
        E_LEANING                        = 2,
        E_PHYSICAL_WITH_EXTRA_COM_OFFSET = 3,
        E_PHYSICAL                       = 4,
        E_MOVED                          = 5,
        E_SMASHED                        = 6,
        E_STATE_COUNT                    = 7
    };

    // mu8Flags bit indices / masks (DWARF BrnPropEntityInstance.h:232-260).
    static const u16 KU_PHYSICS_ENABLED_FLAG_INDEX   = 0;
    static const u16 KU_ADDED_TO_SCENE_FLAG_INDEX    = 1;
    static const u16 KU_ADDED_TO_CONTACT_GEN_FLAG_INDEX = 2;
    static const u16 KU_SMASHABLE_FLAG_INDEX         = 3;
    static const u16 KU_ANIMATED_DIRECTION           = 4;
    static const u16 KU_MOVED_FLAG_INDEX             = 5;
    static const u16 KU_LAMPPOST_FLAG_INDEX          = 6;

    static const u8 KU_PHYSICS_ENABLED_BIT     = 1;
    static const u8 KU_ADDED_TO_SCENE_BIT      = 2;   // mu8Flags & 2 -> in scene
    static const u8 KU_ADDED_TO_CONTACT_GEN_BIT = 4;  // mu8Flags & 4 -> in contact gen
    static const u8 KU_SMASHABLE_BIT           = 8;
    static const u8 KU_ANIMATED_DIRECTION_BIT  = 16;
    static const u8 KU_MOVED_BIT               = 32;
    static const u8 KU_LAMPPOST_BIT            = 64;

    // BrnPropEntityInstance.h:48 (DWARF) -- per-prop animated-rotation parameters.
    struct PropEntityRotationParams
    {
        s16 miPropIndex;   // :50
        s8  mnRotSpeed;    // :51
        u8  muMinAngle;    // :52
        u8  muMaxAngle;    // :53
    };

    // BrnPropEntityInstance.h:57 (DWARF) -- one loaded prop instance.
    struct alignas(16) PropEntityInstance
    {
    public:
        // Lifecycle / queries the X360 ledger attests (bodies in this type's own TU).
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

        // @ 0x822B80E8. Fill this slot from its loaded PropInstanceData record + shared
        // PropTypeData descriptor (zone/flags/transform/instance id + rotation-params range).
        void InitialiseFromData(const BrnPhysics::Props::PropInstanceData* lpInstanceData,
                                const BrnPhysics::Props::PropTypeData*     lpTypeData,
                                PropEntityID                               liEntityID,
                                u16                                        lu16ZoneIndex,
                                s32                                        liRotationParamsIndex);

        u16 GetZone() const;
        u32 GetTypeId() const;
        u32 GetInstanceID() const;
        u16 GetFirstPartIndex() const;

        const Matrix44Affine& GetWorldTransform() const;
        Matrix44Affine&       GetWorldTransform();

        EPropState GetState() const;
        void       SetState(EPropState leState);
        EPropState GetNextState() const;
        void       SetNextState(EPropState leState);

        bool IsPhysical() const;     // mu8State == E_PHYSICAL
        bool IsSmashed() const;      // 0x822B82C8
        bool IsAddedToScene() const;
        bool IsAddedToContactGen() const;
        bool IsAnimated() const;
        bool IsLamppost() const;

        s32 GetRotationParamsIndex() const;   // mi8RotationParamsIndex
        s32 GetPhysicsIndex() const;
        void SetPhysicsIndex(s32 liIndex);

    public:
        // ---- DWARF-faithful layout (offsets asm-verified for this TU) ----
        Matrix44Affine mWorldTransform;        // +0   (64B, 16-aligned)
        u32            muInstanceID;           // +64
        u16            muTypeId;               // +68
        u16            mu16ZoneIndex;          // +70  zone id (asm: lhz 0x46)
        u16            mu16PartsIndex;         // +72  first part slot
        u8             mu8Flags;               // +74  KU_*_BIT mask
        s8             mi8RotationParamsIndex; // +75  -1 == none
        u8             mu8PhysicsIndex;        // +76
        u8             mu8State;               // +77  EPropState
        u8             mu8NextState;           // +78
        u8             mu8Pad0;                // +79  -> sizeof 80
    };
    static_assert(sizeof(PropEntityInstance) == 80, "PropEntityInstance sizeof 80");

    // BrnPropPartEntityInstance.h:33 (DWARF) -- one loaded prop *part* instance.
    struct alignas(16) PropPartEntityInstance
    {
    public:
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

    public:
        Matrix44Affine mWorldTransform;  // +0
        u16            muTypeId;          // +64
        u16            muPartId;          // +66
        u16            mu16ZoneIndex;     // +68
        u8             mu8PhysicsIndex;   // +70
        bool           mbPhysical;        // +71
        u8             mu8Pad0[8];        // +72 -> sizeof 80
    };
    static_assert(sizeof(PropPartEntityInstance) == 80, "PropPartEntityInstance sizeof 80");

    // Feb-2007 InstanceIndex helper (kept for GetProp/GetPart shape; not embedded by
    // the shipped X360 PropZoneManager layout). A packed { isPart, index } pair.
    struct InstanceIndex
    {
        void Construct(bool lbIsPart, u16 luIndex)
        {
            mu16IsPartFlag = lbIsPart ? 1 : 0;
            mu16Index = luIndex;
        }
        u16  GetIndex() const          { return mu16Index; }
        void SetIndex(u16 luIndex)     { mu16Index = luIndex; }
        bool IsPart() const            { return mu16IsPartFlag == 1; }
        void SetIsPart(bool lbIsPart)  { mu16IsPartFlag = lbIsPart ? 1 : 0; }

        u16 mu16IsPartFlag;
        u16 mu16Index;
    };
}
