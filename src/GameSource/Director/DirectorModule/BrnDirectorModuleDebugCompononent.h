#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex

namespace BrnDirector { namespace Camera { struct Camera; } }   // UpdatePanoramaScreenshots' param; pointer-only here

// BrnDirector::DebugComponent - the director module's in-game debug menu component
// (registered as "Camera" - GetName).
//
// LAYOUT: the CgsDev::DebugComponent base occupies +0x0..+0xB (vptr + mbActive +
// mpDebugLinkedListNext over an empty DebugInternal base -- see CgsDebugComponent.h).
// Construct places mpDirectorModule at +0xC -- i.e. it is this class's very first owned
// member, immediately after the base, with no gap. mbShowCameraPos /
// mbShowCrashShotInfo / mbTakePanoramaScreenshot follow it byte-for-byte at
// +0x10 / +0x11 / +0x12 (Construct's stores and UpdatePanoramaScreenshots' reads of
// +0x12 / +0x14 / +0x18 agree), so those five members are attested in this exact order.
//
// FLAG: mePlayerCarIndexOverride is declared `public` ahead of the private block in the
// original header, but none of the 7 recovered functions of this class (Construct,
// GetName, OnActivate, RenderHUD, StartEditor, TakePanorama, UpdatePanoramaScreenshots)
// ever touches it, so its byte offset is NOT attested. Per the project's offset-authority
// rule (attested access over declared member order), it is declared last here rather than
// guessed ahead of the verified members -- placing it first would silently shift
// mpDirectorModule off the +0xC that Construct proves. Move it if a future function in
// another TU proves its true offset.
namespace BrnDirector
{
    class DirectorModule;   // pointer member; full layout not yet modelled (see BrnDirectorModule.h)

    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Sets the owning director module and clears the panorama / camera-pos /
        // crash-shot debug toggles.
        void Construct(DirectorModule* lpDirectorModule);

        // Declared in the original header; no body was recovered for it, and there is no
        // evidence to reconstruct one from.
        void Destruct();

        // Draws the camera HUD debug overlay (camera X/Y/Z, FOV, lens length, look-at,
        // near clip). BLOCKED: reads dozens of BrnDirector::DirectorModule sub-object
        // fields through mpDirectorModule by raw byte offset (the +0x33BD0 / +0x33BD4 /
        // ... family) and DirectorModule's real layout is not yet modelled
        // (BrnDirectorModule.h is still constructor-only) -- named-member access is not
        // possible yet. Declaration-only per AGENTS.md (no raw offset-cast access).
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

        // Advances the panorama screenshot state machine (pitch/yaw step grid) each time
        // a panorama shot is requested. BLOCKED: heavy vector-unit quaternion/matrix math
        // whose coefficient tables have no recovered semantic names -- reproducing it
        // faithfully without fabricating the math is not possible. Declaration-only.
        void UpdatePanoramaScreenshots(Camera::Camera* lpCamera);

    protected:
        const char* GetName() const override;

        // Registers every camera/testbed/crash debug variable and action with the debug
        // menu. BLOCKED: reaches ~45 BrnDirector::DirectorModule sub-object fields through
        // mpDirectorModule by raw byte offset (same unmodelled-layout blocker as RenderHUD)
        // plus two still-todo callees (BehaviourParameterBank::Serialise<...> and the
        // unnamed behaviour-bank accessor helper). Declaration-only.
        void OnActivate() override;

    private:
        // Registered with the debug menu as DebugUI::Function::DebugCallbackFunction
        // (void(*)(void*)) callbacks (OnActivate: RegisterFunction(this, &SavePlaylists,
        // this, ...) / &StartEditor(..., this, ...) etc. -- the void* userData IS `this`),
        // so these four are STATIC: the single incoming parameter is the callback's void*
        // userData, not an implicit `this`, and reinterpret_cast<DebugComponent*>
        // (lpUserData) inside the body recovers the real `this`.
        //
        // Declared in the original header; registered as debug menu callbacks by OnActivate
        // (which is itself declaration-only -- see above). Bodies ARE recovered
        // (SavePlaylists / LoadPlaylists: fopen "d:\\playlists.txt", then
        // SharedPlaylists::Serialise<TextFileWriteSerialiser|TextFileReadSerialiser|
        // DebugMenuSerialiser> over the playlist store), but BOTH stay DECLARATION-ONLY
        // (BLOCKED): the SharedPlaylists object they serialise sits at DirectorModule
        // +0x13BD0, which lies inside BrnDirector::MainDirector's opaque
        // `alignas(16) u8 maStorage[0x35450]` placement region (DirectorModule +0xB00..
        // +0x35F50; BrnMainDirector.h). MainDirector exposes no SharedPlaylists member, so
        // reaching it would require a raw offset poke into another class's opaque storage --
        // forbidden by AGENTS.md's NO-RAW-OFFSET rule for in-memory engine objects.
        // LoadPlaylists additionally needs SharedPlaylists::Serialise<
        // Camera::DebugMenuSerialiser>, whose DebugMenuSerialiser visitor type has no
        // reconstructed home anywhere in the tree. (SharedPlaylists + its Serialise<
        // TextFileWriteSerialiser> template ARE now homed in BrnICEMoviePlayer.h -- the only
        // remaining blockers are the MainDirector-opaque container access and, for Load, the
        // un-homed DebugMenuSerialiser.)
        static void SavePlaylists(void* lpUserData);
        static void LoadPlaylists(void* lpUserData);

        // Arms the panorama screenshot request (resets the pitch/yaw step counters on the
        // rising edge; a no-op while already armed).
        static void TakePanorama(void* lpUserData);

        // Creates a new ICE take named "New Take" and hands it to the director's ICE
        // editor. BLOCKED: reaches the ICEAuthor/ICEWrapper sub-objects through
        // mpDirectorModule by raw byte offset (+0xB50 and +0xB50 +0x2750) -- same
        // unmodelled-DirectorModule-layout blocker as OnActivate/RenderHUD, even though
        // both callees (ICEAuthor::CreateNewTake, ICEWrapper::EditorOn) are now
        // reconstructed. Declaration-only.
        static void StartEditor(void* lpUserData);

        // Attested order (see the class-level FLAG comment above): mpDirectorModule is this
        // class's first owned member, at +0xC, immediately after the CgsDev::DebugComponent
        // base.
        DirectorModule* mpDirectorModule;

        // +0x10 / +0x11 / +0x12 (Construct's stores; the third is also read by
        // UpdatePanoramaScreenshots at +0x12).
        bool mbShowCameraPos;
        bool mbShowCrashShotInfo;
        bool mbTakePanoramaScreenshot;

        // +0x14 / +0x18 (UpdatePanoramaScreenshots' loads/stores).
        s32 miPanoramaStepPitch;
        s32 miPanoramaStepYaw;

        // Declared in the original header, public there, but not attested by any recovered
        // function of this TU -- see the class-level FLAG comment. Declared last so it
        // cannot silently shift the verified members above off their proven offsets.
        EActiveRaceCarIndex mePlayerCarIndexOverride;
    };
}
