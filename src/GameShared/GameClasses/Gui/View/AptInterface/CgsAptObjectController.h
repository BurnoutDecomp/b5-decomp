#ifndef CGS_APT_OBJECT_CONTROLLER_H
#define CGS_APT_OBJECT_CONTROLLER_H

#include "types.hpp"
#include "SDKs/EATech/include/Apt/AptExtObject.h"   // AptExtObject base (and AptValue)

// Pointer-only collaborator (only a pointer is stored / passed through; including the
// full GuiComponent header is not required to compile this surface, and forward-
// declaring it avoids a transitive GUI-state header cascade):
namespace CgsGui { struct GuiComponent; }

// ============================================================================
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h
//
// CgsGui::ObjectController - the apt (Scaleform-style) display-object control
// bridge. It wraps an apt display object (an AptExtObject the apt VM registers
// as an extension) so game code can register/unregister it and get/set its
// ActionScript script variables by key. The controlled GuiComponent supplies the
// object's name; the apt-side reference is the AptValue the apt communicator
// resolves the component to.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member set / order / names and the
// embedded ObjectInfo struct are taken from the DecFIGS DWARF (a high-confidence
// near-ancestor of ARTIST: CgsAptObjectController.h); the X360 word/byte offsets
// confirm each member's displacement past the AptExtObject base:
//
//   AptExtObject base               +0x00 .. +0x0F  (vtable + mpNativeHash + mnObjectSize)
//   mbRegistered      [+0x10]  bool        - byte flag (X360 `*(this+16)`, lbz/stb)
//   mpControlledObject[+0x14]  GuiComponent* - X360 `*(this+20)`
//   mpComponentReference[+0x18] AptValue*   - X360 `*(this+24)`
//
// The X360 build (Jan-2008) attests only the subset of the DWARF method surface
// actually emitted; this header models the full DWARF-declared member layout so
// the sibling Animator (which embeds an ObjectController* and calls
// Register/UnRegister/SetControlledObject/GetObjectVariableFloat) has a complete
// type, while only the methods this TU bodies (plus the inline trivial accessors
// the X360 attests) are declared with bodies elsewhere.
//
// x64 note: members are accessed BY NAME. The AptExtObject base widens under the
// host (x64) ABI so the absolute byte offsets differ from the X360's - that is
// expected and fine (semantic parity by named members, not byte offsets).
// ============================================================================

namespace CgsGui
{
    struct ObjectController : public AptExtObject
    {
        // CgsAptObjectController.h:41 (DWARF) - the snapshot of an apt display
        // object's transform/visibility state GetObjectInfo fills in.
        struct ObjectInfo
        {
            f32  mfPosX;        // CgsAptObjectController.h:43
            f32  mfPosY;        // CgsAptObjectController.h:44
            f32  mfWidth;       // CgsAptObjectController.h:45
            f32  mfHeight;      // CgsAptObjectController.h:46
            f32  mfAlpha;       // CgsAptObjectController.h:47
            f32  mfRotation;    // CgsAptObjectController.h:48
            bool mbVisible;     // CgsAptObjectController.h:49
        };

        // CgsAptObjectController.h:216 (DWARF) - default the controller into the
        // un-registered, un-attached state. Header-inline in the X360 build (its body is
        // folded into Animator::Construct's `new ObjectController`): construct the
        // AptExtObject base reserving no native members, then null the three fields.
        ObjectController()
            : AptExtObject(0)
            , mbRegistered(false)
            , mpControlledObject(0)
            , mpComponentReference(0)
        {
        }

        // CgsAptObjectController.cpp:38 - clears the controller state then runs the
        // AptExtObject base destructor. Virtual (overrides AptExtObject's ~dtor).
        virtual ~ObjectController();

        // CgsAptObjectController.cpp:102 - return the controlled GuiComponent's name
        // (asserts a controlled object has been attached). Virtual: overrides
        // AptExtObject::GetName (the apt VM queries the extension's name through it).
        virtual const char* GetName();

        // CgsAptObjectController.cpp:65 - register this object as an apt extension so
        // the apt VM exposes its script variables. Asserts a controlled object is set
        // and that we are not already registered.
        void Register();

        // CgsAptObjectController.cpp:82 - tear down the apt-extension registration.
        // Asserts a controlled object is set and that we are registered.
        void UnRegister();

        // CgsAptObjectController.cpp:117 - bind the apt-side component reference (the
        // AptValue the communicator resolved the component to). Asserts it is a valid,
        // on-stage display-object reference.
        void AttachController(AptValue* lpComponentReference);

        // CgsAptObjectController.h:233 (DWARF) - set the controlled GuiComponent. The
        // X360 declares it header-inline; it is the trivial store the apt control bridge
        // uses when the controlled component is bound at Construct time.
        void SetControlledObject(GuiComponent* lpControlledObject)
        {
            mpControlledObject = lpControlledObject;
        }

        // CgsAptObjectController.h:269 (DWARF) - read the named ActionScript float
        // variable from the bound reference. Bodied by its own ledger TU; declared here
        // for the surface CgsGui::Animator drives.
        f32 GetObjectVariableFloat(const char* lpacVariable);

        // CgsAptObjectController.cpp:136 - look the named ActionScript variable up on
        // the bound apt reference and return its AptValue (or null). Asserts the key,
        // that the controller is set up, and that the component is on the stage.
        AptValue* GetObjectValue(const char* lpacKey);

        // CgsAptObjectController.cpp:192 - set the named ActionScript boolean variable
        // on the bound apt reference. Asserts the variable info, that the controller is
        // set up, and that the reference is valid.
        void SetObjectVariableBoolean(const char* lpacVariable, bool lbValue);

    private:
        // CgsAptObjectController.h:199 (DWARF) - apt extension hash-table sizing.
        static const s32 KI_NUM_HASHTABLE_ENTRIES;

        bool          mbRegistered;          // [+0x10] CgsAptObjectController.h:201
        GuiComponent* mpControlledObject;    // [+0x14] CgsAptObjectController.h:202
        AptValue*     mpComponentReference;  // [+0x18] CgsAptObjectController.h:203
    };
}

#endif // CGS_APT_OBJECT_CONTROLLER_H
