#ifndef CGS_SOUND_PLAYBACK_CGSOBJECT_H
#define CGS_SOUND_PLAYBACK_CGSOBJECT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSound::Playback::Object - the reference-counted base for sound-playback
// objects (the canonical DWARF home is CgsObject.h:42).
//
// FLAG (committed-type duplication): a SIMPLIFIED copy of this same class already
// lives in CgsSound/Playback/CgsContent.h (struct Object { vptr; u32 mu32RefCount;
// virtual ~Object(); virtual DoDispose(); }) where it is the base of
// CgsContent.h's `Content`. That copy was committed before this real home existed.
// The two share an IDENTICAL X360 layout (vptr @ +0, mu32RefCount @ +4) and an
// identical destructor assert ("0 == mu32RefCount"), so they are ODR-compatible
// member-for-member. This file is the DWARF-correct home and ADDS Acquire/Release
// (CgsObject.h:108/115) which the CgsContent.h copy omits. CONDUCTOR: fold
// CgsContent.h's local Object onto this header (have CgsContent.h #include this
// and drop its private copy) so there is one Object. Not done here -- CgsContent.h
// is another group's surface and no single TU in this group includes both.
namespace CgsSound
{
namespace Playback
{

// CgsObject.h:42. Reference-counted playback-object base.
class Object
{
public:
    // CgsObject.h:94. Construct with a zero reference count.
    Object() : mu32RefCount(0) {}

    // CgsObject.h:102. Virtual destructor. The X360 vector-deleting-destructor at
    // 0x826916F0 (a) installs the Object vtable, (b) asserts the object is no
    // longer referenced, then (c) optionally frees the storage. The non-deleting
    // ~Object() body is just the assert; the install + free are emitted by the
    // compiler's destructor thunk. Defined out-of-line in CgsObject.cpp so the
    // assert (CgsObject.h:104) is bodied store-for-store against the X360.
    virtual ~Object();

    // CgsObject.h:108. Take a reference. FLAG: shape-only -- Acquire is not in
    // this TU's function set (no recon'd asm), modelled as the obvious ++ so
    // dependents can name it; any guard assert is unverified and omitted.
    void Acquire() { ++mu32RefCount; }

    // CgsObject.h:115. Drop a reference. FLAG: shape-only (see Acquire); no
    // fabricated precondition assert.
    void Release() { --mu32RefCount; }

    // CgsObject.h:82. Subsystem dispose hook (overridden by Content & friends).
    virtual void DoDispose() {}

protected:
    // CgsObject.h:85. Outstanding reference count. The destructor at 0x826916F0
    // reads this (a1[1] on X360, i.e. the word right after the 4-byte vptr) and
    // asserts it is zero before teardown.
    u32 mu32RefCount;
};

}
}

#endif // CGS_SOUND_PLAYBACK_CGSOBJECT_H
