#ifndef BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H
#define BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameSource/AttribSys/Enums/eAction.h"
#include "GameSource/AttribSys/Enums/eImpactTime.h"
#include "GameSource/AttribSys/Enums/eOrientation.h"
#include "GameSource/Sound/Collision/BrnCollisionFrameInformation.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

// =============================================================================
// BrnSound::Logic::Collision::ScrapeInfo
//   GameSource/Sound/Collision/BrnCollisionDataStructures.h (assert-cited home) +
//   GameSource/Sound/Collision/BrnCollisionDataStructures.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// ScrapeInfo is the per-scrape descriptor the collision sound logic keeps in a
// short history ring: it identifies a scrape by the pair of collision-object
// identifiers it runs between (maObjectId[0..1]) and carries the running scrape
// audio parameters (mfParam0x18, mfParam0x24). The owning state machine compares
// two ScrapeInfos for "same scrape" (operator==, order-insensitive on the object
// pair) and rolls the audio parameters forward (UpdateHistory).
//
// This TU bodies exactly TWO ledger functions (both assert-cited to
// BrnCollisionDataStructures.h):
//   operator==     @ 0x826821F0   (BrnCollisionDataStructures.h:111/112 assert sites)
//   UpdateHistory  @ 0x826822C8   (BrnCollisionDataStructures.h:146 assert site)
//
// LAYOUT (recovered store-for-store from the asm; offsets are this struct's field
// offsets, all reached BY NAME in the .cpp):
//   lwz/cmplw 0x10, 0x14   -> two u32 object identifiers (compared as the scrape
//                             "key"; order-insensitive: {a,b} == {b,a})
//   lfs/stfs  0x18         -> f32 scrape audio parameter (rolled by UpdateHistory)
//   lwz/cmpw  0x20         -> u32 scrape "kind"/material key (must match exactly)
//   lfs/stfs  0x24         -> f32 scrape audio parameter (rolled by UpdateHistory)
//   lbz       0x29         -> u8 mbValid (asserted set in operator==)
//
// FLAG (un-DWARF'd member names/types): no DecFIGS DWARF hint exists for
// BrnCollisionDataStructures.h, so the members are modelled as HONEST named fields
// at the asm-observed offsets/widths. Only the offsets, the store widths
// (lwz=u32, lfs=f32, lbz=u8) and the comparison/copy semantics are X360 facts; the
// field NAMES are descriptive. The gaps (0x00..0x0F, 0x1C, 0x28, 0x2A..) are other
// ScrapeInfo state NOT touched by these two functions and are modelled as honest
// reserved padding so the offsets land exactly.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

enum ESize
{
    E_SIZE_LARGE  = 0,
    E_SIZE_MEDIUM = 1,
    E_SIZE_SMALL  = 2,
};

// BrnCollisionStateManager.h:280. Per-scrape descriptor.
struct ScrapeInfo
{
    ScrapeInfo()
        : mEntityIdA{0}
        , mEntityIdB{0}
        , mfTimeStamp(0.0f)
        , mCollisionTagB{0}
        , meOrientation(AttribSys::Enums::eOrientation::Front)
        , mfIntensity(0.0f)
        , mbCrashing(false)
        , mbValid(false)
    {
        mRelativeVelocity.SetZero();
    }

    // BrnCollisionDataStructures.h:108 (assert-cited region). True iff the two
    // descriptors identify the same scrape: same kind (mu32Kind) and the same
    // unordered pair of object identifiers ({A,B} matches {A,B} or {B,A}). Asserts
    // both operands are valid (h:111/112). @ 0x826821F0.
    bool operator==( const ScrapeInfo& lInfo ) const;

    // BrnCollisionDataStructures.h:146 (assert site). Roll this descriptor's audio
    // parameters forward from a matching descriptor; asserts *this == lInfo first.
    // @ 0x826822C8.
    void UpdateHistory( const ScrapeInfo& lInfo );

    Vector3 mRelativeVelocity;                                      // +0x00
    EntityId mEntityIdA;                                           // +0x10
    EntityId mEntityIdB;                                           // +0x14
    f32 mfTimeStamp;                                                // +0x18
    CollisionTag mCollisionTagB;                                   // +0x1C
    AttribSys::Enums::eOrientation::eOrientation meOrientation;    // +0x20
    f32 mfIntensity;                                                // +0x24
    bool mbCrashing;                                                // +0x28
    bool mbValid;                                                   // +0x29
};

// BrnCollisionStateManager.h:376. Normalized collision presented to the resolver.
struct InputCollision
{
    enum EPipeline
    {
        E_REGULAR = 0,
        E_PROP = 1,
        E_MAX_PIPELINES = 2,
    };

    InputCollision()
        : maMaterial{0, 0}
        , maEntityID{{0}, {0}}
        , mfPriorityAddition(0.0f)
        , meAction(AttribSys::Enums::eAction::Collision)
        , meOrientation(AttribSys::Enums::eOrientation::Front)
        , mePipeline(E_REGULAR)
        , mbCull(false)
    {
        for (u32 luIndex = 0; luIndex < 3; ++luIndex)
            maParameter[luIndex] = VecFloat();
        mPosition.SetZero();
    }

    ScrapeInfo mScrapeInfo;
    VecFloat maParameter[3];
    Vector3 mPosition;
    u64 maMaterial[2];
    EntityId maEntityID[2];
    f32 mfPriorityAddition;
    AttribSys::Enums::eAction::eAction meAction;
    AttribSys::Enums::eOrientation::eOrientation meOrientation;
    EPipeline mePipeline;
    bool mbCull;
};

// BrnCollisionStateManager.h:486. Fully-resolved collision copied into a state.
struct OutputCollision
{
    OutputCollision()
        : meBankType(0)
        , mePipeline(InputCollision::E_REGULAR)
        , maMaterial{0, 0}
        , maEntityID{{0}, {0}}
        , meAction(AttribSys::Enums::eAction::Collision)
        , meOrientation(AttribSys::Enums::eOrientation::Front)
        , meSize(E_SIZE_LARGE)
        , meFatality(E_FATAL_OFF)
        , meImpactTime(AttribSys::Enums::eImpactTime::False)
        , mBinKey(0)
        , mfPriority(0.0f)
        , miBinIndex(-1)
        , miSampleID(-1)
    {
        mPosition.SetZero();
        for (u32 luIndex = 0; luIndex < 3; ++luIndex)
            maParameter[luIndex] = VecFloat();
        mNormalizedImpulse = VecFloat();
    }

    s32 meBankType;
    InputCollision::EPipeline mePipeline;
    u64 maMaterial[2];
    EntityId maEntityID[2];
    Vector3 mPosition;
    AttribSys::Enums::eAction::eAction meAction;
    AttribSys::Enums::eOrientation::eOrientation meOrientation;
    ESize meSize;
    BrnSound::Logic::EFatalityFlag meFatality;
    AttribSys::Enums::eImpactTime::eImpactTime meImpactTime;
    VecFloat maParameter[3];
    VecFloat mNormalizedImpulse;
    u64 mBinKey;
    ScrapeInfo mScrapeInfo;
    f32 mfPriority;
    s8 miBinIndex;
    s32 miSampleID;
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_DATA_STRUCTURES_H
