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
// All bodies are reconstructed from the X360 asm, ported to project conventions:
// u8/u16/u32 for the width-pinned integer types and CGS_ASSERT for the X360
// PrintStringed tripwires.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsSceneManager
{
    class EntityId
    {
    public:

        inline void Set( u32 luOwner, u32 luEntityIndex, u32 luPartIndex );

        inline u8  GetOwner() const;
        inline u16 GetEntityIndex() const;
        inline u16 GetPartIndex() const;
        inline bool IsValid() const;

        // SetOwner / SetEntityIndex are defined OUT-OF-LINE in CgsEntityId.cpp (the X360
        // ARTIST build emitted them out-of-line); plain declarations here so exactly one
        // definition emits across the ~16 includers (ODR). SetPartIndex stays inline below.
        void SetOwner( u8 luOwner );
        void SetEntityIndex( u32 luEntityIndex );
        inline void SetPartIndex( u32 luPartIndex );
        inline void SetInvalid();

        inline u32  GetPartComparisonMask() const;
        inline bool IsPartOfSameEntity( const EntityId& lOther ) const;

        EntityId() {}
        EntityId( u32 lId ) { mId = lId; }
        operator u32 () const { return mId; }

    private:
        static const u32 KU_NUM_BITS_FOR_OWNER    = 8;
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM  = 10;
        static const u32 KU_OWNER_MASK        = 0xFF000000;
        static const u32 KU_ENTITY_INDEX_MASK = 0x00FFFC00;
        static const u32 KU_PART_INDEX_MASK   = 0x000003FF;
        static const u32 KU_OWNER_BASE        = 24;
        static const u32 KU_ENTITY_INDEX_BASE = 10;
        static const u32 KU_PART_INDEX_BASE   = 0;
        static const u32 KU_INVALID_ENTITY_ID = 0xFFFFFFFF;

        u32 mId;
    };

    static const EntityId K_INVALID_ENTITY_ID = 0xFFFFFFFF;


    inline void
    EntityId::Set( u32 luOwner, u32 luEntityIndex, u32 luPartIndex )
    {
        CGS_ASSERT( luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM),
                    "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)" );
        CGS_ASSERT( luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM),
                    "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)" );
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

    // EntityId::SetOwner and EntityId::SetEntityIndex are defined out-of-line in
    // CgsEntityId.cpp (see that TU). They were moved out of this header to avoid a
    // duplicate-definition/ODR conflict across the header's many includers.

    inline void
    EntityId::SetPartIndex( u32 luPartIndex )
    {
        CGS_ASSERT( luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM),
                    "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)" );
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
    EntityId::IsPartOfSameEntity( const EntityId& lOther ) const
    {
        return ( mId & ~KU_PART_INDEX_MASK ) == ( lOther.mId & ~KU_PART_INDEX_MASK );
    }
}
