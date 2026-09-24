#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Passby ctor range tripwire)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // CgsSound::Playback::Name::MakeHash
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] sink
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // the "Passbys" CPU monitor
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                    // EventQueue<PropUpdateNotification, 200>
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"               // the camera microphone (UpdateDynamicPropBys)
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"                    // State::Attach (UpdateParams' dispatch)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // BrnPhysics::Props::PropUpdateNotification
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
// SIGNATURE NOTE: the parameter is named lfCurrentTime (header + body agree). The
// X360 asm at 0x82683360 (and its sole call site inside UpdateDynamicPropBys
// @ 0x826A0E48..0x826A0E58, which loads `lfs f28, 4(rThis)` -- the manager's
// running game clock -- and passes it as the argument) shows the argument is the
// CURRENT game time, NOT a per-frame delta: each entry's expiry test is
// `(currentTime - entry.mfTimeStamp) >= 5.0`. (The PS3 DecFIGS DWARF records only
// the float32_t type for this inlined accessor, not a name; the name is confirmed
// by the X360 body/call-site role above.)
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

// =============================================================================
// [DIAG] NOT IN THE X360 BINARY
// Opt-in witnesses for the PASSBY state manager's prepare chain, enabled by
// BRN_PASSBY_SOUND_DIAG (any value but "0"). Named for exactly what they
// measure: which stage of PassbyStateManager::Prepare @0x826F9748 the manager
// is in, and whether the PassbyAsset splicer bank ever resolves. The "waiting"
// line is rate-limited to one per 600 calls (~10 s of prepare polling) so a bank
// that never loads reports a STALL instead of flooding the log -- that is the
// failure this witness exists to catch, because the console's Prepare blocks the
// boot stage machine until IsLoaded() goes true.
// =============================================================================
namespace
{
    bool PassbySoundDiagEnabled()
    {
        static int siEnabled = -1;
        if (siEnabled < 0)
        {
            const char* lpcEnv = std::getenv("BRN_PASSBY_SOUND_DIAG");
            siEnabled = (lpcEnv && lpcEnv[0] && lpcEnv[0] != '0') ? 1 : 0;
        }
        return siEnabled != 0;
    }

    void PassbySoundDiag(const char* lpcMessage)
    {
        if (PassbySoundDiagEnabled())
            CgsDev::Log::WriteToLog(lpcMessage);
    }

    void PassbySoundDiagWaiting(bool lbCreated)
    {
        static u32 suCalls = 0;
        if (!PassbySoundDiagEnabled())
            return;
        if ((suCalls++ % 600u) != 0)
            return;
        char lacMsg[160];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[passby-sound] Prepare: still WAITING on the splicer bank "
                      "(created=%d) after %u polls\n",
                      lbCreated ? 1 : 0, suCalls);
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

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
// State-manager surface (the RTTI/factory + the boot and per-frame virtuals).
// Sources:
//   PassbyStateManager::CreateObject        @ 0x82702030
//   PassbyStateManager::Prepare             @ 0x826F9748
//   PassbyStateManager::GetTypeName         @ 0x82688FF8
//   PassbyStateManager::ResourcesAreReady   @ 0x826D4BA8  (the bank; the assert-only
//                                                          passbybin walk is omitted)
//   PassbyStateManager::Release             @ 0x826D4CC8
//   PassbyStateManager::UpdateParams        @ 0x826D4D00
//   PassbyStateManager::UpdateDynamicPropBys @ 0x826A0DD0
//   DynamicPropByCache::Insert              @ 0x826975C8
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
        CgsSound::Logic::StateManager::GetStaticTypeInfo(), // baseTypeInfo (PS3 0x85FA1C: =StateManager::GetStaticTypeInfo())
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
// ObjectID RESOLVED (PS3 DecFIGS static-init 0x85FA1C): PassbyStateManager::sTypeInfo
// .ObjectID = 4. The descriptor comes from GetStaticTypeInfo() (seeded with that id and
// baseTypeInfo = StateManager::GetStaticTypeInfo()), so this registration lands the real
// id. NOTE (2026-08-25): this TU IS in the game build -- the registration runs at
// static-init and CreateStateManagers constructs this manager at boot.
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
//   switch (mePrepareState) {                               ; the 6-entry jump table
//     case 0: case 5: mePrepareState = 0;                   // fall through
//     case 1: mePrepareState = 1;
//             LoadAsset("sound\\splicer\\PassbyAsset.bundle", 0, E_DATA);   ; r5 = r6 = 0
//             // fall through
//     case 2: mePrepareState = 2;
//             if (!mSplicerBank.IsLoaded()) return false;   ; Content::IsLoaded(this+0x224)
//             miCpuMonitor = PerfMonCpu::AddMonitor("Passbys", 14, 0, 1.0f, 1);   ; stw 0x94
//             // fall through
//     case 3: mePrepareState = 3;
//             if (!PrepareStates(1, 8, 0)) return false;    ; KU_NUMBER_OF_PASSBY_STATES
//             // fall through
//     case 4: mePrepareState = 4; return true;
//     default: return false;
//   }
// ResourcesAreReady (below) constructs mSplicerBank once the bundle resolves, which is what
// state 2 waits on. The eight states are created by PrepareStates through PassbyState's
// registered descriptor, each with one PassbyEffect (ObjectID 0x40000) and its Passby3DControl
// (BrnPassbyState.cpp / BrnPassbyEffect.cpp).
// (Until FX-TAILS-B item 6 this returned true at once: with no states and no effect bodies the
// posted pass-bys were queued and dropped every frame -- nothing was ever voiced.)
// ---------------------------------------------------------------------------
bool PassbyStateManager::Prepare()
{
    switch( GetPrepareState() )
    {
    case E_PREPARE_NONE:
    case E_PREPARE_RELEASED:
        mePrepareState = E_PREPARE_NONE;
        // fall through
    case E_PREPARE_BEGIN:
        mePrepareState = E_PREPARE_BEGIN;
        LoadAsset( "sound\\splicer\\PassbyAsset.bundle", nullptr, ResourceRegistrar::E_DATA );
        PassbySoundDiag( "[passby-sound] Prepare: LoadAsset issued for "
                         "sound\\splicer\\PassbyAsset.bundle\n" );
        // fall through
    case E_PREPARE_UPDATING:
        mePrepareState = E_PREPARE_UPDATING;
        if( !mSplicerBank.IsLoaded() )
        {
            PassbySoundDiagWaiting( mSplicerBank.IsCreated() );
            return false;
        }
        miCpuMonitor = CgsDev::PerfMonCpu::AddMonitor(
            "Passbys", static_cast<CgsDev::PerfMonCpuPage>( 14 ), false, 1.0f, true );
        PassbySoundDiag( "[passby-sound] Prepare: splicer bank LOADED\n" );
        // fall through
    case E_PREPARE_STATES:
        mePrepareState = E_PREPARE_STATES;
        if( !PrepareStates( 1, KU_NUMBER_OF_PASSBY_STATES, 0 ) )
            return false;
        PassbySoundDiag( "[passby-sound] Prepare: FINISHED (8 states prepared)\n" );
        // fall through
    case E_PREPARE_FINISHED:
        mePrepareState = E_PREPARE_FINISHED;
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// PassbyStateManager::Release()  @ 0x826D4CC8
//
//   if ( *(this+0x228) ) Content::Destruct(this+0x224);   // the splicer bank, if constructed
//   return 1;
// (+0x228 is mSplicerBank's handle word -- Content::IsCreated().)
// ---------------------------------------------------------------------------
bool PassbyStateManager::Release()
{
    if( mSplicerBank.IsCreated() )
        mSplicerBank.Destruct();
    return true;
}

// ---------------------------------------------------------------------------
// PassbyStateManager::UpdateParams(f32)  @ 0x826D4D00  (vtable +0x18)
//
//   PerfMonCpu::StartMonitor(miCpuMonitor)                   ; +0x94
//   StateManager::UpdateParams(dt)                           ; 0x8268D800 -- the states
//   UpdateDynamicPropBys(dt)
//   lpInput = module->mpBrnLogicInputBuffer                  ; asserts "mpBrnLogicInputBuffer"
//                                                            ;   (h:432) and "lpInput" (cpp:201)
//   IsPlayerCarActive() on the vehicle interface (@0x82694D30, inlined: the index < 8 tripwire,
//   -1 -> false, else mbIsPlayerCarActive @+0x2860):
//       for each posted pass-by (+0xA0, stride 0x30, count +0x220):
//           lpState = GetFreeState(&posted[i])               ; vt +0x14
//           none -> ["[AWWWOOGA AWWWOOGA] No more free pass-by states" on the
//                    KI_SPEW_PASSBY_STATE_INFO dev switch] ; break
//           lpState->Attach(&posted[i])                      ; vt +0xC
//   muPostedPassbyCount = 0                                  ; every frame, dispatched or not
//   [the KI_SPEW_PASSBY_STATE_INFO state dump -- dev switch, not reproduced]
//   PerfMonCpu::StopMonitor(miCpuMonitor)
// ---------------------------------------------------------------------------
void PassbyStateManager::UpdateParams( f32 lfTimeStep )
{
    CgsDev::PerfMonCpu::StartMonitor( miCpuMonitor );
    CgsSound::Logic::StateManager::UpdateParams( lfTimeStep );
    UpdateDynamicPropBys( lfTimeStep );

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    const BrnSound::Module::Io::LogicInputBuffer* lpInput = lpModule->GetBrnInputStructure();
    CGS_ASSERT( lpInput != 0, "lpInput" );
    u32 luAttached = 0;
    if( lpInput->GetVehicleInterface()->IsPlayerCarActive() )
    {
        for( u32 luPassby = 0; luPassby < muPostedPassbyCount; ++luPassby )
        {
            CgsSound::Logic::State* lpState = GetFreeState( &maPostedPassbys[luPassby] );
            if( !lpState )
                break;
            lpState->Attach( &maPostedPassbys[luPassby] );
            ++luAttached;
        }
    }

    // [DIAG] NOT IN THE X360 BINARY (BRN_PASSBY_SOUND_DIAG): each frame that had posts --
    // how many were handed to a state. Capped at 64 lines.
    if( muPostedPassbyCount != 0 && PassbySoundDiagEnabled() )
    {
        static u32 suPrintCount = 0;
        if( suPrintCount++ < 64u )
        {
            char lacMsg[160];
            std::snprintf( lacMsg, sizeof( lacMsg ),
                           "[passby-sound] dispatch posted=%u attached=%u type0=%d relVel0=%g control0=%d\n",
                           muPostedPassbyCount, luAttached, static_cast<s32>( maPostedPassbys[0].meType ),
                           static_cast<double>( maPostedPassbys[0].mfRelativeVelocityMagnitude ),
                           maPostedPassbys[0].mp3dControl != 0 ? 1 : 0 );
            CgsDev::Log::WriteToLog( lacMsg );
        }
    }

    muPostedPassbyCount = 0;
    CgsDev::PerfMonCpu::StopMonitor( miCpuMonitor );
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
//   0x826D4BB4..0x826D4BD8  Content::Construct(&mSplicerBank (this+0x194 on the requester
//                           sub-object == +0x224), mpLogicModule, dword_83008404
//                           (MakeHash("~SplicerFactory::SK_NAME~")), MakeHash("PassbyAsset"))
// This is the IResourceRequester completion callback the registrar runs once Prepare's
// LoadAsset resolves; Prepare's state 2 waits on the bank it constructs.
// ---------------------------------------------------------------------------
void PassbyStateManager::ResourcesAreReady()
{
    PassbySoundDiag("[passby-sound] ResourcesAreReady: constructing the "
                    "PassbyAsset splicer bank\n");
    mSplicerBank.Construct(
        GetLogicModule(),
        static_cast<u32>(CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~")),
        static_cast<u32>(CgsSound::Playback::Name::MakeHash("PassbyAsset")));

    // FLAG (omitted, assert-only): the X360 tail walks the 18 burnoutglobaldata
    // mPassbyBins RefSpecs (layout +0x288 header / +0x290 elements, 0x18 stride)
    // through an Attrib::Gen::passbybin instance and fires two range asserts per row
    // (BrnPassbyStateManager.cpp:147/148, mFirstBoostPassBy <= mLastBoostPassBy and
    // mFirstPassBy <= mLastPassBy). It has NO side effect -- it validates data and
    // returns -- so it is not reproduced here.
}

// (GetResourceRegistrar: the console has no PassbyStateManager override -- the inherited
// BrnStateManager::GetResourceRegistrar @0x82696510 serves LoadAsset; see the header.)


// ---------------------------------------------------------------------------
// DynamicPropByCache::Find  @ 0x82683438
//   Linear scan for the first ACTIVE cache slot whose stored EntityId matches lId;
//   returns a pointer to it, or 0 when no active slot matches within the 32-entry
//   cache. Faithful to the X360 loop (BrnPassbyStateManager.h:180 declares
//   `Item* Find( const EntityId& )`):
//     lbz mbActive@+0 ; if !active -> advance ; lwz mId.muValue@+8 vs lId.muValue@+0
//     -> hit returns &maItems[count] ; advance ++count, stride 0xC, cap 0x20.
//   Cap 0x20 == KU_DYNAMIC_PROP_CACHE_SIZE (32). Reached BY NAME (indexed maItems
//   walk) -- no absolute offsets asserted.
// ---------------------------------------------------------------------------
PassbyStateManager::DynamicPropByCache::Item*
PassbyStateManager::DynamicPropByCache::Find( const EntityId& lId )
{
    for( u32 luIndex = 0; luIndex < PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE; ++luIndex )
    {
        Item& lrItem = maItems[ luIndex ];
        if( lrItem.mbActive && lrItem.mId.muValue == lId.muValue )
        {
            return &lrItem;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// DynamicPropByCache::Insert  @ 0x826975C8  (DWARF BrnPassbyStateManager.h:160)
//   assert Find(lId) == 0                              ; "Find( lEntity ) == 0" (h:163, li r5, 0xA3)
//   the first INACTIVE slot (stride 0xC, cap 0x20):
//       mfTimeStamp = lfTimeStamp ; mbActive = 1 ; mId = lId ; return it
//   no free slot -> 0
// ---------------------------------------------------------------------------
PassbyStateManager::DynamicPropByCache::Item*
PassbyStateManager::DynamicPropByCache::Insert( f32 lfTimeStamp, const EntityId& lId )
{
    CGS_ASSERT( Find( lId ) == 0, "Find( lEntity ) == 0" );
    for( u32 luIndex = 0; luIndex < PassbyStateManager::KU_DYNAMIC_PROP_CACHE_SIZE; ++luIndex )
    {
        Item& lrItem = maItems[ luIndex ];
        if( !lrItem.mbActive )
        {
            lrItem.mfTimeStamp = lfTimeStamp;
            lrItem.mbActive = true;
            lrItem.mId = lId;
            return &lrItem;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Passby::Passby( Vector3, f32, EePassbyTypes, bool, f32 )  (DWARF BrnPassbyStateManager.h:102)
//
// Header inline on the console; its inlined copies (UpdateDynamicPropBys @0x826A1070..
// 0x826A1094, StaticPassbyControl::TriggerPassby @0x826B9BF0) store exactly
//   mStaticPos = lStaticPos (stvx128) ; mp3dControl = 0 ; mfRelativeVelocityMagnitude ;
//   meType ; mfVolumeModifier ; mbSuppressBoostBys
// and no range assert (TriggerPassby passes a run-time type and shows only its own
// "lePassbyType < ePassbyTypes::MaxPassbyTypes" check, BrnStaticPassbyControl.cpp:247).
// ---------------------------------------------------------------------------
PassbyStateManager::Passby::Passby(
        Vector3 lStaticPos,
        f32 lfRelativeVelocityMagnitude,
        EePassbyTypes leType,
        bool lbSuppressBoostBys,
        f32 lfVolumeModifier )
    : mStaticPos( lStaticPos )
    , mp3dControl( 0 )
    , mfRelativeVelocityMagnitude( lfRelativeVelocityMagnitude )
    , meType( leType )
    , mfVolumeModifier( lfVolumeModifier )
    , mbSuppressBoostBys( lbSuppressBoostBys )
{
}

namespace
{
// |a - b| over x / y / z: `vsubfp ; vmsum3fp`, vrsqrtefp with two Newton steps, and the
// `vcmpeqfp / vsel` guard that makes a zero vector 0 instead of 0 * infinity.
f32 DistanceBetween( const Vector3& lrA, const Vector3& lrB )
{
    const f32 lfX = lrA.x - lrB.x;
    const f32 lfY = lrA.y - lrB.y;
    const f32 lfZ = lrA.z - lrB.z;
    const f32 lfSquared = lfX * lfX + lfY * lfY + lfZ * lfZ;
    return lfSquared == 0.0f ? 0.0f : std::sqrt( lfSquared );
}
}

// ---------------------------------------------------------------------------
// PassbyStateManager::UpdateDynamicPropBys(f32)  @ 0x826A0DD0  (DWARF cpp:~230)
//
//   listener = the CAMERA microphone, maMicrophones[E_MIC_CAMERA][E_PLAYER_1]
//              (module+0x29B0: position = its current matrix row 3 @+0x30, velocity @+0x90)
//   queue    = the logic input buffer's prop-update notifications (assert
//              "mpBrnLogicInputBuffer", h:432 ; GetPropUpdateNotificationQueue @0x826949E8)
//   mDynamicPropCache.Update(mfCurrentTime)                 ; lfs f28, 4(this)
//   for each notification (BaseEventQueue::GetEvent @0x8268EA38, 64-byte stride):
//       id = its PropEntityID (the inlined owner tripwire, BrnPropEntityID.h:278)
//       cached -> item.mfTimeStamp = mfCurrentTime ; next
//       speed = |mLinearVelocity (+0x10) - listener velocity|
//       !(speed > 25 mph)                         -> next    ; splat(flt_82F31928 0.44704 *
//                                                              flt_820AA53C 25.0), static
//       !(speed * flt_82004E58 (0.15) > |mPosition - listener position|) -> next
//       PostPassby(Passby(mPosition, speed, Camera (10), false, 1.0f))   ; inlined, result unused
//       mDynamicPropCache.Insert(mfCurrentTime, GetEntityId() @0x826944E0)
// i.e. a prop flung past the camera that will reach it within 0.15 s is voiced once, and is
// not voiced again until it has been out of the notifications for KF_PROP_BY_CACHE_LIFETIME.
// ---------------------------------------------------------------------------
void PassbyStateManager::UpdateDynamicPropBys( f32 /*lfTimeStep*/ )
{
    static const f32 KF_MIN_PROP_SPEED = 0.44704f * 25.0f;     // flt_82F31928 * flt_820AA53C (0x826A0FD4)
    static const f32 KF_PROP_TIME_TO_LISTENER = 0.15f;         // flt_82004E58

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>( GetLogicModule() );
    const CgsSound::Logic::MicrophoneSystem::Microphone* lpListener =
        lpModule->GetEnvironment().GetMicrophoneSystem().GetMicrophone(
            CgsSound::Logic::MicrophoneSystem::E_MIC_CAMERA, CgsSound::Logic::MicrophoneSystem::E_PLAYER_1 );
    const Vector3 lListenerPosition = lpListener->GetMicrophoneMatrix().Pos();
    const Vector3 lListenerVelocity = lpListener->GetVelocity();

    // The input buffer's attested-width queue storage, viewed through the world's typed queue
    // -- the identity BrnGameModule::BridgeWorldToSound's Append already relies on.
    typedef CgsModule::EventQueue<BrnPhysics::Props::PropUpdateNotification, 200> NotificationQueue;
    const NotificationQueue& lrNotifications = *reinterpret_cast<const NotificationQueue*>(
        static_cast<const BrnSound::Module::Io::LogicInputBuffer*>( lpModule->GetBrnInputStructure() )
            ->GetPropUpdateNotificationQueue() );

    const f32 lfCurrentTime = mfCurrentTime;
    mDynamicPropCache.Update( lfCurrentTime );
    for( s32 liNotification = 0; liNotification < lrNotifications.GetLength(); ++liNotification )
    {
        const BrnPhysics::Props::PropUpdateNotification& lrNotification =
            lrNotifications.GetEvent( liNotification );
        DynamicPropByCache::Item* lpItem = mDynamicPropCache.Find( lrNotification.GetEntityId().mEntityId );
        if( lpItem )
        {
            lpItem->mfTimeStamp = lfCurrentTime;
            continue;
        }

        const f32 lfSpeed = DistanceBetween( lrNotification.mLinearVelocity, lListenerVelocity );
        if( !( lfSpeed > KF_MIN_PROP_SPEED ) )
            continue;
        if( !( lfSpeed * KF_PROP_TIME_TO_LISTENER > DistanceBetween( lrNotification.mPosition, lListenerPosition ) ) )
            continue;

        const Passby lPassby( lrNotification.mPosition, lfSpeed,
                              AttribSys::Enums::ePassbyTypes::Camera, false, 1.0f );
        PostPassby( lPassby );
        mDynamicPropCache.Insert( lfCurrentTime, lrNotification.GetEntityId().mEntityId );
    }
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
