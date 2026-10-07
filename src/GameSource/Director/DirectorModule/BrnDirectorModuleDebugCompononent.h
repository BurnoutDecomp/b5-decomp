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
        // near clip) from MainDirector::CameraDebugInfo. Bodied in
        // BrnDirectorModuleDebugCompononent_wS34_00.cpp.
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

        // Advances the panorama screenshot state machine (pitch/yaw step grid) each time
        // a panorama shot is requested. Bodied in BrnDirectorModuleDebugCompononent_wS34_00.cpp.
        void UpdatePanoramaScreenshots(Camera::Camera* lpCamera);

    protected:
        const char* GetName() const override;

        // Registers every camera/testbed/crash debug variable and action with the debug
        // menu. BLOCKED on the director dev-tools closure (the parameter-bank and playlist
        // serialisers, the testbed state, the ICE editor entry); the exact list is on its
        // stub in DirectorLinkStubs.cpp.
        void OnActivate() override;

    private:
        // Registered with the debug menu as DebugUI::Function::DebugCallbackFunction
        // (void(*)(void*)) callbacks (OnActivate: RegisterFunction(this, &SavePlaylists,
        // this, ...) / &StartEditor(..., this, ...) etc. -- the void* userData IS `this`),
        // so these four are STATIC: the single incoming parameter is the callback's void*
        // userData, not an implicit `this`, and reinterpret_cast<DebugComponent*>
        // (lpUserData) inside the body recovers the real `this`.
        //
        // Declared in the original header; registered as debug menu callbacks by OnActivate.
        // SavePlaylists / LoadPlaylists fopen "d:\\playlists.txt" and run SharedPlaylists::
        // Serialise<TextFileWriteSerialiser | TextFileReadSerialiser | DebugMenuSerialiser>
        // over the arbitrator state container's mSharedPlaylists (DirectorModule +0x13BD0).
        // BLOCKED: those Serialise instantiations live in the unmounted
        // Utils/BrnICEMoviePlayerSerialise.cpp, whose closure (the DebugMenuSerialiser and
        // TextFile*Serialiser scalar overloads, ICEMoviePlaylist::Serialise<S>) is not in the
        // link. Declaration-only.
        static void SavePlaylists(void* lpUserData);
        static void LoadPlaylists(void* lpUserData);

        // Arms the panorama screenshot request (resets the pitch/yaw step counters on the
        // rising edge; a no-op while already armed).
        static void TakePanorama(void* lpUserData);

        // Creates a new ICE take named "New Take" (ICEAuthor::CreateNewTake on the director's
        // ICEWrapper author, guid -1) and hands it to ICEWrapper::EditorOn. Both are
        // reachable by name through MainDirector::GetICEWrapper(). BLOCKED: EditorOn lives
        // in the unmounted BrnDirectorICEWrapper.cpp. Declaration-only.
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
