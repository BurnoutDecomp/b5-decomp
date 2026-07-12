#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (typedef rw::math::vpu::Vector3) -- the vec3 Serialise overload

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h
//
// BrnDirector::Camera::DebugMenuSerialiser -- the serialiser "visitor" that mirrors a
// camera-tunings/playlist Parameters tree INTO the in-game debug menu (add) or removes it
// (remove). It walks the same Serialise<T> visitor protocol as the text-file serialisers,
// but instead of reading/writing a file it registers each leaf field as a debug-menu
// variable on a BrnDirector::DebugComponent (RegisterVariable / SetStep / ...), building the
// menu path as it descends.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../Serialisation.h:221):
//       struct DebugMenuSerialiser {
//           static const s32 KI_CHARBUFFERLENGTH = 64;   // :224
//           enum EMode { E_MODE_ADD_TO_MENU=0, E_MODE_REMOVE_FROM_MENU=1 };  // :226
//       private:
//           static const s32            KI_PATHSTACKSIZE = 64;      // :448
//           EMode                       meMode;                     // :450  +0x00
//           u32                         muStackPos;                 // :451  +0x04
//           char                        macPathStack[64];           // :452  +0x08
//           BrnDirector::DebugComponent* mpDebugComponent;          // :454  +0x48
//       };
// The mpDebugComponent offset (+0x48) is X360-attested: DebugMenuSerialiser::Serialise
// (@0x82219A50) loads it with `lwz r3, 0x48(r31)` before every SetStep call, which pins
// meMode(+0x00)/muStackPos(+0x04)/macPathStack[64](+0x08..+0x47)/mpDebugComponent(+0x48).
//
// HOME for the class slice owned by this TU:
//   - DebugMenuSerialiser::Serialise(const char*, Vector3&) @0x82219A50 (body in the .cpp).
// The other visitor overloads (scalar/VersionNumber/FOV/playlist), Construct, AddToPath,
// ProcessFunction and the Process<T> worker are separate TUs; the ones this slice can spell
// with already-reconstructed parameter types are declared here (declaration-only) so callers
// can drive them by name. The VersionNumber/FOV/SharedPlaylists/ICEMoviePlaylist overloads are
// intentionally NOT declared in this minimal slice (they would pull in types this TU does not
// need); add them when the DebugMenuSerialiser's own TU lands.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // mpDebugComponent's target + the SetStep receiver. Pointer member here, so a forward
    // declaration suffices for the header; the .cpp includes the full definition
    // (BrnDirectorModuleDebugCompononent.h) to call the inherited CgsDev::DebugComponent::SetStep.
    class DebugComponent;

namespace Camera
{

class DebugMenuSerialiser
{
public:
    // Fixed label-buffer length (Serialisation.h:224).
    static const s32 KI_CHARBUFFERLENGTH = 64;

    // Serialisation.h:226. Whether this pass installs the fields into the debug menu or
    // strips them back out (LoadPlaylists rebuilds the menu with REMOVE then ADD around the
    // file read).
    enum EMode
    {
        E_MODE_ADD_TO_MENU      = 0,
        E_MODE_REMOVE_FROM_MENU = 1,
    };

    // Serialisation.h:235. Bind the target debug component and the add/remove mode; resets the
    // path stack (muStackPos = 0, macPathStack[0] = 0). Body is a separate TU; declared here so
    // callers (e.g. the inlined DebugComponent::LoadPlaylists/SavePlaylists) drive it by name.
    void Construct(BrnDirector::DebugComponent* lpDebugComponent, EMode leMode);

    // The per-leaf worker (X360 public member template `void Process<T>(const char*, T&)`,
    // Serialisation.h:265). For the current EMode it either registers the field as a debug-menu
    // variable under the current path or removes it. One instantiation per leaf type; the bodies
    // are separate TUs (attested by the `bl Process<float>` call in Serialise). Declared here so
    // the vec3 Serialise below can drive Process<float> by name.
    template<class T> void Process(const char* lpcName, T& lrValue);

    // Scalar leaf visitors -- the debug-menu counterparts of the text serialisers' scalar
    // overloads. Declaration-only in this slice (bodies are separate TUs); declared so the
    // Parameters::Serialise<DebugMenuSerialiser> walkers drive them by name.
    void Serialise(const char* lpcName, f32& lrValue);
    void Serialise(const char* lpcName, s32& lrValue);
    void Serialise(const char* lpcName, u32& lrValue);
    void Serialise(const char* lpcName, bool& lrValue);

    // The vec3 leaf visitor -- DEFINED in BrnDebugMenuSerialiser.cpp @0x82219A50. Registers each
    // component (x/y/z) as a debug-menu float (Process<float>) with a 0.01 adjust step (SetStep).
    void Serialise(const char* lpcName, Vector3& lrValue);

private:
    // Path-stack capacity (Serialisation.h:448).
    static const s32 KI_PATHSTACKSIZE = 64;

    EMode                        meMode;                          // +0x00  add vs remove
    u32                          muStackPos;                      // +0x04  current path length
    char                         macPathStack[KI_PATHSTACKSIZE];  // +0x08  menu-path scratch
    BrnDirector::DebugComponent* mpDebugComponent;                // +0x48  menu the fields land in
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_DEBUG_MENU_SERIALISER_H
