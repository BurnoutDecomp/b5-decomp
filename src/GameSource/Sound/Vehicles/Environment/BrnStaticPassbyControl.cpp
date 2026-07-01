#include "GameSource/Sound/Vehicles/Environment/BrnStaticPassbyControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::StaticPassbyControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   StaticPassbyControl::CreateObject(u32)        @ 0x826D0E38  (the RTTI factory hook)
//   StaticPassbyControl::StaticPassbyControl      @ 0x826B9A90  (ctor)
//   StaticPassbyControl::`vector deleting dtor'   @ 0x826B9B48  (-> ~StaticPassbyControl)
//   PassbyHistory::Record                         @ 0x8269B7D0
//   PassbyHistory::Update                         @ 0x8269AF18
//
// The out-of-line Array<PassbyRecord,5>::Append/Erase/GetItem the X360 emits per-using-TU
// are instantiated in CgsArrayStaticPassbyRecord5.cpp (the generic body is inline in
// CgsArray.h).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// DWARF-named file-scope constants (BrnStaticPassbyControl.cpp:34/37). Their exact
// rodata magnitudes are not pinned in this slice (deferred); declared for the bodies.
// FLAG: values UNVERIFIED -- placeholders for the proximity / re-trigger tuning.
static const f32 KF_STATIC_PASSBY_VELOCITY_THRESHOLD = 0.0f;
static const f32 KF_TIME_TO_WAIT_FOR_RETRIGGER       = 0.0f;

// ---------------------------------------------------------------------------
// StaticPassbyControl::CreateObject(u32)  @ 0x826D0E38   (the RTTI factory hook)
// Allocates a 0xD60 (3424) byte block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "StaticPassbyControl" and inline-constructs a StaticPassbyControl,
// upcast to CgsSound::Logic::EffectControl* (+4). `luType` only selects the operator-new
// flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0xD60 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* StaticPassbyControl::CreateObject( u32 /*luType*/ )
{
    return new StaticPassbyControl();
}

// ---------------------------------------------------------------------------
// StaticPassbyControl::StaticPassbyControl()  @ 0x826B9A90
//
// After the (X360-inlined) BrnEffectControl base ctor chain installs the two leaf vptrs
// and value-inits the base members, this leaf body brings the 19-entry PassbyHistory
// table at this+0x40 to its unconstructed state: each PassbyHistory's Array<PassbyRecord,
// 5> has its count word set to the -1 (KI_UNCONSTRUCTED) sentinel.
//
// NOTE: mpPhysicsControl (X360 +3408) is deliberately NOT initialised here -- the X360
// ctor stores nothing to that offset (indeterminate until Attach).
// ---------------------------------------------------------------------------
StaticPassbyControl::StaticPassbyControl()
{
    // X360: 19 PassbyHistory sub-objects at this+0x40, stride 0xB0. Each history's
    // Array<PassbyRecord,5> has its count word set to the -1 (unconstructed) sentinel.
    for ( u32 luHistory = 0; luHistory < 19; ++luHistory )
    {
        mafHistoryTimeouts[ luHistory ].mPassbyRecords.MarkUnconstructed();
    }
}

// ---------------------------------------------------------------------------
// ~StaticPassbyControl  @ 0x826B9B48  (anchor for the X360 `vector deleting destructor').
// The observable member teardown lives in the inherited ~BrnEffectControl base chain;
// the PassbyHistory / Array / PassbyRecord sub-objects are trivially destructible, so
// this leaf body is empty. The (a2 & 1) allocator-free tail is left to the host
// toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
StaticPassbyControl::~StaticPassbyControl()
{
}

// ---------------------------------------------------------------------------
// PassbyHistory::Record  @ 0x8269B7D0
//   bool Record(const rw::math::vpu::Vector3 lvPosition)
//
// Refuse-if-near-existing then append. For each live record, compute the componentwise
// |record.mvPosition - lvPosition| and compare it against KF_STATIC_PASSBY_VELOCITY_
// THRESHOLD. If NO component exceeds the threshold -- the new position lies inside the
// proximity box of an already-recorded pass-by -- the query is a re-trigger and Record
// returns false. Otherwise, if the fixed 5-slot buffer is full, return false; else
// append a fresh record at lvPosition with its countdown seeded to
// KF_TIME_TO_WAIT_FOR_RETRIGGER, and return true.
//
// FLAG: Vector3 is the vendor POD {x,y,z,w}; the X360 VMX componentwise |delta| vs the
// broadcast threshold is expressed here on the plain x/y/z floats.
// ---------------------------------------------------------------------------
bool StaticPassbyControl::PassbyHistory::Record( rw::math::vpu::Vector3 lvPosition )
{
    for ( u32 luIndex = 0; luIndex < mPassbyRecords.GetLength(); ++luIndex )
    {
        const PassbyRecord& lrRecord = mPassbyRecords[luIndex];

        const f32 lfDeltaX = lrRecord.mvPosition.x - lvPosition.x;
        const f32 lfDeltaY = lrRecord.mvPosition.y - lvPosition.y;
        const f32 lfDeltaZ = lrRecord.mvPosition.z - lvPosition.z;
        const f32 lfAbsX = lfDeltaX < 0.0f ? -lfDeltaX : lfDeltaX;
        const f32 lfAbsY = lfDeltaY < 0.0f ? -lfDeltaY : lfDeltaY;
        const f32 lfAbsZ = lfDeltaZ < 0.0f ? -lfDeltaZ : lfDeltaZ;

        const bool lbAnyGreater = (lfAbsX > KF_STATIC_PASSBY_VELOCITY_THRESHOLD) ||
                                  (lfAbsY > KF_STATIC_PASSBY_VELOCITY_THRESHOLD) ||
                                  (lfAbsZ > KF_STATIC_PASSBY_VELOCITY_THRESHOLD);
        if ( !lbAnyGreater )
        {
            return false;
        }
    }

    if ( mPassbyRecords.IsFull() )
    {
        return false;
    }

    PassbyRecord lRecord;
    lRecord.mvPosition  = lvPosition;
    lRecord.mfTimeStamp = KF_TIME_TO_WAIT_FOR_RETRIGGER;
    mPassbyRecords.Append(lRecord);
    return true;
}

// ---------------------------------------------------------------------------
// PassbyHistory::Update  @ 0x8269AF18
//   void Update(float32_t)
//
// Age every live record by lfDeltaTime and drop the expired ones. Each record's
// countdown mfTimeStamp is decremented by the frame delta; if still >= 0 the record
// survives and we advance, otherwise it is Erase()d (order-preserving shift-down) and the
// index is NOT advanced so the element shifted in is re-examined.
// ---------------------------------------------------------------------------
void StaticPassbyControl::PassbyHistory::Update( f32 lfDeltaTime )
{
    u32 luIndex = 0;
    while ( luIndex < mPassbyRecords.GetLength() )
    {
        PassbyRecord& lrRecord = mPassbyRecords[luIndex];
        lrRecord.mfTimeStamp -= lfDeltaTime;
        if ( lrRecord.mfTimeStamp >= 0.0f )
        {
            ++luIndex;
        }
        else
        {
            mPassbyRecords.Erase(luIndex);
        }
    }
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
