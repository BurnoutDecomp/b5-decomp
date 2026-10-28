#pragma once

// Attrib::Gen::sparkeffect -- generated AttribSys class (spark-effect burst
// parameters, fed to BrnEffects::EffectsModule::Prepare). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::sparkeffect::sparkeffect @ 0x8227EC10
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling generated classes shotgroup / debrisparams / iceanim. The X360
// build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only sparkeffect function in the ledger (minimal, X360-faithful
// recon). Unlike debrisparams/iceanim/surfacelist, the X360 ctor body has NO
// AssertOnClassCheck call -- it resolves its own collection via FindCollection (like
// shotgroup) and only guards the default data area. Derives from Attrib::Instance.
#include "types.hpp"                                                          // u32
#include "rw/math/vpu/types.h"                                                // Vector4 (the colour / lifetime slots)
#include <cstring>                                                            // std::memcpy (TextAt)
#include "types.hpp"                                                            // uintptr_t  (TextAt)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class sparkeffect : private Instance
    {
    public:
        // The sparkeffect class key the ctor resolves its collection against. The X360
        // ctor builds this as the low 32 bits of a 64-bit immediate (0xCAF7F032_BCD68E9C);
        // Hex-Rays collapses the call to the single int FindCollection(-1126789476) --
        // the high half is a dead/incidental upper word of the 64-bit immediate load, not
        // a second argument (FindCollection reads only a 32-bit key here, matching the
        // sibling shotgroup ctor's shape).
        static const int KI_SPARKEFFECT_CLASS = -1126789476; // 0xBCD68E9C

        // The FULL 64-bit class key Attrib::FindCollection resolves against -- the
        // doubleword the X360 ctor stages in r3 with lis/ori + insrdi. KI_KI_SPARKEFFECT_CLASS
        // above is only its LOW word (which is what Hex-Rays surfaces, and what this
        // header used to pass to the old one-key FindCollection(int)); the class
        // registry is keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_SPARKEFFECT_CLASS_KEY = 0xCAF7F032BCD68E9CULL;

        // ====================================================================
        // The generated per-attribute accessor API, modelled back out of the
        // X360's inlined reader (the same convention boostparamsasset.h and
        // vehicleengine.h already use). ⚠ FLAG -- THESE NAMES ARE THE
        // CONSUMER'S, NOT THE SCHEMA'S. AttribSys keys every attribute by a
        // 64-bit hash and the shipped schema carries no strings (grepped:
        // build/game/schema.bin holds no "spark*" text), so the real attribute
        // names are not recovered. What IS recovered, exactly, is which slot
        // feeds which SparkArray member: EffectsModule::Prepare @0x8229E910..
        // 0x8229E9E0 copies this 0x90-byte data area into one SparkArray with
        // one load per member, and every offset below is one of those loads.
        //   +0x00/+0x10/+0x20/+0x30  the four colour Vector4s
        //                            (maColours[i] <- Colour(3 - i); the
        //                             console's copy is REVERSED -- see
        //                             SparkArray::UpdateParams)
        //   +0x40                    the four lifetimes, as one Vector4
        //   +0x50                    the texture name (a 32-bit PtrN slot)
        //   +0x68 radius   +0x74 motion blur   +0x78 gravity
        //   +0x7C drag terminal   +0x80 drag initial   +0x84 drag duration
        //   +0x88 bounce
        // The layout size is the ctor's own DefaultDataArea(0x90) argument.
        // ====================================================================
        static const u32 KU_LAYOUT_SIZE = 0x90;

        rw::math::vpu::Vector4 Colour(u32 luIndex) const { return VectorAt(luIndex * 0x10u); }
        rw::math::vpu::Vector4 Lifetimes()         const { return VectorAt(0x40u); }
        const char* SparkTextureName()             const { return TextAt(0x50u); }
        f32 SparkRadius()                          const { return FloatAt(0x68u); }
        f32 MotionBlurTime()                       const { return FloatAt(0x74u); }
        f32 GravityStrength()                      const { return FloatAt(0x78u); }
        f32 DragTerminalVelocityScale()            const { return FloatAt(0x7Cu); }
        f32 DragInitialVelocityScale()             const { return FloatAt(0x80u); }
        f32 DragDuration()                         const { return FloatAt(0x84u); }
        f32 BounceStrength()                       const { return FloatAt(0x88u); }

        // ⭐ ADDITIVE 2026-09-06 (spark-producer wave). Six more slots of the SAME 0x90 layout,
        // every one of them a load in BrnEffects::EffectsModule::HandleSparkContacts @0x822906A8
        // off `lwz r10, 4(r30)` with r30 == &mSparkParams[type]:
        //   0x82290878 lfs f0,  0x5C(r10) / 0x82290880 lfs f13, 0x60(r10)
        //        -> min/max of the RANDOM SEGMENT LENGTH the spark line is drawn along
        //           (the result multiplies the normalised friction-stress direction);
        //   0x82290930 lfs f12, 0x70(r8)  / 0x82290934 lfs f0,  0x6C(r8)
        //        -> min/max of the number of sparks, which lands in the event's mfNumSparks;
        //   0x822909D0 lfs f0,  0x54(r10) / 0x82290A0C lfs f0,  0x58(r10)
        //        -> the event's mfVelocityInheritanceMin / Max, copied straight through.
        // ⚠ SAME FLAG AS THE BLOCK ABOVE: the names are the CONSUMER'S, not the schema's.
        f32 VelocityInheritanceMin()               const { return FloatAt(0x54u); }
        f32 VelocityInheritanceMax()               const { return FloatAt(0x58u); }
        f32 SparkLineLengthMin()                   const { return FloatAt(0x5Cu); }
        f32 SparkLineLengthMax()                   const { return FloatAt(0x60u); }
        f32 NumSparksMin()                         const { return FloatAt(0x6Cu); }
        f32 NumSparksMax()                         const { return FloatAt(0x70u); }

        using Instance::IsValid;

        // Construct over the sparkeffect collection, optionally owned by lpOwner.
        explicit sparkeffect(void* lpOwner = 0);
        // The X360 ctor's r4 IS the collection key (it is passed straight through to
        // FindCollection(class, key)); EffectsModule::Prepare @0x8229E690 hands it
        // Attrib::StringToKey("376835"/"376836"/"376837"/"554431"). Additive (2026-09-02).
        sparkeffect(u64 luCollectionKey, void* lpOwner);

    private:
        // The layout block is a packed array of 4-byte attribute slots; Float/Vector4 slots
        // are the same width on console and host, so the console offsets ARE the host
        // offsets (the reasoning boostparamsasset.h:216-227 records for its own SlotAt).
        const void* SlotAt(u32 luOffset) const
        {
            return static_cast<const u8*>(GetLayoutPointer()) + luOffset;
        }
        f32 FloatAt(u32 luOffset) const
        {
            return *static_cast<const f32*>(SlotAt(luOffset));
        }
        rw::math::vpu::Vector4 VectorAt(u32 luOffset) const
        {
            return *static_cast<const rw::math::vpu::Vector4*>(SlotAt(luOffset));
        }
        // The name slot is a 32-bit PtrN fixup target that stays 32-bit on disk; the PC
        // resource arena is allocated below 4 GiB, so widen only after reading it (the
        // identical treatment vehicleengine.h:114-130 gives its seven string attributes).
        const char* TextAt(u32 luOffset) const
        {
            if (!GetLayoutPointer())
                return 0;
            u32 luAddress = 0;
            std::memcpy(&luAddress, static_cast<const u8*>(GetLayoutPointer()) + luOffset,
                        sizeof(luAddress));
            return reinterpret_cast<const char*>(static_cast<uintptr_t>(luAddress));
        }
    };

    inline sparkeffect::sparkeffect(u64 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_SPARKEFFECT_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }

    // X360 ctor @0x8227EC10: Collection = FindCollection(KI_SPARKEFFECT_CLASS); chain the
    // Instance ctor over it; then give the instance a default data area (0x90 bytes) if
    // it has none. No class-check assert in this ctor (unlike debrisparams/iceanim).
    inline sparkeffect::sparkeffect(void* lpOwner)
                // FLAG (collection key): the X360 ctor never writes r4, so the CALLER's key
        // argument passes straight through to FindCollection as the collection key.
        // This ctor does not model that parameter yet (no call site in this repo
        // supplies one), so it resolves the class's collection key 0 -- exactly what
        // the previous `FindCollection(KI_..., nullptr)` form did. Add the parameter
        // when a real call site needs a named collection.
    : Instance(FindCollection(KU_SPARKEFFECT_CLASS_KEY, 0), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
