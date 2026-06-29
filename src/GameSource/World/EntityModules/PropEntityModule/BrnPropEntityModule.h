#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h
//
// BrnWorld::PropEntityModule -- the world entity module that owns every loaded
// destructible prop: its PropZoneManager (instance pools + per-zone slots + cell grid),
// the loaded prop-physics data header, and the runtime tuning flags exposed to the debug
// menu. The full module is large and is reconstructed by its own TU; this header is a
// PARTIAL recovery that declares only the members the prop debug overlay
// (PropEntityDebugComponent) reaches, with explicit padding for the unreconstructed gaps
// so the named members land at their X360-verified offsets.
//
// Reconstructed from:
//   - DecFIGS DWARF (BrnPropEntityModule.h)   -> member names/types/order (X360-gated).
//   - BURNOUT_X360_ARTIST.XEX asm             -> member offsets (authoritative). The
//     PropEntityDebugComponent::OnActivate (0x822C52C8) + RenderProps (0x822DCBF8) bodies
//     pin: mZoneManager @ +0x280 (640), mpPropPhysicsDataHeader @ +0xCDD48 (843144),
//     mbUseOverrides @ +0xCDA20 (842080), mfOverrideLeanThreshold/Move/Smash @
//     +0xCDA24/+0xCDA28/+0xCDA2C, mbCurrentlyOnline @ +0xD3340 (865088),
//     mbAllowPropProgression @ +0xD3342 (865090). mbPropsEnabled is the first flag the
//     overlay registers ("Enable props"); the X360 OnActivate does not touch it directly
//     here, so it is modelled as a leading member of the post-ResourcePtr flag block.
//
// PADDING NOTE: the inter-member gaps are filled with `u8 mPadN[..]` sized purely to place
// the named members at their console offsets (provenance only; on the x64 PC compile this
// is not byte-exact). The module's own TU will replace the padding with the real members.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "BrnPropZoneManager.h"   // PropZoneManager (embedded by value @ +0x280)

namespace BrnPhysics { namespace Props { class PropPhysicsDataHeader; } }

namespace BrnWorld
{
    // Partial PropEntityModule -- ONLY the members the prop debug overlay reaches. The
    // padding preserves the X360 offsets of the named members; the full layout (base
    // CgsModule::ModuleSingleBuffered, the IO buffers, the streaming state machine, the
    // recently-recycled arrays, etc.) belongs to BrnPropEntityModule.cpp's own TU.
    class PropEntityModule
    {
    public:
        const BrnPhysics::Props::PropPhysicsDataHeader* GetPropPhysicsDataHeader() const
        {
            return mpPropPhysicsDataHeader;
        }

    public:
        // The prop streaming / reset state machine the module runs each frame and the debug
        // overlay's "Reset props" action pokes. DWARF-faithful (BrnPropEntityModule.h:93).
        enum EPropStreamingMode
        {
            E_STREAM                      = 0,
            E_DONT_STREAM                 = 1,
            E_RESET_UNLOADING             = 2,
            E_RESET_UNLOADING_FOR_PROFILE = 3,
            E_REQUESTING_PROFILE_DATA     = 4,
            E_WAITING_FOR_PROFILE_DATA    = 5,
        };

    public:
        // [+0x000 .. +0x27F] base (CgsModule::ModuleSingleBuffered) + prepare/release state
        // + leading module members -- not reconstructed here.
        u8 mPad0[640];                                       // -> mZoneManager @ +0x280

        PropZoneManager mZoneManager;                        // +0x280 (640)

        // [end of mZoneManager .. mbPropsEnabled] -- IO buffers / streaming state / etc.
        // mZoneManager console sizeof is 841312 (0x0CD660); the next named member the
        // overlay reaches (mbUseOverrides) sits at +0xCDA20, i.e. 0x3C0 past the manager.
        u8 mPad1[0x3C0];                                     // -> +0xCDA20

        bool mbUseOverrides;                                 // +0xCDA20 (842080)
        u8   mPad2[3];
        f32  mfOverrideLeanThreshold;                        // +0xCDA24 (842084)
        f32  mfOverrideMoveThreshold;                        // +0xCDA28 (842088)
        f32  mfOverrideSmashThreshold;                       // +0xCDA2C (842092)

        // mpPropPhysicsDataHeader @ +0xCDD48 (843144); 0x318 past mbUseOverrides.
        u8 mPad3[0x318];                                     // -> +0xCDD48
        const BrnPhysics::Props::PropPhysicsDataHeader* mpPropPhysicsDataHeader; // +0xCDD48

        // meStreamingMode @ +0xD3200 (864768): the prop streaming/reset state. The debug
        // overlay's "Reset props" action (PropEntityDebugComponent::ResetProps @0x822A9758)
        // writes E_RESET_UNLOADING(=2) here -- asm: `lwz r11,0x34(this)` (mpPropEntityModule)
        // then `stwx <2>, r11, 0xD3200`.
        u8                mPad4a[0x54B4];                    // -> +0xD3200
        EPropStreamingMode meStreamingMode;                 // +0xD3200 (864768)

        // muNumberOfLoadedZones @ +0xD320C (864780): the running loaded-zone count the
        // module-stats overlay prints (" Zones loaded: " <count>). RenderModuleStats
        // 0x822DE3F8 reads it as `*(module + 864780)` (asm 0x822DEA14-0x822DEA28:
        // `lwzx r4, module, 0xD320C`). The 8-byte gap holds the DWARF muMaxLoadedZones /
        // muZonesLoaded the module's own TU will name.
        u8   mPad4z[0x8];                                    // -> +0xD320C
        u32  muNumberOfLoadedZones;                          // +0xD320C (864780)

        // mbCurrentlyOnline @ +0xD3340 (865088); mbAllowPropProgression @ +0xD3342.
        u8   mPad4b[0x55F4 - 0x54C0 - sizeof(u32)];          // -> +0xD3340
        bool mbCurrentlyOnline;                              // +0xD3340 (865088)
        bool mbPropsEnabled;                                 // +0xD3341 ("Enable props")
        bool mbAllowPropProgression;                         // +0xD3342 (865090)

        friend class PropEntityDebugComponent;
    };
}
