#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex

namespace BrnDirector { namespace Camera { struct Camera; } }   // UpdatePanoramaScreenshots' param; pointer-only here

// BrnDirector::DebugComponent - the director module's in-game debug menu component
// (registered as "Camera" - GetName). Recovered from the DecFIGS DWARF
// (GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h).
//
// LAYOUT: the real CgsDev::DebugComponent base is 0xC bytes on the X360 spine
// (vptr + mbActive + mpDebugLinkedListNext over an empty DebugInternal base -- see
// CgsDebugComponent.h). Construct's asm (0x821F6FD8) stores mpDirectorModule at
// this+0xC -- i.e. it is this class's very first owned member, immediately after the
// base, with no gap. mbShowCameraPos/mbShowCrashShotInfo/mbTakePanoramaScreenshot
// follow it byte-for-byte (this+0x10/0x11/0x12 -- both Construct's stores and
// UpdatePanoramaScreenshots' this+0x12/0x14/0x18 loads agree), so those five members
// are asm-attested in this exact order.
//
// FLAG: mePlayerCarIndexOverride (DWARF BrnDirectorModuleDebugCompononent.h:68) is
// declared `public` ahead of the private block in the DWARF text, but none of the 7
// functions recovered from this build's ARTIST export (Construct, GetName, OnActivate,
// RenderHUD, StartEditor, TakePanorama, UpdatePanoramaScreenshots) ever touch it, so its
// byte offset is NOT asm-attested. Per the project's offset-authority rule (pseudocode/
// asm over DWARF member order), it is declared last here rather than guessed ahead of
// the asm-verified members -- placing it first would silently shift mpDirectorModule
// off the address Construct's `stw r30, 0xC(r31)` proves. Move it if a future function
// in another TU proves its true offset.
namespace BrnDirector
{
    class DirectorModule;   // pointer member; full layout not yet modelled (see BrnDirectorModule.h)

    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // X360 0x821F6FD8. Sets the owning director module and clears the panorama /
        // camera-pos / crash-shot debug toggles.
        void Construct(DirectorModule* lpDirectorModule);

        // DWARF-declared (BrnDirectorModuleDebugCompononent.cpp:75); no body recovered in
        // this build's ARTIST export -- no asm/pseudocode evidence to reconstruct from.
        void Destruct();

        // X360 0x82209A68. Draws the camera HUD debug overlay (camera X/Y/Z, FOV, lens
        // length, look-at, near clip). BLOCKED: reads dozens of BrnDirector::DirectorModule
        // sub-object fields through mpDirectorModule by raw byte offset (the +211920/
        // +211924/... family) and DirectorModule's real layout is not yet modelled
        // (BrnDirectorModule.h is still constructor-only) -- named-member access is not
        // possible yet. Declaration-only per AGENTS.md (no raw offset-cast access).
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

        // X360 0x8221B470. Advances the panorama screenshot state machine (pitch/yaw step
        // grid) each time a panorama shot is requested. BLOCKED: heavy VMX quaternion/matrix
        // math (vupkd3d128/vmaddfp/vrfin chains building the pan/tilt rotation) with no
        // recovered semantic names for the constant tables (unk_82000BD0 etc.) -- reproducing
        // it faithfully without fabricating the math is not possible from this export.
        // Declaration-only.
        void UpdatePanoramaScreenshots(Camera::Camera* lpCamera);

    protected:
        // X360 0x821F7050.
        const char* GetName() const override;

        // X360 0x82275F68. Registers every camera/testbed/crash debug variable and action
        // with the debug menu. BLOCKED: reaches ~40 BrnDirector::DirectorModule sub-object
        // fields through mpDirectorModule by raw byte offset (same unmodelled-layout blocker
        // as RenderHUD) plus two still-todo callees (BehaviourParameterBank::Serialise<...>,
        // the unnamed `BrnDirec` helper). Declaration-only.
        void OnActivate() override;

    private:
        // Registered with the debug menu as DebugUI::Function::DebugCallbackFunction
        // (void(*)(void*)) callbacks (OnActivate: RegisterFunction(this, &SavePlaylists, this,
        // ...) / &StartEditor(..., this, ...) etc. -- the void* userData IS `this`), so these
        // four are STATIC per the X360 asm calling convention (single incoming param in r3 is
        // the callback's void* userData, not an implicit `this`): reinterpret_cast<DebugComponent*>
        // (lpUserData) inside the body recovers the real `this`.
        //
        // DWARF-declared (BrnDirectorModuleDebugCompononent.cpp:441/455); registered as debug
        // menu callbacks by OnActivate (which is itself declaration-only -- see above).
        // Bodies ARE recovered in this build's ARTIST export (SavePlaylists @0x8225EAC8,
        // LoadPlaylists @0x8225EB60: fopen "d:\\playlists.txt", then
        // SharedPlaylists::Serialise<TextFileWriteSerialiser|TextFileReadSerialiser|
        // DebugMenuSerialiser> over the playlist store), but BOTH stay DECLARATION-ONLY
        // (BLOCKED): the SharedPlaylists object they serialise sits at DirectorModule+0x13BD0
        // (asm: lwz r11,0xC(this) -> mpDirectorModule; addis+addi +0x13BD0), which lies inside
        // BrnDirector::MainDirector's opaque `alignas(16) u8 maStorage[0x35450]` placement region
        // (DirectorModule+0xB00..+0x35F50; BrnMainDirector.h). MainDirector exposes no
        // SharedPlaylists member, so reaching it would require a raw offset poke into another
        // class's opaque storage -- forbidden by AGENTS.md's NO-RAW-OFFSET rule for in-memory
        // engine objects. LoadPlaylists additionally needs SharedPlaylists::Serialise<
        // Camera::DebugMenuSerialiser>, whose DebugMenuSerialiser visitor type has no
        // reconstructed home anywhere in the tree. (SharedPlaylists + its Serialise<
        // TextFileWriteSerialiser> template ARE now homed in BrnICEMoviePlayer.h -- the only
        // remaining blockers are the MainDirector-opaque container access and, for Load, the
        // un-homed DebugMenuSerialiser.)
        static void SavePlaylists(void* lpUserData);
        static void LoadPlaylists(void* lpUserData);

        // X360 0x821F7060. Arms the panorama screenshot request (resets the pitch/yaw step
        // counters on the rising edge; a no-op while already armed).
        static void TakePanorama(void* lpUserData);

        // X360 0x821F7080. Creates a new ICE take named "New Take" and hands it to the
        // director's ICE editor. BLOCKED: reaches the ICEAuthor/ICEWrapper sub-objects
        // through mpDirectorModule by raw byte offset (this+0xB50+0x2750 / this+0xB50) --
        // same unmodelled-DirectorModule-layout blocker as OnActivate/RenderHUD, even though
        // both callees (ICEAuthor::CreateNewTake, ICEWrapper::EditorOn) are now reconstructed.
        // Declaration-only.
        static void StartEditor(void* lpUserData);

        // Asm-attested order (see the class-level FLAG comment above): mpDirectorModule is
        // this class's first owned member, at this+0xC, immediately after the CgsDev::
        // DebugComponent base.
        DirectorModule* mpDirectorModule;

        // this+0x10 / this+0x11 / this+0x12 (Construct's stores; the third is also read by
        // UpdatePanoramaScreenshots at this+0x12).
        bool mbShowCameraPos;
        bool mbShowCrashShotInfo;
        bool mbTakePanoramaScreenshot;

        // this+0x14 / this+0x18 (UpdatePanoramaScreenshots' loads/stores).
        s32 miPanoramaStepPitch;
        s32 miPanoramaStepYaw;

        // DWARF-declared (BrnDirectorModuleDebugCompononent.h:68), public in the original,
        // but not asm-attested by any function recovered in this TU -- see the class-level
        // FLAG comment. Declared last so it cannot silently shift the asm-verified members
        // above off their proven offsets.
        EActiveRaceCarIndex mePlayerCarIndexOverride;
    };
}
