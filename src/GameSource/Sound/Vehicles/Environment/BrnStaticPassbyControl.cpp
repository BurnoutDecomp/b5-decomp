#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
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

// Original mutable debug constants (DWARF cpp:34/37), image82F2CE0C/10.
f32 KF_STATIC_PASSBY_VELOCITY_THRESHOLD = 50.0f;
f32 KF_TIME_TO_WAIT_FOR_RETRIGGER = 1.0f;
// Record's vector comparison reads 820AA0E0, not the speed threshold.
static const f32 KF_POSITION_TOLERANCE = 0x1p-16f;

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
// |record.mvPosition - lvPosition| and compare it against KF_POSITION_TOLERANCE. If NO component exceeds the threshold -- the new position lies inside the
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

        const bool lbAnyGreater = (lfAbsX > KF_POSITION_TOLERANCE) ||
                                  (lfAbsY > KF_POSITION_TOLERANCE) ||
                                  (lfAbsZ > KF_POSITION_TOLERANCE);
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
        // 8269AF9C bge ->AFB0 retains >=0 OR unordered; <0 ->AFA0 erases.
        if ( !(lrRecord.mfTimeStamp < 0.0f) )
        {
            ++luIndex;
        }
        else
        {
            mPassbyRecords.Erase(luIndex);
        }
    }
}

// StaticPassby's controller slot is ICF-folded onto 82685D38, named MusicEffect
// in ARTIST. Vtable820B1FE8 contains that exact pointer: slot0=physics, others=-1.
s32 StaticPassbyControl::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

// ARTIST82686228 (adjusted EffectBase this).
void StaticPassbyControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController->GetEffectID() == 0, "Unexpected control.");
    if (apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<Engines::PhysicsControl*>(apController);
}

// ARTIST8269B738: inlined EffectBase::Attach followed by nineteen count clears.
bool StaticPassbyControl::Attach()
{
    CgsSound::Logic::EffectBase::Attach();
    for (s32 liType = 0; liType < 19; ++liType)
        mafHistoryTimeouts[liType].mPassbyRecords.Clear();
    return true;
}

// ARTIST826FE1C0; velocity is MPH at physics+0x128, position at physics+0xA0.
void StaticPassbyControl::UpdateParams(f32 afTimeStep)
{
    const auto& lrPhysics = mpPhysicsControl->GetPhysicsData();
    const Vector3 lPosition = lrPhysics.mPosition3d.GetCurrent();
    const f32 lfSpeed = lrPhysics.mSpeedMPH.GetCurrent();
    auto* lpStateBase = GetStateBase();
    CGS_ASSERT(lpStateBase != nullptr, "lpStateBase");
    auto* lpPlayerStateMan = static_cast<const PlayerVehicleStateManager*>(lpStateBase->GetStateManager());
    CGS_ASSERT(lpPlayerStateMan != nullptr, "lpPlayerStateMan");
    ProcessPassbys(lPosition, lfSpeed, lpPlayerStateMan);
    UpdateHistory(afTimeStep);
}

// ARTIST826F51B0, DWARF cpp:203. Query runs even below the speed threshold.
void StaticPassbyControl::ProcessPassbys(Vector3 lvPosition, f32 afSpeed,
                                       const PlayerVehicleStateManager* apPlayerStateMan)
{
    CGS_ASSERT(apPlayerStateMan != nullptr, "lpPlayerStateMan");
    BrnSound::World::StaticSoundEntity laEntities[16];
    const s32 liCount = apPlayerStateMan->Query(lvPosition, 30.0f, laEntities, 16,
                                               KB_SHOW_STATIC_ENVIRONMENT);
    // 826F5234 blt ->5268 skips; fallthrough ->5238 includes unordered speed.
    if (!(afSpeed < KF_STATIC_PASSBY_VELOCITY_THRESHOLD) && liCount > 0)
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
            TriggerPassby(laEntities[liIndex]);
}

// ARTIST826B9BF0: the packed position retains its W metadata in both copies.
void StaticPassbyControl::TriggerPassby(const BrnSound::World::StaticSoundEntity& arEntity)
{
    const s32 liType = arEntity.GetType();
    CGS_ASSERT(liType < 19, "lePassbyType < ePassbyTypes::MaxPassbyTypes");
    const auto& lrPackedPos = arEntity.GetPosPlus();
    const Vector3 lPosition = {lrPackedPos.x, lrPackedPos.y, lrPackedPos.z, lrPackedPos.w};
    if (mafHistoryTimeouts[liType].Record(lPosition))
    {
        auto* lpModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        auto* lpManager = static_cast<BrnSound::Logic::Passby::PassbyStateManager*>(
            lpModule->GetEnvironment().GetStateManager(4));
        const BrnSound::Logic::Passby::PassbyStateManager::Passby lPassby(
            lPosition, 0.0f, static_cast<AttribSys::Enums::ePassbyTypes::ePassbyTypes>(liType), false, 1.0f);
        lpManager->PostPassby(lPassby);
        static s32 siDiag = 0;
        if (SndEnvDiagBudget(siDiag))
            *CgsDev::Log::gpDebugPrint << "[sndenv] static passby type=" << liType
                << " pos=(" << lPosition.x << "," << lPosition.y << "," << lPosition.z
                << ") [FLAG PC witness]\n";
    }
}

// ARTIST8269B770 (raw image; missing standalone export): nineteen history calls.
void StaticPassbyControl::UpdateHistory(f32 afTimeStep)
{
    for (s32 liType = 0; liType < 19; ++liType)
        mafHistoryTimeouts[liType].Update(afTimeStep);
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
