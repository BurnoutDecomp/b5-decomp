#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (the vec3 Serialise overload)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT (ProcessFunction's unhandled-case arm)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Functions/CgsFunction.h" // DebugUI::Function::DebugCallbackFunction
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"    // complete BrnDirector::DebugComponent (the inline leaf bodies call it)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                            // Utils::VersionNumber / Utils::FOV (inline leaf bodies)

#include <cstring>   // strlen (RemoveFromPath)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h
//
// BrnDirector::Camera::DebugMenuSerialiser -- the serialiser "visitor" that mirrors a
// camera-tunings/playlist Parameters tree INTO the in-game debug menu (add) or removes it
// (remove). It walks the same Serialise<T> visitor protocol as the text-file serialisers,
// but instead of reading/writing a file it registers each leaf field as a debug-menu
// variable on a BrnDirector::DebugComponent, building the menu path as it descends.
//
// The console declares all five camera serialisers in one header,
// Camera/Behaviours/Serialisation.h (this tree splits the three file/menu ones into their own
// headers; Serialisation.h includes all three and adds the testbed and naming serialisers).
//
// Layout (console member order and offsets):
//     EMode                         meMode;              // +0x00
//     u32                           muStackPos;          // +0x04
//     char                          macPathStack[64];    // +0x08
//     BrnDirector::DebugComponent*  mpDebugComponent;    // +0x48
//
// The leaf overloads the console always inlines into the Parameters walkers (scalar, version,
// FOV, Construct, ProcessFunction, RemoveFromPath and the nested-block template) are inline
// here; AddToPath, the vec3 overload and the four Process<T> instances are out-of-line in
// BrnDebugMenuSerialiser.cpp, as on the console. The SharedPlaylists / ICEMoviePlaylist
// overloads are bodied with the playlist serialisers (Utils/BrnICEMoviePlayerSerialise.cpp).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    struct SharedPlaylists;    // Utils/BrnICEMoviePlayer.h
    struct ICEMoviePlaylist;   // Utils/BrnICEMoviePlayer.h

namespace Camera
{

class DebugMenuSerialiser
{
public:
    // Fixed label-buffer length.
    static const s32 KI_CHARBUFFERLENGTH = 64;

    // Whether this pass installs the fields into the debug menu or strips them back out.
    enum EMode
    {
        E_MODE_ADD_TO_MENU      = 0,
        E_MODE_REMOVE_FROM_MENU = 1,
    };

    // Bind the target debug component and the add/remove mode, and empty the path stack.
    // Always inlined (ArbStateTestbed::RegisterParameters builds the serialiser on its stack
    // with exactly these four stores).
    void Construct(BrnDirector::DebugComponent* lpDebugComponent, EMode leMode)
    {
        mpDebugComponent = lpDebugComponent;
        meMode           = leMode;
        muStackPos       = 0;
        macPathStack[0]  = '\0';
    }

    // Push "<lpacPath>/" onto the menu path stack. Out-of-line (BrnDebugMenuSerialiser.cpp).
    void AddToPath(const char* lpacPath);

    // Register (ADD) or unregister (REMOVE) a debug-menu action under the current path. Always
    // inlined (the playlist debug-menu builder's "New Movie" / "Remove Movie" entries).
    void ProcessFunction(CgsDev::DebugUI::Function::DebugCallbackFunction lpfFunction,
                         void* lpUserData, const char* lpcName)
    {
        if (meMode == E_MODE_ADD_TO_MENU)
            mpDebugComponent->RegisterFunction(lpfFunction, lpUserData, macPathStack, lpcName);
        else if (meMode == E_MODE_REMOVE_FROM_MENU)
            mpDebugComponent->UnregisterFunction(lpfFunction, lpUserData);
        else
            CGS_ASSERT(false, "unhandled case");
    }

    // ---- leaf visitors ----------------------------------------------------------------
    // f32: a menu float with a 0.01 adjust step.
    void Serialise(const char* lpcName, f32& lrValue)
    {
        Process<float>(lpcName, lrValue);
        mpDebugComponent->SetStep(&lrValue, KF_DEBUG_ADJUST_STEP);
    }

    // s32 / u32 / bool: the plain menu variable, no step or range.
    void Serialise(const char* lpcName, s32& lrValue)  { Process<int>(lpcName, lrValue); }
    void Serialise(const char* lpcName, u32& lrValue)  { Process<unsigned int>(lpcName, lrValue); }
    void Serialise(const char* lpcName, bool& lrValue) { Process<bool>(lpcName, lrValue); }

    // The block's leading version tag is not an editable menu variable: the debug-menu walkers
    // store the current version into the tag and emit nothing for it.
    void Serialise(const char* /*lpcName*/, Utils::VersionNumber& /*lrValue*/) {}

    // FOV: a menu float clamped to [1, 150] degrees.
    void Serialise(const char* lpcName, Utils::FOV& lrValue)
    {
        Process<float>(lpcName, lrValue.mfFOV);
        mpDebugComponent->SetRange(&lrValue.mfFOV, KF_FOV_MIN_DEGS, KF_FOV_MAX_DEGS);
    }

    // The vec3 leaf: x/y/z each as a menu float with the 0.01 step. Out-of-line
    // (BrnDebugMenuSerialiser.cpp).
    void Serialise(const char* lpcName, Vector3& lrValue);

    // The playlist overloads. Bodies live with the playlist serialisers
    // (Utils/BrnICEMoviePlayerSerialise.cpp).
    void Serialise(const char* lpcName, SharedPlaylists& lrPlaylists);
    void Serialise(const char* lpcName, ICEMoviePlaylist& lrPlaylist);

    // The nested-block visitor: push the section name, walk T's own fields one level deeper,
    // pop the name again. One shared body; the console emits it per T.
    template<class T>
    void Serialise(const char* lpcName, T& lrParams)
    {
        AddToPath(lpcName);
        lrParams.Serialise(*this);
        RemoveFromPath(lpcName);
    }

    // The per-leaf worker: ADD registers &lrValue as a menu variable under the current path,
    // REMOVE strips it back out. Body + the four explicit instances (float / int / unsigned int
    // / bool) in BrnDebugMenuSerialiser.cpp.
    template<class T> void Process(const char* lpcName, T& lrValue);

private:
    // Path-stack capacity.
    static const s32 KI_PATHSTACKSIZE = 64;

    // The per-component adjust step and the FOV menu range.
    static constexpr f32 KF_DEBUG_ADJUST_STEP = 0.01f;
    static constexpr f32 KF_FOV_MIN_DEGS      = 1.0f;
    static constexpr f32 KF_FOV_MAX_DEGS      = 150.0f;

    // Pop "<lpcPath>/" back off the path stack. Always inlined into the nested-block visitor.
    void RemoveFromPath(const char* lpcPath)
    {
        muStackPos -= static_cast<u32>(strlen(lpcPath)) + 1;
        macPathStack[muStackPos] = '\0';
    }

    EMode                        meMode;                          // +0x00  add vs remove
    u32                          muStackPos;                      // +0x04  current path length
    char                         macPathStack[KI_PATHSTACKSIZE];  // +0x08  menu-path scratch
    BrnDirector::DebugComponent* mpDebugComponent;                // +0x48  menu the fields land in
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H
