#ifndef GAMESOURCE_EFFECTS_BRNEFFECTSDEBUGCOMPONENT_H
#define GAMESOURCE_EFFECTS_BRNEFFECTSDEBUGCOMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// ============================================================================
// GameSource/Effects/BrnEffectsDebugComponent.h
//
// BrnEffects::EffectsDebugComponent - the in-game debug menu for the effects
// (VFX) module. It derives from CgsDev::DebugComponent and, on activation,
// registers a large tree of debug variables (post-process toggles, per-feature
// render enables, the LION / Boost / Jump / Props / Junkyard sub-menus) with the
// debug UI. RenderWorld draws the junkyard-VFX axis gizmo when junkyard editing
// is active.
//
// SOURCE-OF-TRUTH: shape recovered from the DecFIGS DWARF
// (GameSource/Effects/BrnEffectsDebugComponent.h) and gated on the X360 ARTIST
// ledger; member OFFSETS / behaviour verified against the ARTIST asm
// (Construct 0x82278C98, OnActivate 0x8228F3E8, the Setup* / OnJunkyard* funcs).
//
// LAYOUT (verified from the ARTIST asm store offsets; base CgsDev::DebugComponent
// occupies the first 0xC bytes - vtable ptr + mbActive + mpDebugLinkedListNext):
//   +0x0C mpEffectsModule           +0x3C..0x42 post-process toggles
//   +0x10..0x1C per-feature render enables
//   +0x24 mTagPointIndex            +0x44/0x48 motion-blur user amounts
//   +0x28 mfSimulationRate          +0x4C mbMotionBlurUserHighQuality
//   +0x2C mfForceStateBlendValue    +0x4D/0x4E junkyard edit/show-axes flags
//   +0x30 mfTransitionMin           +0x50 mnJunkyardVfxId
//   +0x34 mfTransitionMax           +0x54/0x58/0x5C/0x60 junkyard pos X/Y/Z + angle
//   +0x38 mfBoostExhaustCutOffSpeed +0x64..0x74 mJumpParams (EffectsDebugJumping)
//                                   +0x78/0x7C mPropParams (EffectsDebugProps)
//   sizeof == 0x80.
// ============================================================================

namespace BrnEffects
{
    // The effects (VFX) module this debug component drives. Reached only by pointer
    // here; full layout lives in its own header (GameSource/Effects/EffectsModule.h).
    class EffectsModule;

    // --- EffectsDebugJumping (mJumpParams, +0x64) --------------------------------
    // The "Jumping" sub-menu's edited values. DWARF home BrnEffectsDebugComponent.h:47.
    // Offsets verified from SetupJumpMenu (0x82279108): +0 force-jump flag, +4 landing
    // spark speed, +8 landing spark time, +0xC vapour start delay, +0x10 vapour ramp.
    struct EffectsDebugJumping
    {
        EffectsDebugJumping();

        bool  IsForceJumping() const   { return mbForceJumping; }
        f32   LandingSparksTime() const{ return mfLandingSparksTime; }
        f32   VapourStartDelay() const { return mfVapourDelayTime; }
        f32   VapourRampEndTime() const{ return mfVapourRampTime; }

        bool  mbForceJumping;          // +0x00
        f32   mfJumpLandingSparkSpeed; // +0x04
        f32   mfLandingSparksTime;     // +0x08
        f32   mfVapourDelayTime;       // +0x0C
        f32   mfVapourRampTime;        // +0x10
    };

    // --- EffectsDebugProps (mPropParams, +0x78) ----------------------------------
    // The "Props" sub-menu's edited values. DWARF home BrnEffectsDebugComponent.h:87.
    struct EffectsDebugProps
    {
        EffectsDebugProps();

        bool mbEnable;        // +0x00 ("Force Props")
        u32  mMaterialIndex;  // +0x04 ("Material Index")
    };

    // --- EffectsDebugComponent ---------------------------------------------------
    class EffectsDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // X360 0x82278C98. Wires the owning module and resets every debug value to
        // its default (asserts lpEffectsModule != NULL).
        void Construct(EffectsModule* lpEffectsModule);
        void Destruct();

        // X360 0x82278DB8. Draws the junkyard-VFX axis gizmo when junkyard editing
        // is active (and resets the junkyard editor when the module leaves edit mode).
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender);

        // Query accessors used by the effects module (declared per the DWARF; bodies
        // are trivial inline reads, kept here so callers compile against the shape).
        bool IsForceStateBlend() const          { return mbForceStateBlend; }
        f32  ForceStateBlendValue() const        { return mfForceStateBlendValue; }
        bool IsBoostTransitionEditing() const    { return mbBoostTransition; }
        f32  BoostTransitionMin() const          { return mfTransitionMin; }
        f32  BoostTransitionMax() const          { return mfTransitionMax; }
        f32  BoostExhaustCutOffSpeed() const     { return mfBoostExhaustCutOffSpeed; }
        const EffectsDebugJumping& JumpParams() const { return mJumpParams; }
        const EffectsDebugProps&   PropParams() const { return mPropParams; }
        bool ForceNonPlayerEffects() const       { return mbForceNonPlayerEffects; }

    protected:
        // X360 0x82278EB8 / 0x8228F3E8 (DebugComponent virtual overrides).
        virtual const char* GetName() const;
        virtual void        OnActivate();

    private:
        // Sub-menu builders called from OnActivate.
        void SetupLionMenu();   // X360 0x82278EC8
        void SetupBoostMenu();  // X360 0x82279010
        void SetupJumpMenu();   // X360 0x82279108

        // Junkyard-VFX edit callbacks (registered with SetChangeCallback). The debug UI
        // invokes them with the changed-variable pointer (unused) and the user-data the
        // component registered (this). They re-sync the playing LION effects with the
        // junkyard editor state. Modelled as static so the &Fn / this user-data pair the
        // X360 registers matches the asm (r4 = component, passed as the user-data).
        static void OnJunkyardEffectEditChange(void* lpValue, void* lpUserData);  // X360 0x82287B68
        static void OnJunkyardEffectValueChange(void* lpValue, void* lpUserData); // X360 0x82287C10
        static void OnJunkyardEffectIndexChange(void* lpValue, void* lpUserData); // X360 0x8227F120

        // ----- data members (offsets relative to the EffectsDebugComponent object) --
        EffectsModule* mpEffectsModule;             // +0x0C

        bool  mbBypassAllVFXProcessing;             // +0x10 "Enable/Disable all VFX Update()"
        bool  mbSparksEnabled;                      // +0x11
        bool  mbTrailsEnabled;                      // +0x12
        bool  mbDebrisEnabled;                      // +0x13
        bool  mbGlassEnabled;                       // +0x14
        bool  mbPopcornEnabled;                     // +0x15
        bool  mbSimpleEnabled;                      // +0x16
        bool  mbLionEnabled;                        // +0x17
        bool  mbLionStatisticsEnabled;              // +0x18
        bool  mbUseZFade;                           // +0x19
        bool  mbEngineCrashEffectsEnabled;          // +0x1A
        bool  mbSparksDebugEnabled;                 // +0x1B
        bool  mbQAEnabled;                          // +0x1C
        bool  mbEnabled;                            // +0x1D "Enable/Disable Effects debugging"
        bool  mbForceStateBlend;                    // +0x1E
        bool  mbBoostTransition;                    // +0x1F
        bool  mbForceFinishLineRendering;           // +0x20
        bool  mbDumpRunningLionEffects;             // +0x21
        bool  mbForceNonPlayerEffects;              // +0x22
        u8    mPad23[1];                            // +0x23 (alignment to the int below)

        s32   mTagPointIndex;                       // +0x24
        f32   mfSimulationRate;                     // +0x28 "Lion Simulation Rate"
        f32   mfForceStateBlendValue;               // +0x2C "Force State Value"
        f32   mfTransitionMin;                      // +0x30
        f32   mfTransitionMax;                      // +0x34
        f32   mfBoostExhaustCutOffSpeed;            // +0x38

        bool  mbBloom;                              // +0x3C
        bool  mbVignette;                           // +0x3D
        bool  mbDepthOfField;                       // +0x3E
        bool  mbTint;                               // +0x3F
        bool  mbTint2d;                             // +0x40
        bool  mbMotionBlur;                         // +0x41
        bool  mbMotionBlurEnableUserSettings;       // +0x42
        u8    mPad43[1];                            // +0x43 (alignment to the float below)
        f32   mfMotionBlurUserAmountCars;           // +0x44
        f32   mfMotionBlurUserAmountWorld;          // +0x48
        bool  mbMotionBlurUserHighQuality;          // +0x4C
        bool  mbJunkyardVfxEdit;                    // +0x4D "Edit junkyard VFX"
        bool  mbJunkyardVfxShowAxes;                // +0x4E "Show axes"
        u8    mPad4F[1];                            // +0x4F (alignment to the int below)
        s32   mnJunkyardVfxId;                      // +0x50 "Effect index"
        f32   mfJunkyardVfxPosX;                    // +0x54
        f32   mfJunkyardVfxPosY;                    // +0x58
        f32   mfJunkyardVfxPosZ;                    // +0x5C
        f32   mfJunkyardVfxAngle;                   // +0x60

        EffectsDebugJumping mJumpParams;            // +0x64
        EffectsDebugProps   mPropParams;            // +0x78
    };
}

#endif // GAMESOURCE_EFFECTS_BRNEFFECTSDEBUGCOMPONENT_H
