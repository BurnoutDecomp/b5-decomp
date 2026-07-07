// GameSource/Physics/PropManager/BrnPropManager_RoutePropVsRaceCarContactToDummyCar.cpp
//
// BrnPhysics::Props::PropManager::RoutePropVsRaceCarContactToDummyCar  (X360 0x825BACB0).
//
// A one-function slice split out of the (still-blocked) BrnPropManager.cpp: this private
// helper never dereferences `this` (the X360 body ignores r3 entirely), so it does NOT
// depend on the ungroundable PropManager class layout and can be reconstructed on its own.
//
// Behaviour (store-for-store against the X360 asm; the DecFIGS DWARF at
// BrnPropManager.cpp:1726 confirms the { bool, InAddPotentialContact* } signature and the
// `EntityId lDummyRaceCarEntityId` local): pick the race-car side of the potential contact
// -- mIDB when the PROP is entity A, otherwise mIDA -- pull its EntityId out of the high
// dword, stamp the EntityId owner-type to the shared dummy-race-car owner (11), and write
// the EntityId back into the RigidBodyId's high dword. In asm:
//   ld r11,0xNN(contact); srdi r11,r11,32; stw r11,eid      // eid = RigidBodyId.GetEntityId()
//   bl EntityId::SetOwner(&eid, 0xB)                         // eid.SetOwner(11)
//   stw 0,0xNN(contact); ld; extldi (eid<<32); or; std       // RigidBodyId.SetEntityId(eid)
// where 0xNN == 0x38 (mIDB) when the prop is entity A, else 0x30 (mIDA).
//
// InAddPotentialContact's committed home (CgsPhysicsSimulationIO_Events.h) models the
// 80-byte payload as an opaque span (a cross-family CgsPhysics header this slice must not
// edit), so the two RigidBodyId fields are addressed at their byte offsets. Those offsets
// are doubly attested: the X360 asm loads 0x30/0x38, and the DecFIGS DWARF for
// InAddPotentialContact (CgsPhysicsSimulationModuleIO.h:200-208) lays the record out as
// Vector3 mPointOnA@0x00, mPointOnB@0x10, mNormal@0x20, RigidBodyId mIDA@0x30, mIDB@0x38.
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

namespace BrnPhysics
{
namespace Props
{
    // EntityId owner-type of the shared "dummy" race car (X360 immediate `li r4,0xB`).
    static const u8 KU_DUMMY_RACE_CAR_OWNER = 11;

    // Byte offsets of the two RigidBodyId fields inside InAddPotentialContact (opaque in the
    // committed CgsPhysics event-payload header). DWARF CgsPhysicsSimulationModuleIO.h:203-204
    // + X360 asm ld 0x30 / ld 0x38.
    static const u32 KU_OFF_MIDA = 0x30;   // RigidBodyId mIDA (race-car side when prop is entity B)
    static const u32 KU_OFF_MIDB = 0x38;   // RigidBodyId mIDB (race-car side when prop is entity A)

    void
    PropManager::RoutePropVsRaceCarContactToDummyCar(
        bool                                                   lbPropIsEntityA,
        CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact )
    {
        u8* lpContactBytes = reinterpret_cast<u8*>( lpOutContact );

        if ( lbPropIsEntityA )
        {
            CgsPhysics::RigidBodyId& lRaceCarId =
                *reinterpret_cast<CgsPhysics::RigidBodyId*>( lpContactBytes + KU_OFF_MIDB );

            CgsSceneManager::EntityId lDummyRaceCarEntityId = lRaceCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( KU_DUMMY_RACE_CAR_OWNER );
            lRaceCarId.SetEntityId( lDummyRaceCarEntityId );
        }
        else
        {
            CgsPhysics::RigidBodyId& lRaceCarId =
                *reinterpret_cast<CgsPhysics::RigidBodyId*>( lpContactBytes + KU_OFF_MIDA );

            CgsSceneManager::EntityId lDummyRaceCarEntityId = lRaceCarId.GetEntityId();
            lDummyRaceCarEntityId.SetOwner( KU_DUMMY_RACE_CAR_OWNER );
            lRaceCarId.SetEntityId( lDummyRaceCarEntityId );
        }
    }
}
}
