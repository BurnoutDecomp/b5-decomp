#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// BrnPhysics::Props::PropDebugComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX, with the
// member NAMES taken verbatim from the DecFIGS dwarfdump (BrnPropDebugComponent.h:54..113).
//
// ============================================================================================
// WHY THE MEMBER LIST IS 16 LONG AND sizeof IS EXACTLY 0x48 -- proved, not assumed
// ============================================================================================
// The seven OnChange* callbacks each read ONE member of the object handed to them as
// lpUserData, and the ARTIST asm pins the offset of each read:
//
//     OnChangeGravityScale                @0x825BAEF0   lfs f0, 0x14(r4)
//     OnChangeInertiaScale                @0x825BAF28   lfs f0, 0x18(r4)
//     OnChangeAntiHerdUpwardScale         @0x825BAF60   lfs f0, 0x1C(r4)
//     OnChangeAntiHerdSideScale           @0x825BAF98   lfs f0, 0x20(r4)
//     OnChangeAntiHerdHighSpeedSideScale  @0x825BAFD0   lfs f0, 0x24(r4)
//     OnChangeMaxSpeedForSideForce        @0x825BB008   lfs f0, 0x28(r4)
//     OnChangeAntiHerdSpeedClamp          @0x825BB040   lfs f0, 0x2C(r4)
//
// Construct @0x825BAD58 pins the three before them: stw r30,0xC(r31) / stb r11,0x10(r31) /
// stb r10,0x11(r31) == mpPropManager, mbRenderStats, mbRenderWorldContacts. And the base
// CgsDev::DebugComponent sub-object therefore occupies +0x00..+0x0B.
//
// That is a gap-free run of ten members ending at +0x30 -- but the object is NOT 0x30 bytes.
// PropManager embeds this class BY VALUE at its own +0x00 (PropManager::Construct @0x82627390
// calls PropDebugComponent::Construct with r3 == r4 == this), and TWO independent ARTIST sites
// put PropManager::mbRenderCOM -- the member immediately after mDebugComponent -- at +0x48:
//
//     PropManager::Construct   @0x82627404   stb r30, 0x48(r31)
//     PropDebugComponent::OnActivate @0x825E39E0   lwz r11,0xC(r31) ; addi r4,r11,0x48
//                                                  ("Render prop centre of mass")
//
// A struct whose widest member is 4 bytes cannot carry 0x18 bytes of trailing padding, so six
// more 4-byte members MUST occupy +0x30..+0x47. The dwarfdump names exactly six there, in
// declaration order, and they close the arithmetic exactly: 0x30 + 6*4 == 0x48.
//   -> the six mfLeanProp* members below are NOT speculative; their COUNT and WIDTH are forced
//      by the ARTIST image and only their NAMES come from the DWARF.
//
// ============================================================================================
// WHAT THE ARTIST BUILD DOES *NOT* HAVE, stated rather than back-filled
// ============================================================================================
// The DecFIGS (PS3) build of this same class has SIX more OnChange* callbacks (one per
// mfLeanProp* member, PS3 0x6B54AC/0x6B5C80..0x6B5D90) plus RenderWorld and GetPath overrides.
// The ARTIST image has NONE of them: the whole export set contains seven OnChange* symbols, no
// PropDebugComponent::RenderWorld and no PropDebugComponent::GetPath, and ARTIST's OnActivate
// @0x825E3628 registers exactly the seven variables below plus five namespace-scope floats.
// So the six lean-prop members are storage the ARTIST build never wires up, mbRenderWorldContacts
// is registered but read by nothing in this image, and no callback/override is invented here.
namespace BrnPhysics
{
namespace Props
{
    class PropManager;

    class PropDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // 0x825BAD58 (DWARF BrnPropDebugComponent.cpp:47) -- base Construct, assert the owner is
        // non-null, then seed the three non-tuning members.
        void Construct(PropManager* lpPropManager);

        // DWARF BrnPropDebugComponent.cpp:67. The ARTIST build has no out-of-line symbol for it:
        // it is inlined into PropManager::Destruct @0x825E3398, which asserts on *(this+0xC),
        // nulls it, and tail-calls the (ICF-folded) empty base Destruct.
        void Destruct();

        // 0x82628EB0 -- `if (mbRenderStats) RenderStats(lpRender);`, verbatim.
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

    protected:
        // 0x825BADD0 -- returns "Prop Manager".
        const char* GetName() const override;

        // 0x825E3628.
        void OnActivate() override;

        // 0x822A9750 -- a single `b` to the empty (ICF-folded) base OnRegister.
        void OnRegister() override;

    private:
        // 0x826131E8 -- the three-line prop-manager stats block.
        void RenderStats(CgsDev::Debug2DImmediateRender* lpRender);

        static void OnChangeGravityScale(void* lpValue, void* lpUserData);
        static void OnChangeInertiaScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdUpwardScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdSideScale(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdHighSpeedSideScale(void* lpValue, void* lpUserData);
        static void OnChangeMaxSpeedForSideForce(void* lpValue, void* lpUserData);
        static void OnChangeAntiHerdSpeedClamp(void* lpValue, void* lpUserData);

        PropManager* mpPropManager;                 // X360 +0x0C
        bool mbRenderStats;                         // X360 +0x10
        bool mbRenderWorldContacts;                 // X360 +0x11
        f32 mfGravityScale;                         // X360 +0x14
        f32 mfInertiaScale;                         // X360 +0x18
        f32 mfAntiHerdUpwardScale;                  // X360 +0x1C
        f32 mfAntiHerdSideScale;                    // X360 +0x20
        f32 mfAntiHerdHighSpeedSideScale;           // X360 +0x24
        f32 mfMaxSpeedForSideForce;                 // X360 +0x28
        f32 mfAntiHerdSpeedClamp;                   // X360 +0x2C
        // The six the ARTIST build allocates but never registers (see the banner).
        f32 mfLeanPropsSpeed;                       // X360 +0x30
        f32 mfLeanPropsSpeedClamp;                  // X360 +0x34
        f32 mfLeanPropForceOnCar;                   // X360 +0x38
        f32 mfLeanPropCarTranslation;               // X360 +0x3C
        f32 mfLeanPropBreakLinearVel;               // X360 +0x40
        f32 mfLeanPropBreakAngularVel;              // X360 +0x44
    };                                              // X360 sizeof == 0x48
}
}
