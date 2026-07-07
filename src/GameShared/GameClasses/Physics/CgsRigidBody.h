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
// signatures. Members whose exact low-32 split (the DWARF-declared UserIDA/UserIDB and the
// multi-arg Set() packers) is NOT evidenced by this build's asm are intentionally omitted
// rather than guessed (they carry heavy version drift from the Feb-2007 tree).
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
        inline void                      SetEntityId( CgsSceneManager::EntityId lEntityId );
        inline CgsSceneManager::EntityId GetEntityId() const;
        inline u16                       GetIndex() const;
        inline void                      SetInvalid();
        inline bool                      IsInvalid() const;

        RigidBodyId() {}
        RigidBodyId( u64 lId ) { mId = lId; }
        operator u64 () const { return mId; }

    private:
        u64 mId;
    };

    static const RigidBodyId K_INVALID_RIGID_BODY_ID = 0xFFFFFFFFFFFFFFFFULL;


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

    inline u16
    RigidBodyId::GetIndex() const
    {
        return (u16)( mId & 0xFFFF );
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
}
