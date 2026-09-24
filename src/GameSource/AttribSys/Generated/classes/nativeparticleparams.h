#pragma once

// Attrib::Gen::nativeparticleparams -- generated AttribSys class: the authored parameters of one
// native simple-particle array (impact smoke, crash impact dust, the ten skid-smoke surfaces).
// Consumer: BrnParticle::Native::BrnSimpleParticleArray::UpdateParams @0x8228C6E0, fed by
// BrnEffects::EffectsModule::LoadNativeParticleParams @0x82290510.
//
// The X360 inlines this class entirely (it has no out-of-line ctor in the ARTIST ledger):
// LoadNativeParticleParams builds each instance by hand --
//     0x82290620  bl  Attrib::StringToKey                (the collection key, e.g. "504146")
//     0x82290624  lis r11,-0x17CA ; ori r3,r11,0x238A     -> r3  = 0xE836238A
//     0x82290630  lis r11,0x43DA  ; ori r11,r11,0x904B    -> r11 = 0x43DA904B
//     0x82290638  insrdi r3, r11, 32, 0                   -> r3  = 0x43DA904B_E836238A
//     0x8229063C  bl  Attrib::FindCollection
//     0x8229064C  bl  Attrib::Instance::Instance(.., collection, 0)
//     0x82290654  (mpAttributeData == 0) -> Attrib::DefaultDataArea(0x90)
// -- which is exactly the sparkeffect keyed-ctor shape, reproduced here as that ctor.
// ⚠ The old LoadNativeParticleParams note spelled the low word 0xE836C90A; the immediate is
// 0x238A (the ori at 0x8229062C), i.e. -399105142 == 0xE836238A.
//
// Corroborated by the ported vault: build/game/POSTFX/POSTFXVAULT.BIN carries the little-endian
// class key 8A 23 36 E8 4B 90 DA 43 exactly twelve times -- one collection per array 1..12.
//
// ⚠ FLAG -- THE ACCESSOR NAMES ARE THE CONSUMER'S, NOT THE SCHEMA'S (the same caveat
// sparkeffect.h records: AttribSys keys attributes by hash and the shipped schema carries no
// strings). What is recovered exactly is which layout slot feeds which member of
// CB4ParticleArrayStandardParams -- every offset below is one load in UpdateParams, named for the
// member it is stored into:
//   +0x00/+0x10/+0x20  start / mid / end colour (Vector4, 0..1 -> *255 bytes)
//   +0x30 texture name (32-bit PtrN)   +0x34 use-drag (byte)     +0x38 start size
//   +0x3C/+0x40 rotation speed min/max +0x44 blend mode          +0x48/+0x4C tiles wide/high
//   +0x50 near fade  +0x54 near clip   +0x58 mid time            +0x5C mid size
//   +0x60 max screen size              +0x64/+0x68 lighting min/max
//   +0x6C life time  +0x70 gravity     +0x74 far fade            +0x78 far clip
//   +0x7C end size   +0x80 drag terminal velocity scale          +0x84 drag initial velocity scale
//   +0x88 drag duration
// The layout size is the ctor's DefaultDataArea(0x90) argument.
#include "types.hpp"                                                          // u32 / u64
#include "rw/math/vpu/types.h"                                                // Vector4
#include <cstring>                                                            // std::memcpy (TextAt)
#include <cstdint>                                                            // uintptr_t  (TextAt)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"             // Attrib::FindCollection

namespace Attrib
{
namespace Gen
{
    class nativeparticleparams : private Instance
    {
    public:
        // The full 64-bit class key (LoadNativeParticleParams @0x82290624..0x82290638).
        static const u64 KU_NATIVEPARTICLEPARAMS_CLASS_KEY = 0x43DA904BE836238AULL;
        static const u32 KU_LAYOUT_SIZE = 0x90;

        rw::math::vpu::Vector4 StartColour()     const { return VectorAt(0x00u); }
        rw::math::vpu::Vector4 MidColour()       const { return VectorAt(0x10u); }
        rw::math::vpu::Vector4 EndColour()       const { return VectorAt(0x20u); }
        const char* TextureName()                const { return TextAt(0x30u); }
        u8   UseDrag()                           const { return ByteAt(0x34u); }
        f32  StartSize()                         const { return FloatAt(0x38u); }
        f32  RotationSpeedMin()                  const { return FloatAt(0x3Cu); }
        f32  RotationSpeedMax()                  const { return FloatAt(0x40u); }
        u32  BlendMode()                         const { return WordAt(0x44u); }
        u32  TilesWide()                         const { return WordAt(0x48u); }
        u32  TilesHigh()                         const { return WordAt(0x4Cu); }
        f32  NearFade()                          const { return FloatAt(0x50u); }
        f32  NearClip()                          const { return FloatAt(0x54u); }
        f32  MidTime()                           const { return FloatAt(0x58u); }
        f32  MidSize()                           const { return FloatAt(0x5Cu); }
        f32  MaxScreenSize()                     const { return FloatAt(0x60u); }
        f32  LightingMin()                       const { return FloatAt(0x64u); }
        f32  LightingMax()                       const { return FloatAt(0x68u); }
        f32  LifeTime()                          const { return FloatAt(0x6Cu); }
        f32  Gravity()                           const { return FloatAt(0x70u); }
        f32  FarFade()                           const { return FloatAt(0x74u); }
        f32  FarClip()                           const { return FloatAt(0x78u); }
        f32  EndSize()                           const { return FloatAt(0x7Cu); }
        f32  DragTerminalVelocityScale()         const { return FloatAt(0x80u); }
        f32  DragInitialVelocityScale()          const { return FloatAt(0x84u); }
        f32  DragDuration()                      const { return FloatAt(0x88u); }

        using Instance::IsValid;

        // The keyed ctor LoadNativeParticleParams inlines: resolve (class, luCollectionKey),
        // wrap it, and fall back to a zeroed 0x90-byte data area when the collection is absent.
        nativeparticleparams(u64 luCollectionKey, void* lpOwner)
            : Instance(FindCollection(KU_NATIVEPARTICLEPARAMS_CLASS_KEY, luCollectionKey), lpOwner)
        {
            if (!mpAttributeData)
                mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
        }

    private:
        // The layout block is a packed array of 4-byte attribute slots; the console offsets ARE
        // the host offsets (sparkeffect.h records the same reasoning for its own SlotAt).
        const void* SlotAt(u32 luOffset) const
        {
            return static_cast<const u8*>(GetLayoutPointer()) + luOffset;
        }
        f32 FloatAt(u32 luOffset) const { return *static_cast<const f32*>(SlotAt(luOffset)); }
        u32 WordAt(u32 luOffset)  const { return *static_cast<const u32*>(SlotAt(luOffset)); }
        u8  ByteAt(u32 luOffset)  const { return *static_cast<const u8*>(SlotAt(luOffset)); }
        rw::math::vpu::Vector4 VectorAt(u32 luOffset) const
        {
            return *static_cast<const rw::math::vpu::Vector4*>(SlotAt(luOffset));
        }
        // The name slot is a 32-bit PtrN fixup target that stays 32-bit on disk; the PC resource
        // arena is allocated below 4 GiB, so widen only after reading it (sparkeffect.h:TextAt).
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
}
}
