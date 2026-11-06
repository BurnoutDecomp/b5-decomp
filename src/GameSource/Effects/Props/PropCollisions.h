#ifndef GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H
#define GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (VFXCoronaTypeData::GetCoronaType)
#include "rw/math/vpu/types.h"                                          // rw::math::vpu::Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                   // CgsNumeric::Random (PropCollisions::mRandom, BY VALUE)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"      // CgsResource::ResourcePtr / ResourceHandle (BY VALUE)

#include <cstdint>   // uintptr_t (the collection's 32-bit address words)

// ============================================================================
// GameSource/Effects/Props/PropCollisions.h
//
// The prop-strike VFX: when a prop is hit or smashed near the camera, the LION effects baked on the prop's
// material locators fire. DWARF PropCollisions.h (BrnEffects::PropCollisions, PropToVFXMaterialMapping) and
// PropCollisions.cpp (VFXRuntimeMaterialLef), and VFXPropsResourceType.h for the collection the table maps
// into. Reconstructed from the ARTIST ledger:
//   PropCollisions::Initialise                @ 0x822937A8
//   PropCollisions::BuildPropToMaterialTable  @ 0x82288640
//   PropCollisions::UpdateLocatorVfx          @ 0x822993A0
//   VFXRuntimeMaterialLef::CreateEffect       @ 0x822936F0
//   VFXRuntimeMaterialLef::TriggerLocators    @ 0x82299168
// ParticleModule embeds one PropCollisions at +0x4270 (DWARF ParticleModule.h:28); LoadFXBundle binds its
// collection and its prop physics data and runs Initialise, and EffectsModule::Update runs UpdateLocatorVfx.
// ============================================================================

namespace BrnParticle
{
    class ParticleModule;
    struct LionEffect;

    // DWARF VFXPropsResourceType.h:84 -- one baked VFX locator on a prop material:
    // a local-space anchor position, a precomputed effect-name hash, and the raw name
    // string. 0x50-byte stride (X360 TriggerLocators steps lpLocatorArray by 0x50).
    struct VFXLocator
    {
        rw::math::vpu::Vector3 mPosition;   // +0x00  local-space anchor        (:114)
        u32     mHashedName;    // +0x10  precomputed effect-name hash           (:115)
        char    macName[60];    // +0x14  macDebugLefName, the raw locator name  (:118)

        u32 GetHash() const { return mHashedName; }   // :102
    };

    // The VFXPropCollection resource as VFXPropCollectionResourceType::FixUp leaves it in memory
    // (SharedClasses/Graphics/VFXPropsResourceType.cpp): the console's serialised layout, each table index
    // rebased IN PLACE into a 32-BIT ADDRESS WORD. The accessors below widen those words; the member names
    // are the DWARF's.

    // DWARF VFXPropsResourceType.h:123 -- 12 bytes.
    struct VFXMaterial
    {
        u32 mType;          // +0x00 :152
        u32 mNumLocators;   // +0x04 :153
        u32 mpLocators;     // +0x08 :154  VFXLocator* -- a 32-bit address word

        const VFXLocator* GetLocators() const
        {
            return reinterpret_cast<const VFXLocator*>(static_cast<uintptr_t>(mpLocators));
        }
    };

    // DWARF VFXPropsResourceType.h:256 -- one corona's light behaviour, shared by every placement of it.
    // Console stride 0x20 (FixUp loop D `32 * index`, PreSceneUpdate `addi r28, r28, 0x20`).
    struct VFXCoronaTypeData
    {
        static const u32 KU_CORONA_TYPE_MAX = 17;   // VFXCoronaTypeMax: GetCoronaType's `cmplwi r11, 0x11`

        u32  mnID;             // +0x00 :256
        u32  mType;            // +0x04 :257  eVFXCoronaType, the corona texture
        f32  mrTimeOn;         // +0x08 :258
        f32  mrTimeOff;        // +0x0C :259
        f32  mrSizeMin;        // +0x10 :260
        f32  mrSizeMax;        // +0x14 :261
        f32  mrMasterTime;     // +0x18 :262  advanced by PropEntityModule::PreSceneUpdate
        bool mbSynchronised;   // +0x1C :263
        u8   maPad1D[3];

        // :166 / 0x822A4418 -- with its "mType<VFXCoronaTypeMax" assert (VFXPropsResourceType.h:168).
        s32 GetCoronaType() const
        {
            CGS_ASSERT(mType < KU_CORONA_TYPE_MAX, "mType<VFXCoronaTypeMax");
            return static_cast<s32>(mType);
        }
        f32 GetTotalTime() const { return mrTimeOn + mrTimeOff; }   // :202
    };
    static_assert(sizeof(VFXCoronaTypeData) == 0x20, "VFXCoronaTypeData is the console's 32-byte record");

    // DWARF VFXPropsResourceType.h:302 -- one corona placed on a prop, in the prop's model space.
    // Console stride 0x50 (FixUp loop D `80 * index`, RenderPropAndCoronas `addi r24, r24, 0x50`).
    struct VFXCoronaType
    {
        rw::math::vpu::Matrix44Affine mTransform;   // +0x00 :302  row zAxis (+0x20) = facing, wAxis (+0x30) = position
        u32 mpTypeData;                            // +0x40 :303  VFXCoronaTypeData* -- a 32-bit address word
        f32 mrTimeOffset;                          // +0x44 :306  phase offset into the shared blink cycle
        u8  maPad48[8];

        f32 GetTimeOffset() const { return mrTimeOffset; }   // :289

        const VFXCoronaTypeData* GetTypeData() const   // :274
        {
            return reinterpret_cast<const VFXCoronaTypeData*>(static_cast<uintptr_t>(mpTypeData));
        }
    };
    static_assert(sizeof(VFXCoronaType) == 0x50, "VFXCoronaType is the console's 80-byte record");

    // DWARF VFXPropsResourceType.h:311 -- 16 bytes.
    struct VFXPropState
    {
        u32 mpVFXMaterial;       // +0x00 :344  VFXMaterial* -- a 32-bit address word
        u32 muNumVFXMaterials;   // +0x04 :345
        u32 mpCoronaType;        // +0x08 :347  VFXCoronaType* -- a 32-bit address word
        u32 muNumCoronas;        // +0x0C :348

        const VFXMaterial* GetMaterial() const
        {
            return reinterpret_cast<const VFXMaterial*>(static_cast<uintptr_t>(mpVFXMaterial));
        }
        // RenderPropAndCoronas reads `*(state + 8)` + 0x50 * i and `*(state + 0xC)` for the count.
        const VFXCoronaType* GetCoronaTypes() const
        {
            return reinterpret_cast<const VFXCoronaType*>(static_cast<uintptr_t>(mpCoronaType));
        }
        u32 GetNumCoronas() const { return muNumCoronas; }
    };

    // DWARF VFXPropsResourceType.h:352 -- 16 bytes.
    struct VFXProp
    {
        u64 mPropID;            // +0x00 :436
        u32 mpPropStates;       // +0x08 :437  VFXPropState* -- a 32-bit address word
        u32 muNumPropStates;    // +0x0C :438

        u64 GetPropID() const    { return mPropID; }           // :386
        u32 GetNumStates() const { return muNumPropStates; }   // :392
        // :400 -- inlined by BuildPropToMaterialTable with its two asserts (VFXPropsResourceType.h:402 / :403,
        // `cmplwi ; bgt` and `cmplwi ; bne` at 0x822888BC..0x822888E8).
        const VFXPropState* GetState(u32 luState) const;
    };

    // DWARF VFXPropsResourceType.h:446 -- the thirteen header words FixUp rebases.
    class VFXPropCollection
    {
    public:
        const VFXProp* GetTable() const                                             // :460
        {
            return reinterpret_cast<const VFXProp*>(static_cast<uintptr_t>(mpPropTable));
        }
        u32 GetTableSize() const { return muPropTableSize; }                        // :466

        // :634, with its "luOffset < muCoronaDataTableSize" assert (:636) -- PreSceneUpdate's phase loop
        // (0x8230BD50 `lwz 0x2C`, 0x8230BD74 `lwz 0x28` + 0x20 * i).
        u32 GetNumCoronaTypeData() const { return muCoronaDataTableSize; }
        VFXCoronaTypeData* GetCoronaTypeDataByOffset(u32 luOffset) const
        {
            CGS_ASSERT(luOffset < muCoronaDataTableSize, "luOffset < muCoronaDataTableSize");
            return reinterpret_cast<VFXCoronaTypeData*>(static_cast<uintptr_t>(mpCoronaTypeDataTable)) + luOffset;
        }

        u32 mpPropTable;             // +0x00 :645
        u32 muPropTableSize;         // +0x04 :646
        u32 mpPropStateTable;        // +0x08 :648
        u32 muPropStateTableSize;    // +0x0C :649
        u32 mpMaterialTable;         // +0x10 :651
        u32 muMaterialTableSize;     // +0x14 :652
        u32 mpLocatorTable;          // +0x18 :654
        u32 muLocatorTableSize;      // +0x1C :655
        u32 mpCoronaTable;           // +0x20 :657
        u32 muCoronaTableSize;       // +0x24 :658
        u32 mpCoronaTypeDataTable;   // +0x28 :660
        u32 muCoronaDataTableSize;   // +0x2C :661
        u32 muVersion;               // +0x30 :672
    };
}

namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
namespace BrnPhysics { namespace Props { class PropPhysicsDataHeader; } }
namespace BrnDirector { namespace Camera { class Camera; } }
namespace BrnWorld { namespace PropEntityIO { struct PropVFXLocatorEvent; } }
namespace CgsModule { template <typename T> class BaseEventQueue; }

namespace BrnEffects
{
    // The runtime striking-VFX helper for prop material locators (Lion-effect flavour). DWARF PropCollisions.cpp:30.
    class VFXRuntimeMaterialLef
    {
    public:
        // The round-robin depth of the playing-effect handle ring (DWARF PropCollisions.cpp:26).
        static const u32 kuSizeOfEffectsArray = 5;

        // :33 -- inlined by PropCollisions::Initialise (0x822937BC..0x82293810): every handle invalid, the ring
        // back to its first slot.
        static void Initialise();

        // TriggerLocators @ 0x82299168 (DWARF :56) -- fire the LION effect on each locator of the struck prop:
        // a basis aligned to the car's velocity, the locator carried through the prop transform, the car's
        // velocity as the effect's override velocity, and a RandomFloat() state blend.
        static void TriggerLocators(BrnParticle::ParticleModule& lParticleModule,
                                    f32 lfCurrentTimeStep,
                                    f32 lfCurrentTime,
                                    const rw::math::vpu::Matrix44Affine& lPropTransform,
                                    const BrnParticle::VFXLocator* lpLocatorArray,
                                    u32 lNumLocators,
                                    const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                    CgsNumeric::Random& lRandom);

    private:
        // CreateEffect @ 0x822936F0 (DWARF :104) -- round-robin one of the five VFX slots: stop the
        // slot if it is playing, start a fresh LION effect under the given name hash,
        // record the handle, return the resolved playing slot.
        static BrnParticle::LionEffect* CreateEffect(BrnParticle::ParticleModule& lParticleModule,
                                                     u32 lHashName);

        // File-scope statics (DWARF :97 / :98; X360 dword_82FAB698 / dword_82FAB624[5]).
        static u32 mNextEffect;
        static u32 maEffectHandles[kuSizeOfEffectsArray];
    };

    // DWARF PropCollisions.h:37 -- the two materials of one prop type: unbroken (a hit) and smashing.
    struct PropToVFXMaterialMapping
    {
        const BrnParticle::VFXMaterial* mpUnBrokenVFXMaterial;   // :38 (console +0x00)
        const BrnParticle::VFXMaterial* mpSmashingVFXMaterial;   // :39 (console +0x04)

        PropToVFXMaterialMapping() : mpUnBrokenVFXMaterial(0), mpSmashingVFXMaterial(0) {}   // :41
    };

    // DWARF PropCollisions.h:45 -- 0x1000 bytes on the console, embedded at ParticleModule +0x4270.
    class PropCollisions
    {
    public:
        // DWARF PropCollisions.h:82 maPropToMaterialMappings[500] (== BrnPhysics::Props::KU_MAX_PROP_TYPES).
        static const u32 KU_MAX_PROP_TYPE_MAPPINGS = 500;

        typedef CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent> PropVFXLocatorQueue;
        typedef BrnPhysics::Vehicle::RaceCarState                                     RaceCarState;

        // :48 -- LoadFXBundle stage 18 stores the prop physics data's handle (`std r11, 0x4290(r31)`).
        void SetPropDataResource(CgsResource::ResourceHandle lHandle) { mPropDataResourceHandle = lHandle; }
        // :53 -- LoadFXBundle stage 14 binds the VFX prop collection (`CreateFromHandle(this + 0x4270, &handle)`).
        void SetPropCollection(CgsResource::ResourceHandle lHandle) { mVFXPropCollection = lHandle; }

        // :58 @0x822937A8.
        void Initialise();

        // :63 -- inlined by UpdateLocatorVfx (`cmplwi r30, 0x1F4 ; bge`, 0x8229948C).
        const PropToVFXMaterialMapping* MapPropTypeToMaterial(u32 luPropTypeFromSpy) const;

        // :74 @0x822993A0. lMaterialOverride (r8) is never read by this build.
        void UpdateLocatorVfx(f32 lfCurrentTimeStep,
                              f32 lfCurrentTime,
                              BrnParticle::ParticleModule& lParticleModule,
                              const PropVFXLocatorQueue& lPropStateQueue,
                              s32 lMaterialOverride,
                              const RaceCarState* lpRaceCarState,
                              const BrnDirector::Camera::Camera* lpCamera);

    private:
        // :85 @0x82288640.
        void BuildPropToMaterialTable();

        // :91 -- inlined by UpdateLocatorVfx (0x82299414..0x82299448).
        bool ContactVisible(rw::math::vpu::Vector3 lPoint, const BrnDirector::Camera::Camera* lpCamera);

        CgsResource::ResourcePtr<BrnParticle::VFXPropCollection> mVFXPropCollection;   // :80  console +0x00
        CgsResource::ResourceHandle                              mPropDataResourceHandle; // :81  console +0x20
        PropToVFXMaterialMapping maPropToMaterialMappings[KU_MAX_PROP_TYPE_MAPPINGS];  // :82  console +0x28
        CgsNumeric::Random                                       mRandom;              // :83  console +0xFD0
    };
}

#endif // GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H
