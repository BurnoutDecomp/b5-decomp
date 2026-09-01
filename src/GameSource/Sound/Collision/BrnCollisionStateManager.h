#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h" // BrnSound::Logic::Collision::ScrapeInfo (committed; maScrapeHistory element)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"       // CgsSound::Playback::Name::MakeHash (SelectBin helper)
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameSource/AttribSys/Generated/classes/crashbin.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"
#include "GameSource/AttribSys/Generated/classes/crashbinlist.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h"
#include "GameSource/AttribSys/Generated/classes/proptomaterialmappings.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace BrnDirector { namespace Camera { struct Camera; } }
namespace BrnPhysics { namespace ContactSpy { struct BaseContact; struct PropContact; } }
namespace BrnSound { namespace Module { namespace Io { struct RootInputBuffer; } } }

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
// size / absolute offsets are NOT static_asserted. Runtime members are represented by
// their recovered names and roles rather than console padding or raw-offset access.
// =============================================================================

// The crash-bin classes must be complete before CrashBinUtils is declared. On MSVC x64
// a pointer-to-member of an incomplete class uses the general 24-byte representation,
// while these single-inheritance generated classes use the 8-byte representation. The
// ARTIST helper is called with pointers to their concrete accessors, so both the caller
// and the explicit-instantiation site must compile against the same complete types.

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

enum ECollisionSpliceTags : int
{
    E_COLLISION_SPLICE_SMALL_LANDINGS = 0,
    E_COLLISION_SPLICE_SUSPENSION = 1,
    E_COLLISION_SPLICE_HARD_LANDINGS = 2,
    E_COLLISION_SPLICE_CRASH_LANDINGS = 3,
    E_COLLISION_SPLICE_JUNKYARD_LANDING_SWEETNER = 4,
    E_COLLISION_SPLICE_CRASH_IN_WATER = 5,
};

enum ECollisionSpliceBankType
{
    E_COLLISION_SPLICE_BANK_COLLISION = 0,
    E_COLLISION_SPLICE_BANK_MAX = 1,
};

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

// Collision-event descriptor is defined in BrnCollisionDataStructures.h.
struct OutputCollision;

struct CameraInfo
{
    CameraInfo()
        : mfFieldOfView(0.0f)
        , mfCosineHalfFov(0.0f)
        , mfAspectRatio(0.0f)
        , mfZoom(0.0f)
    {
        mTransform.SetIdentity();
    }

    Matrix44Affine mTransform;
    f32 mfFieldOfView;
    f32 mfCosineHalfFov;
    f32 mfAspectRatio;
    f32 mfZoom;
};

struct PropToMaterialMapping
{
    PropToMaterialMapping() : muMaterialIndex(0), mbValid(false) {}
    u16 muMaterialIndex;
    bool mbValid;
};

// SelectBin name->bin-index helper  @ 0x826A0598. Free function (the asm never uses
// its r3 as `this`) -- the shared body both CollisionStateManager::SelectBin<>
// template instantiations tail-call. Hashes the requested crash-bin content name and
// looks it up in a small interned name-hash table; returns bin index 0 (default) on a
// hit at entry 0, else 1 (fallback). See BrnCollisionStateManager.cpp.
int SelectBin( int a1, const char* lkpacName, int a3, int a4, int a5 );

class CollisionStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // CollisionStateManager @ 0x826FFAC0.
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
    virtual bool Prepare();                       // @ 0x826F8B78  (vtable +0x0C)
    virtual CgsSound::Logic::State* GetFreeState(void* apvAttachment) override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void Notify(const CgsSound::Io::MessageHeader* apkMessage) override;

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar();

    // FindInScrapeHistory @ 0x826889E0 (DWARF h:880 -- the NON-const overload). Linear
    // scan of the 16-slot scrape history; returns the first VALID slot that compares
    // equal to rScrapeInfo, else nullptr.
    BrnSound::Logic::Collision::ScrapeInfo* FindInScrapeHistory( const BrnSound::Logic::Collision::ScrapeInfo& rScrapeInfo );

    // PlayCollision @ 0x82704028. Non-virtual; called by UpdateParams.
    int PlayCollision( OutputCollision* lpCollision );

    const CgsSound::Logic::Content& GetSplicerBank(
        ECollisionSpliceBankType aeBank) const
    {
        CGS_ASSERT(aeBank >= E_COLLISION_SPLICE_BANK_COLLISION &&
                   aeBank < E_COLLISION_SPLICE_BANK_MAX,
                   "leSpliceBankType < E_COLLISION_SPLICE_BANK_MAX");
        return mCollisionSplicerBank[aeBank];
    }

    const CgsSound::Logic::Content& GetScrapeAemsBank() const
    {
        return mScrapesAemsBank;
    }

private:
    void SetCollisionBinList(u64 luCollisionBinListKey,
                             u64 luPropsCollisionBinListKey,
                             u64 luPropsMappingKey);
    void BuildPropToMaterialTable();
    void UpdateResolver(const BrnSound::Module::Io::RootInputBuffer& lrInput,
                        const BrnSound::Logic::FrameInformation& lrFrame,
                        f32 afDeltaTime);
    void AddInputCollision(const InputCollision& lrCollision);
    bool ProcessCollision(OutputCollision& lrOutput, const InputCollision& lrInput);
    void ProcessCollisions();
    u64 MapEntityIdToMaterial(const EntityId& lrEntityId,
                              const BrnSound::Module::Io::RootInputBuffer& lrInput) const;
    bool MapPropTypeToMaterial(u16 luPropType, u64& lruMaterial) const;
    void MakeBaseInputCollision(InputCollision& lrOut,
                                const BrnPhysics::ContactSpy::BaseContact& lrContact,
                                const BrnSound::Module::Io::RootInputBuffer& lrInput,
                                f32 afDeltaTime) const;
    void MakePropInputCollision(InputCollision& lrOut,
                                const BrnPhysics::ContactSpy::PropContact& lrContact,
                                const BrnSound::Module::Io::RootInputBuffer& lrInput,
                                f32 afDeltaTime) const;
    void SetCameraInfo(const BrnDirector::Camera::Camera& lrCamera);
    u32 MapCameraStateToBinFlags(const BrnDirector::Camera::Camera& lrCamera) const;
    u32 MapGameModesToBinFlags(const void* lpGameMode) const;
    static bool LessThanPriority(const OutputCollision* lpLeft,
                                 const OutputCollision* lpRight);

    template <typename ListType, typename BinType>
    void SelectCollisionBin(OutputCollision& lrOutput, const ListType& lrList);

    template <typename BinType>
    void GetRandomSampleID(OutputCollision& lrOutput);

    CgsSound::Utils::SelectionHistory<512, u16, u16, 65536>
        maSelectionHistory[E_COLLISION_SPLICE_BANK_MAX];
    PropToMaterialMapping maPropToMaterialMappings[500];
    InputCollision maInputCollision[64];
    OutputCollision maOutputCollision[64];
    CameraInfo mCameraInfo;
    u32 mu32InputCollisionCount;
    u32 mu32OutputCollisionCount;
    BrnSound::Logic::FrameInformation mFrameInformation;

    // DWARF (BrnCollisionStateManager.h:639). The 16-entry scrape history ring
    // FindInScrapeHistory scans (X360 offset +0x1E40, stride 48). Modelled with the
    // COMMITTED ScrapeInfo (BrnCollisionDataStructures.h) -- which carries the mbValid
    // flag + operator== FindInScrapeHistory needs -- rather than fabricating a second
    // same-FQN ScrapeInfo. FLAG: the committed ScrapeInfo is not the DWARF's full 48-byte
    // shape (it defers several fields), so the exact per-slot layout is a semantic-parity
    // approximation; members are pinned BY NAME, offsets NOT static_asserted on host.
    BrnSound::Logic::Collision::ScrapeInfo maScrapeHistory[16];

    // The three ref-counted content handles built at the tail of the ARTIST
    // constructor.  They are the runtime-visible part of the otherwise deferred
    // collision-manager payload and are required by landing/scrape voices.
    CgsSound::Logic::Content mScrapesCsisInterface;
    CgsSound::Logic::Content mScrapesAemsBank;
    CgsSound::Logic::Content mCollisionSplicerBank[E_COLLISION_SPLICE_BANK_MAX];
    Attrib::Gen::crashbinlist mCrashBinList;
    Attrib::Gen::propscrashbinlist mPropsCrashBinList;
    Attrib::Gen::proptomaterialmappings mPropMaterialMappings;
    bool mbResourcesAreLoaded;
    bool mbBoundToProps;
    CgsResource::ResourceHandle mPropDataResourceHandle;
    u32 mx32CameraBinFlags;
    u32 mx32GameModeBinFlags;
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
