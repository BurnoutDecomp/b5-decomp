#pragma once

// CgsSceneManager::EntityId — a packed 32-bit scene-entity handle: an 8-bit owner
// (entity type), a 14-bit entity index and a 10-bit part index, laid out
// owner:[31..24] | entityIndex:[23..10] | partIndex:[9..0].
//
// GROUND TRUTH (X360 asm, EntityId::Set @ 0x82277330): the pack sequence is
// `slwi r11,r29(owner),14 ; or r11,r11,r30(entityIndex) ; slwi r11,r11,10 ;
// or r11,r11,r28(partIndex)` i.e. ((owner<<14 | entityIndex) << 10) | partIndex
// = (owner<<24) | (entityIndex<<10) | partIndex — an 8/14/10 split, matching the
// DecFIGS DWARF constants (KU_NUM_BITS_FOR_ENTITY_NUM=14, KU_NUM_BITS_FOR_PART_NUM=10,
// KU_ENTITY_INDEX_BASE=10). The bounds checks in Set also confirm this: entityIndex is
// compared against 0x4000 (1<<14) and partIndex against 0x400 (1<<10).
//
// Line layout: the declarations sit on their DecFIGS DWARF lines (namespace h:27, class h:47,
// methods h:54..h:90, constants h:92..h:101, mId h:103, K_INVALID_ENTITY_ID h:106) and the
// inline bodies follow the LegacySrc header's order, so each assert lands on the line ARTIST
// bakes into every copy: Set h:116/117/118 (0x82277330 and 57 inlined sites), SetOwner h:153
// (0x8229FEF0), SetEntityIndex h:160 (0x8229FF58), SetPartIndex h:167 (inlined only).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT




namespace CgsSceneManager
{
    // u8/u16/u32 are the project's width-pinned spellings of DWARF's uint8_t/uint16_t/uint32_t,
    // and CGS_ASSERT is the X360 Begin/Fire/EndAssert tripwire.
    //
    // SetOwner and SetEntityIndex are header-inline in the original: ARTIST bakes this header's
    // path into their asserts, inlines them into callers (SetOwner into
    // VolumeInstanceId::SetEntityIDOwner 0x822B0E00, SetEntityIndex into
    // PropEntityID::SetEntityIndex 0x822B79D0) and keeps out-of-line copies where a caller
    // declined to inline (0x8229FEF0, 0x8229FF58). SetEntityIndex is still out of line here;
    // see CgsEntityId.cpp.
    //
    // SetOwner's bound is `luOwner <= 12` on the console (cmplwi r31,0xC + ble at 0x8229FF0C);
    // the LegacySrc header still had `luOwner < 8`.






    class EntityId
    {
    public:

        inline void
        Set(
            u32 luOwner,
            u32 luEntityIndex,
            u32 luPartIndex );

        inline u8
        GetOwner() const;

        inline u16
        GetEntityIndex() const;

        inline u16
        GetPartIndex() const;

        inline bool
        IsValid() const;

        inline void
        SetOwner( u8 luOwner );

        void
        SetEntityIndex( u32 luEntityIndex );

        inline void
        SetPartIndex( u32 luPartIndex );

        inline void
        SetInvalid();

        inline u32
        GetPartComparisonMask() const;

        inline bool
        IsPartOfSameEntity(
            const EntityId& lOther ) const;


        EntityId() {}
        EntityId( u32 lId ) { mId = lId; }
        operator u32 () const { return mId; }
    private:
        static const u32 KU_NUM_BITS_FOR_OWNER      = 8;
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM   = 10;
        static const u32 KU_OWNER_MASK              = 0xFF000000;
        static const u32 KU_ENTITY_INDEX_MASK       = 0x00FFFC00;
        static const u32 KU_PART_INDEX_MASK         = 0x000003FF;
        static const u32 KU_PART_INDEX_BASE         = 0;
        static const u32 KU_ENTITY_INDEX_BASE       = 10;
        static const u32 KU_OWNER_BASE              = 24;
        static const u32 KU_INVALID_ENTITY_ID       = 0xFFFFFFFF;

        u32 mId;
    };

    static const EntityId K_INVALID_ENTITY_ID = 0xFFFFFFFF;



    inline void
    EntityId::Set(
        u32 luOwner,
        u32 luEntityIndex,
        u32 luPartIndex )
    {
        CGS_ASSERT( luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM), "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)" );
        CGS_ASSERT( luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM), "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)" );
        CGS_ASSERT( luOwner < 128, "Burnout Specfic: Bad entity type set" );

        mId = 0;
        mId = ( (u32)luOwner       << KU_OWNER_BASE ) |
              ( (u32)luEntityIndex << KU_ENTITY_INDEX_BASE ) |
              ( (u32)luPartIndex   << KU_PART_INDEX_BASE );
    }

    inline u8
    EntityId::GetOwner() const
    {
        return (u8)( mId >> KU_OWNER_BASE );
    }

    inline u16
    EntityId::GetEntityIndex() const
    {
        return (u16)( ( mId & KU_ENTITY_INDEX_MASK ) >> KU_ENTITY_INDEX_BASE );
    }

    inline u16
    EntityId::GetPartIndex() const
    {
        return (u16)( ( mId & KU_PART_INDEX_MASK ) >> KU_PART_INDEX_BASE );
    }

    inline bool
    EntityId::IsValid() const
    {
        return mId != KU_INVALID_ENTITY_ID;
    }

    inline void
    EntityId::SetOwner( u8 luOwner )
    {
        CGS_ASSERT( luOwner <= 12, "Burnout Specfic: Bad entity type set" );
        mId = ( (u32)luOwner << KU_OWNER_BASE ) | ( mId & ~KU_OWNER_MASK );
    }

    // EntityId::SetEntityIndex sat here in the original (assert h:160). It is defined out of line
    // in CgsEntityId.cpp for now: the x360 tree's versions of the two callers that make ARTIST
    // emit its copy 0x8229FF58 (VehicleManager::ProcessShowtimeShunts and
    // ::HandleRaceCarTrafficCarPotentialContact) do not call it yet, so an inline definition here
    // would leave no object with the verified copy. See CgsEntityId.cpp.
    //

    inline void
    EntityId::SetPartIndex( u32 luPartIndex )
    {
        CGS_ASSERT( luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM), "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)" );
        mId = ( (u32)luPartIndex << KU_PART_INDEX_BASE ) | ( mId & ~KU_PART_INDEX_MASK );
    }

    inline void
    EntityId::SetInvalid()
    {
        mId = KU_INVALID_ENTITY_ID;
    }

    inline u32
    EntityId::GetPartComparisonMask() const
    {
        return ~KU_PART_INDEX_MASK;
    }

    inline bool
    EntityId::IsPartOfSameEntity(
        const EntityId& lOther ) const
    {
        return ( mId & ~KU_PART_INDEX_MASK ) == ( lOther.mId & ~KU_PART_INDEX_MASK );
    }
}
