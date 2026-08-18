#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h" // BrnSound::Logic::Collision::ScrapeInfo (committed; maScrapeHistory element)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"       // CgsSound::Playback::Name::MakeHash (SelectBin helper)

// =============================================================================
// BrnSound::Logic::Collision::CollisionStateManager
//   GameSource/Sound/Collision/BrnCollisionStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Logic::Collision::CollisionStateManager; the Sound/Collision/ dir
//    already exists in-tree and hosts the sibling collision-audio classes --
//    BrnCollisionDataStructures, BrnCollisionFrameInformation, BrnHingeStateCache.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionStateManager is the sound-logic state manager that owns the collision /
// crash audio -- BY FAR the largest of the 9 managers (33408 bytes / 0x8280). It
// builds the per-material collision-generator lists, the crash-bin attribute tables,
// and the collision-event splicer banks, then drives the crash voices each frame. It
// is one of the 9 managers the SoundLogicModule factory CreateStateManagers
// (0x826AFEF8) creates via CreateStateMan.
//
// BASE CHAIN: CollisionStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FFAC0 installs a
//   primary vtable @ +0 (off_820B844C) AND a secondary sub-object vtable @ +0x90
//   (*(a1+144) = off_820B8444, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and the dtor @ 0x826FFD48 tears down the
//   base CgsSound::Logic::StateManager::RegisteredContent ObjectPool at +0xC. Same
//   shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 33408 bytes (0x8280)
// behind 4-byte pointers/vptrs (CreateObject @ 0x82701FA8 allocates 33408); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0x8280
// size / absolute offsets are NOT static_asserted. No recovered body in this slice
// names an individual data member by a recovered field name; the ~33KB of collision-
// audio state is deferred (see FLAG) and a single opaque pad honestly names it.
// =============================================================================

// AttribSys-generated crash-bin containers (forward declarations only). CrashBinUtils
// takes them as pointer parameters; the accessor member-pointers are invoked through
// them only inside the template body in the .cpp, so a forward decl suffices here.
// Both crashbin.h and propscrashbin.h are committed under AttribSys/Generated/classes/.
namespace Attrib { namespace Gen { class crashbin; class propscrashbin; } }

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// BrnSound::Logic::Collision::CrashBinUtils<CrashBin> -- a STATELESS utility struct
// (DWARF BrnCollisionStateManager.h:528/538) that copies the collision-sample-id
// array out of an AttribSys crash-bin container into a caller u16 buffer.
// GetSampleIds takes the container as an explicit first parameter (the struct holds
// no data, so the method never touches its own `this`). The two accessors are the
// bin's generated array-size / array-item getters and they are POINTERS TO MEMBER
// FUNCTIONS of the bin, invoked THROUGH lpCrashBin: the X360 leaf @0x8268FF24 does
// `mr r3,r28(lpCrashBin) ; mtctr r29 ; bctrl` and @0x8268FF68 the same for the item
// getter, i.e. the bin is `this` for both calls. DWARF :529/:530 renders the two
// parameters as `struct { const Int32& (*)() __pfn; int __delta; }` -- MSVC's
// pointer-to-member representation, not a plain function pointer (a 2026-08-18
// verify caught the earlier `const int& (*)()` spelling: it does not accept
// `&propscrashbin::mNumCollisionsSmall`, C2664). The _LayoutStruct::Int32 field is a
// plain 32-bit int in the attribute data area, modelled as `const int&`.
//
// Explicit instantiations (defined in BrnCollisionStateManager.cpp):
//   CrashBinUtils<Attrib::Gen::crashbin>::GetSampleIds      @ 0x8268DC18
//   CrashBinUtils<Attrib::Gen::propscrashbin>::GetSampleIds @ 0x8268FE90
// ---------------------------------------------------------------------------
template< typename CrashBin >
struct CrashBinUtils
{
    // Copy every collision index the crash-bin container holds
    // (count = *lpfnGetArraySize()) into lpauArray (each item truncated to u16),
    // bounded by luMaxSize; return the count.
    unsigned int GetSampleIds(
        const CrashBin*             lpCrashBin,
        const int&    (CrashBin::*lpfnGetArraySize)() const,
        const int&    (CrashBin::*lpfnGetArrayItem)( unsigned int ) const,
        u16*                        lpauArray,
        u16                         luMaxSize );
};

// Deferred collision-event descriptor (homed elsewhere; PlayCollision takes a pointer
// only). DWARF (BrnCollisionStateManager.h:143/:511) declares it `struct OutputCollision`.
struct OutputCollision;

// SelectBin name->bin-index helper  @ 0x826A0598. Free function (the asm never uses
// its r3 as `this`) -- the shared body both CollisionStateManager::SelectBin<>
// template instantiations tail-call. Hashes the requested crash-bin content name and
// looks it up in a small interned name-hash table; returns bin index 0 (default) on a
// hit at entry 0, else 1 (fallback). See BrnCollisionStateManager.cpp.
int SelectBin( int a1, const char* lkpacName, int a3, int a4, int a5 );

class CollisionStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // CollisionStateManager @ 0x826FFAC0 (HEAVY -- builds collision generator lists +
    // crash-bin attribute tables; this shell does a MINIMAL ctor, see .cpp FLAG).
    CollisionStateManager();

    // ~CollisionStateManager @ 0x826FFD48 (the X360 `vector deleting destructor`).
    virtual ~CollisionStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82701FA8 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82701FA8

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826F8B78  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();    // (stub -- domain cascade; see .cpp FLAG)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

    // FindInScrapeHistory @ 0x826889E0 (DWARF h:880 -- the NON-const overload). Linear
    // scan of the 16-slot scrape history; returns the first VALID slot that compares
    // equal to rScrapeInfo, else nullptr.
    BrnSound::Logic::Collision::ScrapeInfo* FindInScrapeHistory( const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo );

    // PlayCollision @ 0x82704028 (STUB -- collision-audio domain not homed; see .cpp
    // FLAG). Non-virtual; called only by UpdateParams. Hex-Rays signature is int; kept.
    int PlayCollision( OutputCollision* lpCollision );

private:
    // FLAG (deferred body -- ~33KB; the WHOLE collision domain): the X360 object is
    // 33408 bytes (0x8280). The collision-audio state -- a SelectionHistory<512>
    // (+0x8B0), a 32-entry table (+0x1300, stride 28), a 500-entry table (+0x1670), a
    // 16-entry table (+0x1E69, stride 48), TWO 64-entry arrays of vector-constructed
    // CgsSceneManager::CgsCollision::BaseCollisionGenerator (+0x2170/+0x4990, the
    // per-material collision generators), three Attrib::Gen tables
    // (crashbinlist / propscrashbinlist / proptomaterialmappings @ +0x8234..), and the
    // crash splicer-bank Content sub-objects (+0x8210..) -- is NOT modelled in this
    // minimal shell. Per the task constraint, this shell does NOT pull in the collision
    // domain (CgsSceneManager::CgsCollision, the Attrib::Gen crash tables); the heavy
    // construction is DEFERRED (see ctor FLAG). The shell exists only to be a CONCRETE,
    // registrable leaf whose Prepare() returns true for PrepareStateManagersOnBoot. A
    // single opaque pad keeps the deferred state honestly named without fabricating
    // field meanings. Size is UNVERIFIED on host (the X360 0x8280 is a 32-bit fact);
    // NOT static_asserted.
    u8 maDeferredCollisionState[1]; // placeholder for the un-reconstructed collision-audio members

    // DWARF (BrnCollisionStateManager.h:639). The 16-entry scrape history ring
    // FindInScrapeHistory scans (X360 offset +0x1E40, stride 48). Modelled with the
    // COMMITTED ScrapeInfo (BrnCollisionDataStructures.h) -- which carries the mbValid
    // flag + operator== FindInScrapeHistory needs -- rather than fabricating a second
    // same-FQN ScrapeInfo. FLAG: the committed ScrapeInfo is not the DWARF's full 48-byte
    // shape (it defers several fields), so the exact per-slot layout is a semantic-parity
    // approximation; members are pinned BY NAME, offsets NOT static_asserted on host.
    BrnSound::Logic::Collision::ScrapeInfo maScrapeHistory[16];
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
