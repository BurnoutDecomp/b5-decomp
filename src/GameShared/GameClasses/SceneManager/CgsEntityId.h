#pragma once

// CgsSceneManager::EntityId — a packed 32-bit scene-entity handle: an 8-bit owner
// (entity type), a 12-bit entity index and a 12-bit part index, laid out
// owner:[31..24] | entityIndex:[23..12] | partIndex:[11..0].
//
// GROUND TRUTH: the Feb-2007 leak header for this exact path (recovered verbatim by
// work.py) AND the X360 masks are 8/12/12 (KU_OWNER_MASK 0xFF000000,
// KU_ENTITY_INDEX_MASK 0x00FFF000, KU_PART_INDEX_MASK 0x00000FFF). The DecFIGS DWARF
// for this header lists 14/10 split constants — those DISAGREE with both the leak and
// the committed masks, so the asm-authoritative leak layout (8/12/12) wins here.
//
// All bodies are the leak source, ported to project conventions: u8/u16/u32 for the
// width-pinned integer types and CGS_ASSERT for the X360 PrintStringed tripwires.
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

        inline void SetOwner( u8 luOwner );
        inline void SetEntityIndex( u32 luEntityIndex );
        inline void SetPartIndex( u32 luPartIndex );
        inline void SetInvalid();

        inline u32  GetPartComparisonMask() const;
        inline bool IsPartOfSameEntity( const EntityId& lOther ) const;

        EntityId() {}
        EntityId( u32 lId ) { mId = lId; }
        operator u32 () const { return mId; }

    private:
        static const u32 KU_NUM_BITS_FOR_OWNER    = 8;
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 12;
        static const u32 KU_NUM_BITS_FOR_PART_NUM  = 12;
        static const u32 KU_OWNER_MASK        = 0xFF000000;
        static const u32 KU_ENTITY_INDEX_MASK = 0x00FFF000;
        static const u32 KU_PART_INDEX_MASK   = 0x00000FFF;
        static const u32 KU_OWNER_BASE        = 24;
        static const u32 KU_ENTITY_INDEX_BASE = 12;
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

    inline void
    EntityId::SetOwner( u8 luOwner )
    {
        CGS_ASSERT( luOwner < 8, "Burnout Specfic: Bad entity type set" );
        mId = ( (u32)luOwner << KU_OWNER_BASE ) | ( mId & ~KU_OWNER_MASK );
    }

    inline void
    EntityId::SetEntityIndex( u32 luEntityIndex )
    {
        CGS_ASSERT( luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM),
                    "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)" );
        mId = ( (u32)luEntityIndex << KU_ENTITY_INDEX_BASE ) | ( mId & ~KU_ENTITY_INDEX_MASK );
    }

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
