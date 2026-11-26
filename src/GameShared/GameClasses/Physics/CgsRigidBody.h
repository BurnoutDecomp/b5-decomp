#pragma once

// CgsPhysics::RigidBodyId -- a packed 64-bit rigid-body handle. The high 32 bits hold a
// CgsSceneManager::EntityId (the owning scene entity); the low 32 bits hold the per-entity
// rigid-body index / user id. mId == (((u64)EntityId) << 32) | lowBits.
//
// GROUND TRUTH: the X360 spine for THIS build only exercises GetEntityId / SetEntityId,
// both inlined into BrnPhysics::Props::PropManager::RoutePropVsRaceCarContactToDummyCar
// (@0x825BACB0): GetEntityId reads the high dword (`ld r11,0x38; srdi r11,r11,32`), and
// SetEntityId clears the high dword then ORs the new EntityId back in
// (`stw 0,0x38; ld; or; std`), i.e. mId = (mId & 0xFFFFFFFF) | ((u64)EntityId << 32).
// The DecFIGS DWARF (CgsRigidBody.h) confirms the { u64 mId } shape and these method
// signatures. The low dword splits into two 16-bit user ids: UserIDA at bits 16..31 (byte +4)
// and UserIDB at bits 0..15 (byte +6). Evidence: BurnoutBodyPartID::Set 0x825C1A10 /
// BurnoutWheelBodyID::Set 0x825C1D40 inline Set(EntityId, u16, u16) as
// `std 0; stw 0,0 / ld / or / std; sth 0,4 / ld / or (a<<16) / std; sth 0,6 / ld / or b / std`
// (mId = 0, SetEntityId, SetUserIDA, SetUserIDB), and every pool reads its slot from the low
// 16 bits (`ld; clrlwi 16`).
//
// Mirrors the sibling value-handle CgsSceneManager::EntityId (CgsEntityId.h): all-inline,
// header-only, with a namespace-scope K_INVALID sentinel.
#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

namespace CgsPhysics
{
    class RigidBodyId
    {
    public:
        inline void                      Set( CgsSceneManager::EntityId lEntityId, u16 lu16UserIDA, u16 lu16UserIDB );   // DWARF h:54
        inline void                      Set( u64 lu64Id );                                                             // DWARF h:66
        inline void                      SetEntityId( CgsSceneManager::EntityId lEntityId );
        inline CgsSceneManager::EntityId GetEntityId() const;
        inline u8                        GetEntityIDOwner() const;
        inline u16                       GetUserIDA() const;                                                            // DWARF h:88
        inline u16                       GetUserIDB() const;                                                            // DWARF h:91
        inline u16                       GetIndex() const;
        inline void                      SetUserIDA( u16 lu16UserIDA );                                                 // DWARF h:98
        inline void                      SetUserIDB( u16 lu16UserIDB );                                                 // DWARF h:102
        inline void                      SetInvalid();
        inline bool                      IsInvalid() const;
        inline u64                       GetRawId() const;                                                              // DWARF h:111

        RigidBodyId() {}
        RigidBodyId( u64 lId ) { mId = lId; }
        operator u64 () const { return mId; }

    private:
        u64 mId;
    };

    static const RigidBodyId K_INVALID_RIGID_BODY_ID = 0xFFFFFFFFFFFFFFFFULL;


    inline void
    RigidBodyId::Set( CgsSceneManager::EntityId lEntityId, u16 lu16UserIDA, u16 lu16UserIDB )
    {
        mId = 0;
        SetEntityId( lEntityId );
        SetUserIDA( lu16UserIDA );
        SetUserIDB( lu16UserIDB );
    }

    inline void
    RigidBodyId::Set( u64 lu64Id )
    {
        mId = lu64Id;
    }

    inline void
    RigidBodyId::SetEntityId( CgsSceneManager::EntityId lEntityId )
    {
        mId &= 0xFFFFFFFFULL;
        mId |= ( (u64)(u32)lEntityId << 32 );
    }

    inline CgsSceneManager::EntityId
    RigidBodyId::GetEntityId() const
    {
        return (CgsSceneManager::EntityId)(u32)( mId >> 32 );
    }

    // DWARF CgsRigidBody.h:76 `uint8_t GetEntityIDOwner() const`. ⭐ ADDITIVE GROW 2026-08-05
    // (the rigid-body drain group): now WITNESSED in this build -- PhysicsSimulationModule::
    // ProcessRemoveAllRigidBodiesQueue @0x8289F274 inlines it as `srdi r10,r10,32` +
    // `srwi r10,r10,24` == bits 56..63 of the handle == the owner byte of the embedded
    // EntityId (CgsEntityId.h KU_OWNER_BASE == 24 on the high dword). Spelled through the
    // two committed accessors so the bit arithmetic lives in ONE place each.
    inline u8
    RigidBodyId::GetEntityIDOwner() const
    {
        return GetEntityId().GetOwner();
    }

    inline u16
    RigidBodyId::GetUserIDA() const
    {
        return (u16)( mId >> 16 );
    }

    inline u16
    RigidBodyId::GetUserIDB() const
    {
        return (u16)( mId & 0xFFFF );
    }

    inline u16
    RigidBodyId::GetIndex() const
    {
        return (u16)( mId & 0xFFFF );
    }

    inline void
    RigidBodyId::SetUserIDA( u16 lu16UserIDA )
    {
        mId &= ~( 0xFFFFULL << 16 );
        mId |= ( (u64)lu16UserIDA << 16 );
    }

    inline void
    RigidBodyId::SetUserIDB( u16 lu16UserIDB )
    {
        mId &= ~0xFFFFULL;
        mId |= (u64)lu16UserIDB;
    }

    inline void
    RigidBodyId::SetInvalid()
    {
        mId = K_INVALID_RIGID_BODY_ID;
    }

    inline bool
    RigidBodyId::IsInvalid() const
    {
        return mId == K_INVALID_RIGID_BODY_ID;
    }

    inline u64
    RigidBodyId::GetRawId() const
    {
        return mId;
    }
}
