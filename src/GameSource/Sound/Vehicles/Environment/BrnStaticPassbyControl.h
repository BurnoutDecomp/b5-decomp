#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_STATIC_PASSBY_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_STATIC_PASSBY_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"      // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Containers/CgsArray.h"                // committed Array<T,N> (BY NAME)
#include "rw/math/vpu/types.h"                                         // rw::math::vpu::Vector3 (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::StaticPassbyControl  (+ nested PassbyHistory)
//   GameSource/Sound/Vehicles/Environment/BrnStaticPassbyControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: StaticPassbyControl : public
// BrnEffectControl. It maintains a per-history-slot ring of recent pass-by positions
// (for static-source pass-by re-trigger suppression), keyed off a 19-slot history table.
//
// FLAG (DWARF/committed-type facts): PassbyRecord is a 32-byte aggregate {Vector3
// mvPosition; f32 mfTimeStamp}; PassbyHistory holds an Array<PassbyRecord,5> using the
// COMMITTED global Array<T,N> (CgsArray.h) -- it does NOT re-declare the count word.
// mpPhysicsControl (X360 +3408) is deliberately UNINITIALIZED by the ctor (left until
// Attach). The proximity/time constants are the two DWARF-named file-scope floats
// KF_STATIC_PASSBY_VELOCITY_THRESHOLD / KF_TIME_TO_WAIT_FOR_RETRIGGER (values deferred
// -- their rodata magnitudes are not pinned in this slice; declared for the bodies).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE.
// =============================================================================

// mpPhysicsControl is pointer-only here -> forward declaration.
namespace BrnSound { namespace Vehicles { namespace Engines { struct PhysicsControl; } } }

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct StaticPassbyControl : public BrnSound::Logic::BrnEffectControl
{
    // DWARF BrnStaticPassbyControl.h:175. One recorded pass-by (32-byte, 16-aligned).
    struct PassbyRecord
    {
        rw::math::vpu::Vector3 mvPosition;  // @ +0x00 (16 bytes, quad-aligned)
        f32                    mfTimeStamp;  // @ +0x10
    };

    // DWARF BrnStaticPassbyControl.h (PassbyHistory). A fixed 5-slot ring of pass-bys.
    struct PassbyHistory
    {
        Array<PassbyRecord, 5> mPassbyRecords;  // committed Array<T,N> (DWARF h:184/196)

        // @ 0x8269B7D0 -- refuse-if-near-existing then append; true if a new record added.
        bool Record( rw::math::vpu::Vector3 lvPosition );
        // @ 0x8269AF18 -- age every live record by dt and drop the expired ones.
        void Update( f32 lfDeltaTime );
    };

    StaticPassbyControl();          // @ 0x826B9A90
    virtual ~StaticPassbyControl(); // anchor for the vector deleting destructor @ 0x826B9B48

    // @ 0x826D0E38 -- RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    // @ +0x40 (X360). The 19-slot history table (stride 0xB0).
    PassbyHistory mafHistoryTimeouts[19];
    // @ +3408 (X360). Uninitialized by the ctor; set on Attach.
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_STATIC_PASSBY_CONTROL_H
