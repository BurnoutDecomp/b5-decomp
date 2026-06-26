#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Passby ctor range tripwire)

// =============================================================================
// BrnSound::Logic::Passby::PassbyStateManager::DynamicPropByCache -- out-of-line
// body for the single ledger function owned by this TU:
//   DynamicPropByCache::Update  @ 0x82683360
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The cache holds KU_DYNAMIC_PROP_CACHE_SIZE recent dynamic-prop passbys so the
// same prop is not re-triggered every frame. Update() ages every active entry:
// once an entry has been live for >= 5 seconds it is cleared so the prop becomes
// eligible to trigger again.
//
// SIGNATURE NOTE: the committed declaration is `void Update(f32 lfTimeStep)`, but
// the X360 asm at 0x82683360 (and its sole call site inside UpdateDynamicPropBys
// @ 0x826A0E48..0x826A0E58, which loads `lfs f28, 4(rThis)` -- the manager's
// running game clock -- and passes it as the argument) shows the argument is the
// CURRENT game time, NOT a per-frame delta: each entry's expiry test is
// `(currentTime - entry.mfTimeStamp) >= 5.0`. The parameter is therefore named
// lfCurrentTime here. FLAG: the committed header parameter name `lfTimeStep` is
// misleading; rename it to `lfCurrentTime` when that home is next grown (the type
// f32 already matches the single-precision load at the call site).
//
// LAYOUT (recovered from the asm): maItems is processed at a 12-byte stride
// (Item = bool mbActive @+0 padded to 4, f32 mfTimeStamp @+4, EntityId mId @+8),
// four items per unrolled loop iteration over 8 iterations == 32 entries. Bodied
// by NAME (range loop over maItems) -- no absolute offsets are asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// Expiry window: an active cache entry is cleared once it has been live for at
// least this many seconds. Recovered from the X360 rodata literal compared by
// every fcmpu in the loop (flt_820ABCD8 == 5.0f).
static const f32 KF_PROP_BY_CACHE_LIFETIME = 5.0f;

// ---------------------------------------------------------------------------
// DynamicPropByCache::Item::Item  -- default ctor for a cache slot. The cache's
// maItems[] array default-constructs each Item; an empty slot starts inactive with a
// zeroed timestamp/id. (X360/PS3 inline this per-element zero-init; reconstructed as the
// obvious POD-clearing default ctor.)
// ---------------------------------------------------------------------------
PassbyStateManager::DynamicPropByCache::Item::Item()
    : mbActive( false )
    , mfTimeStamp( 0.0f )
{
    mId.muValue = 0;
}

// ---------------------------------------------------------------------------
// DynamicPropByCache::Update  @ 0x82683360
//   For every entry: if it is active and has aged past KF_PROP_BY_CACHE_LIFETIME
//   relative to the supplied current time, deactivate it. Inactive entries and
//   still-fresh entries are left as-is.
//
//   Boolean parity with the asm: mbActive becomes
//     mbActive && ( (lfCurrentTime - mfTimeStamp) < KF_PROP_BY_CACHE_LIFETIME )
// ---------------------------------------------------------------------------
void PassbyStateManager::DynamicPropByCache::Update( f32 lfCurrentTime )
{
    for( u32 luIndex = 0; luIndex < PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE; ++luIndex )
    {
        Item& lrItem = maItems[ luIndex ];

        if( lrItem.mbActive
            && ( ( lfCurrentTime - lrItem.mfTimeStamp ) >= KF_PROP_BY_CACHE_LIFETIME ) )
        {
            lrItem.mbActive = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Passby::Passby( const Cgs3dEffectControl*, f32, EePassbyTypes, bool, f32 )
//   @ 0x826832F0
//
// Build a posted-passby record from a live 3D effect control. Recovered store-
// for-store from the X360 asm (offsets are this struct's field offsets, all
// reached BY NAME here):
//   stw    a2,  0x10(this)   ; mp3dControl                  = lp3dControl
//   stfs   f1,  0x14(this)   ; mfRelativeVelocityMagnitude  = lfRelativeVelocityMagnitude
//   stw    a6,  0x18(this)   ; meType                       = leType
//   stfs   f2,  0x1C(this)   ; mfVolumeModifier             = lfVolumeModifier
//   stvx128 v0(=vspltisw 0), this  ; mStaticPos (16B Vector3) = {0,0,0,(pad)}
//   stb    a7,  0x20(this)   ; mbSuppressBoostBys           = lbSuppressBoostBys
//   if (leType >= MaxPassbyTypes)
//       assert("leType < ...MaxPassbyTypes", BrnPassbyStateManager.h:94)
//   return this
//
// mStaticPos is zeroed: the 3D-control form derives its position live from
// mp3dControl, so the cached static position is unused. The X360 wrote it with a
// single 16-byte stvx128 of a vspltisw-0 vector; reproduced here as value-init of
// the whole Vector3 (same observable all-zero bytes), reached BY NAME rather than
// via a raw 16-byte store. The trailing range check is the CGS_ASSERT-vacuous
// tripwire (leType < MaxPassbyTypes); it is non-gating.
//
// ABI NOTE (Hex-Rays register order vs declared source order): the X360 layout
// places leType in r6 and lbSuppressBoostBys in r7 (the two float args go to f1
// /f2 independently). The committed header declares the scalar parameters in
// source order (lp3dControl, lfRelativeVelocityMagnitude, leType,
// lbSuppressBoostBys, lfVolumeModifier). This body assigns each NAMED parameter
// to its named field, so the reconstruction is order-independent and faithful
// regardless of the host compiler's own register assignment.
// ---------------------------------------------------------------------------
PassbyStateManager::Passby::Passby(
        const CgsSound::Logic::Cgs3dEffectControl* lp3dControl,
        f32 lfRelativeVelocityMagnitude,
        EePassbyTypes leType,
        bool lbSuppressBoostBys,
        f32 lfVolumeModifier )
    : mStaticPos()                                                // 16B zero (stvx128 v0)
    , mp3dControl( lp3dControl )                                  // @0x10
    , mfRelativeVelocityMagnitude( lfRelativeVelocityMagnitude )  // @0x14
    , meType( leType )                                            // @0x18
    , mfVolumeModifier( lfVolumeModifier )                        // @0x1C
    , mbSuppressBoostBys( lbSuppressBoostBys )                    // @0x20
{
    CGS_ASSERT( leType < AttribSys::Enums::ePassbyTypes::MaxPassbyTypes,
                "leType < AttribSys::Enums::ePassbyTypes::MaxPassbyTypes" );
}

// =============================================================================
// State-manager surface grown in this slice (the RTTI/factory + boot virtuals so
// PassbyStateManager is a CONCRETE, registrable leaf the StateManager factory
// CreateStateMan @ 0x826A5B60 can construct). Sources:
//   PassbyStateManager::CreateObject        @ 0x82702030  (real)
//   PassbyStateManager::Prepare             @ 0x826F9748  (stub -- domain cascade)
//   PassbyStateManager::GetTypeName         @ 0x82688FF8  (real)
//   PassbyStateManager::ResourcesAreReady   @ 0x826D4BA8  (minimal -- domain cascade)
//   PassbyStateManager::Release             @ 0x826D4CC8  (stub -- Content cascade)
//   ctor                                    @ 0x826FFED8  (JSON ABSENT -- minimal)
// GetTypeInfo / GetStaticTypeInfo / GetResourceRegistrar were NOT individually
// exported; reconstructed from the established in-tree RTTI pattern
// (CgsStateManager.cpp GetStaticTypeInfo, CgsEffectBase.cpp) and the sibling
// BrnEffectObject::GetResourceRegistrar @ 0x82696850.
// =============================================================================

// ---------------------------------------------------------------------------
// PassbyStateManager::PassbyStateManager()  ctor @ 0x826FFED8  (JSON ABSENT)
//
// FLAG (ctor JSON absent -- minimal reconstruction): the individual ctor export
// for 0x826FFED8 is not present in the IDA dump (only its address is confirmed,
// as the xref target of CreateObject @ 0x82702030, which placement-news a
// 944-byte / 0x3B0 PassbyStateManager then `bl`s this ctor). The real ctor
// forwards to the BrnStateManager base ctor (-> CgsSound::Logic::StateManager
// ctor @ 0x826FAA18, the foundation), installs this class's vtables, then
// default-/seed-initialises the Passby members (maPostedPassbys ring,
// muPostedPassbyCount, mSplicerBank, mDynamicPropCache). Reproduced here as the
// implicit base construction + by-name zero-init of the count; the array, splicer
// placeholder and cache are default-constructed by their own members. The
// DynamicPropByCache / its Item have ctors; the Passby ring default-constructs.
// The X360 vtable stores are produced implicitly by deriving a polymorphic class.
// ---------------------------------------------------------------------------
PassbyStateManager::PassbyStateManager()
    : muPostedPassbyCount( 0 )
{
}

// ---------------------------------------------------------------------------
// PassbyStateManager::~PassbyStateManager()  (the X360 `vector deleting destructor`)
//
// FLAG (minimal): the matching dtor was not individually exported. The base
// BrnStateManager / CgsSound::Logic::StateManager virtual destructor tears down
// the base content pool + re-installs the MemBase vtable + routes storage back to
// the sound allocator (off_82FFB954) -- all re-synthesised by the host toolchain
// from this virtual destructor. The held splicer-bank Content is released in
// Release() (0x826D4CC8). No member teardown beyond the implicit base/members.
// ---------------------------------------------------------------------------
PassbyStateManager::~PassbyStateManager()
{
}

// ---------------------------------------------------------------------------
// PassbyStateManager::CreateObject(u32)  @ 0x82702030   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(944, "PassbyStateManager") ) return new'd ctor; }
//   else      { if ( MemBase::operator new(944, "PassbyStateManager") ) return new'd ctor; }
//   return 0;
//
// The X360 allocates a 944-byte (0x3B0) block through CgsSound::MemBase::operator
// new(size, tag) tagged "PassbyStateManager" (off_82F2F970) and placement-
// constructs a PassbyStateManager into it. Both arms call the SAME size+ctor; the
// `a1` argument only selects the operator-new overload flavour (the third arg the
// sibling managers pass, 0/1). The factory CreateStateMan @ 0x826A5B60 calls this
// as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model
// operator new(size, tag[, flags]) -- the sound allocator (off_82FFB954) is not
// homed in this group. A faithful placement-new through that allocator is
// therefore not yet expressible here; this reconstruction uses the host `new`
// (which routes through the global operator new, NOT the sound allocator). The
// observable result -- a constructed PassbyStateManager* (or null) handed to the
// factory -- matches. Replace `new` with the sound-allocator placement-new once
// MemBase::operator new is homed. The 944-byte size is the X360 0x3B0; on the
// 64-bit host the real object is larger (8-byte pointers/vptrs), so the literal
// size is documentation only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* PassbyStateManager::CreateObject( u32 /*luType*/ )
{
    return new PassbyStateManager();
}

// ---------------------------------------------------------------------------
// PassbyStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230,
// CgsEffectBase.cpp:130) -- a function-local static ClassTypeInfo<StateManager>
// seeded with this leaf's (ObjectID, typeName, baseTypeInfo, createObject) so the
// factory CreateStateMan can match descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID
// was NOT exported in the IDA dump (CreateObject @ 0x82702030 has no xrefs_to),
// and no map-state enum names the id in-tree. Following the established in-tree
// placeholder convention (every committed GetStaticTypeInfo uses 0; see
// BrnPassbyState.cpp), the ObjectID is seeded 0 here and MUST be replaced with the
// real id at integration. The factory matches descriptor->ObjectID == loop-index
// (CreateStateManagers @ 0x826AFEF8 iterates 0..8), so the real id is this
// manager's slot in that sequence -- to be pinned by the conductor.
//
// FLAG (registry hookup deferred): the minimal CgsSound::Logic::StateManager view
// pulled via BrnStateManager.h (this TU's base) does NOT declare
// AddToClassTypeInfoArray (that lives in the full CgsStateManager.h view, which is
// ODR-incompatible with BrnStateManager.h and cannot be co-included here). So this
// descriptor is PRODUCED here but its insertion into the static registry
// (dword_82FFBC58) must be performed by a registration site that uses the full
// StateManager view (the conductor-owned CreateStateMan TU). &CreateObject is an
// ABI-compatible StateManager*(*)(u32) across both views (a pointer is a pointer).
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PassbyStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        4,                       // ObjectID (PS3 DecFIGS static-init 0x85FA1C: PassbyStateManager=4)
        "PassbyStateManager",    // typeName
        0,                       // baseTypeInfo     -- StateManager base descriptor (deferred)
        &PassbyStateManager::CreateObject // createObject
    );
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// File-scope registration (Part D): land this leaf's descriptor in the shared
// StateManager RTTI registry (CgsStateManager.cpp gapClassTypeInfoArray, X360
// dword_82FFBC58) at load time, so StateManager::CreateStateMan (0x826A5B60) can
// find it by ObjectID. AddToClassTypeInfoArray is the canonical StateManager
// registration entry (@ 0x8268DFE8), reached through the BrnStateManager base.
//
// FLAG (ObjectID arbitrary-unique): the exact X360 ObjectID was not exported
// (CreateObject @ 0x82702030 has no xrefs_to). Assigned 1 here as a unique-among-the-9
// placeholder; the real id is this manager's slot in the CreateStateManagers 0..8 loop
// (@ 0x826AFEF8) -- pin at integration. This TU is OUT of the build, so dormant until
// the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpPassbyStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            PassbyStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// PassbyStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor (same value GetStaticTypeInfo yields).
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* PassbyStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();   // static -- the per-class descriptor
}

// ---------------------------------------------------------------------------
// PassbyStateManager::GetTypeName() const  @ 0x82688FF8
//   X360: `lwz r3, off_82F2F970` -> returns the literal "PassbyStateManager".
// ---------------------------------------------------------------------------
const char* PassbyStateManager::GetTypeName() const
{
    return "PassbyStateManager";
}

// ---------------------------------------------------------------------------
// PassbyStateManager::Prepare()  @ 0x826F9748   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 1: LoadAsset(this+0x90, "sound\\splicer\\PassbyAsset.bundle", 0, 0)
//   state 2: if (!Content::IsLoaded(this+0x224)) return 0;
//            mCpuMonitor = PerfMonCpu::AddMonitor("Passbys", 14, 0, 1.0, ...)
//   state 3: if (!StateManager::PrepareStates(this, 1, 8, 0)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- domain cascade): the real body cascades into
//   * BrnSound::Logic::IResourceRequester::LoadAsset (the streaming-resource broker),
//   * CgsSound::Logic::Content::IsLoaded (the splicer-bank content object),
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None of these are reconstructed in this slice. PrepareStateManagersOnBoot
// (0x826837F8) only needs Prepare() to return true to advance boot, so this stub
// returns true (boot-ready) WITHOUT running the asset load / state bring-up. NOT an
// X360-faithful body -- the prepare state machine is deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool PassbyStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// PassbyStateManager::Release()  @ 0x826D4CC8
//
//   if ( *(this+0x228) ) Content::Destruct(this+0x224);   // drop the splicer bank
//   return 1;
//
// FLAG (stub -- Content cascade): the real body conditionally destructs the held
// splicer-bank CgsSound::Logic::Content (the +0x224 sub-object, guarded by the
// +0x228 "constructed" flag). CgsSound::Logic::Content is modelled here only as the
// opaque ContentPlaceholder (mSplicerBank) -- the committed CgsContent.h does not
// compile under this gate (see the header's GetSplicerBank FLAG) -- so the
// Content::Destruct call cannot be reproduced faithfully. Returns true (the X360
// return) without the teardown; the held content is never constructed by this
// slice's Prepare stub, so there is nothing to destruct. Deferred with Content.
// ---------------------------------------------------------------------------
bool PassbyStateManager::Release()
{
    return true;
}

// ---------------------------------------------------------------------------
// PassbyStateManager::UpdateParams(f32)  (vtable per-frame param update)
//
// FLAG (stub -- domain cascade): the per-frame parameter update collects posted
// passbys + drives the 3D passby voices (the deep Passby voice/effect graph). Not
// in this slice's reconstructed surface and not needed for boot. No-op stub.
// (The committed sibling AIVehicleStateManager::UpdateParams @ 0x826CA578 shows the
// scale of this domain.)
// ---------------------------------------------------------------------------
void PassbyStateManager::UpdateParams( f32 /*lfTimeStep*/ )
{
}

// ---------------------------------------------------------------------------
// PassbyStateManager::ResourcesAreReady()  @ 0x826D4BA8
//
// X360 body: once the PassbyAsset bundle resolves, it MakeHash("PassbyAsset"),
// Content::Construct(this+0x194, ...) the splicer bank, then iterates the
// generated passbybin attribute table (Attrib::Gen::passbybin / Attrib::Instance::
// ChangeWithDefault) seeding 18 passby-type records, asserting the boost/passby
// index ordering.
//
// FLAG (minimal -- deep domain cascade): the real body pulls
//   * CgsSound::Playback::Name::MakeHash, CgsSound::Logic::Content::Construct,
//   * Attrib::Gen::passbybin + Attrib::Instance::ChangeWithDefault + Attrib::Private
//     (the AttribSys-generated passby tuning table), none reconstructed here, and it
//   * touches mSplicerBank, which is the opaque ContentPlaceholder under this gate.
// This is the IResourceRequester completion callback; it is invoked by the resource
// broker only AFTER LoadAsset resolves, which this slice's Prepare stub never issues.
// Bodied as a no-op so the leaf is concrete; NOT X360-faithful. Deferred with the
// AttribSys passby table + Content. X360 addr above.
// ---------------------------------------------------------------------------
void PassbyStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// PassbyStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850, which loads this->mpLogicModule (+0x2C) then tail-calls the
// IResourceRequester slot-1 of the module's embedded ResourceRegistrar
// (SoundLogicModule::mResourceRegistrar @ module+0x4C90). The state-manager leaves
// share the same +0x2C module back-pointer (stamped by CreateStateMan), so the
// route is identical.
//
// FLAG (module opaque): mpLogicModule is the base StateManager's +0x2C back-pointer
// (modelled void* in the full view; here reached via the BrnStateManager base). The
// SoundLogicModule home is not reconstructed in this slice and the minimal
// StateManager view in this TU does not expose mpLogicModule, so the +0x2C member
// cannot be read here without the full view. This override therefore cannot be
// bodied faithfully in this TU; it is provided as a non-cascading stub that
// abort-asserts if ever reached on boot (PrepareStateManagersOnBoot does NOT call
// it -- it is only used on the per-frame attach/detach path, which this slice does
// not exercise). FLAG: returns a reference to a TU-local empty registrar purely to
// satisfy the non-void signature; NOT a faithful body. Body via the module once the
// full StateManager view (mpLogicModule) + SoundLogicModule are available.
// ---------------------------------------------------------------------------
ResourceRegistrar& PassbyStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "PassbyStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
