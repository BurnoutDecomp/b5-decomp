#ifndef CGS_SOUND_PLAYBACK_CGSOBJECT_H
#define CGS_SOUND_PLAYBACK_CGSOBJECT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// CgsSound::Playback::Object - the reference-counted base for sound-playback
// objects (the canonical DWARF home is CgsObject.h:42).
//
// FOLD DONE (2026-08-25, audio-faithfulness wave 3): CgsContent.h's old simplified
// private copy of this class is DELETED -- CgsContent.h now #includes this header,
// so there is exactly ONE CgsSound::Playback::Object program-wide (this DWARF
// home). NOTE: the out-of-line asserting ~Object @0x826916F0 lives in
// CgsObject.cpp -- any link that pulls a CgsContent.h consumer needs that TU
// mounted.
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

    // CgsObject.h:102. Virtual destructor: asserts the object is no longer referenced.
    // Its assert names CgsObject.h:104, so the body is this header's inline one; ARTIST
    // inlines it into every derived destructor (Factory's deleting destructor 0x826AD3C8
    // asserts at line 104 itself, with no call) and emits the Object deleting destructor
    // (0x826916F0) as a COMDAT.
    virtual ~Object()
    {
        CGS_ASSERT_AT(mu32RefCount == 0, "0 == mu32RefCount",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsObject.h", 104);
    }

    // CgsObject.h:108. Take a reference. FLAG: shape-only -- Acquire is not in
    // this TU's function set (no recon'd asm), modelled as the obvious ++ so
    // dependents can name it; any guard assert is unverified and omitted.
    void Acquire() { ++mu32RefCount; }

    // CgsObject.h:115 (DWARF: defined in this header). Assert a live reference, drop it,
    // and dispose the object through the virtual hook when the count reaches zero. ARTIST
    // calls its COMDAT copy 0x82680938 from 54 sites and expands it inside the content-pool
    // destructor's element loop (~ObjectPool<RegisteredContent,4,int> 0x826EAC90, assert
    // line 117 at 0x826EACE4).
    void Release()
    {
        CGS_ASSERT_AT(mu32RefCount > 0, "mu32RefCount > 0",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsObject.h", 117);

        --mu32RefCount;
        if (mu32RefCount == 0)
            DoDispose();
    }

    // CgsObject.h:82. Subsystem dispose hook (overridden by Content & friends).
    virtual void DoDispose() {}

    // FLAG (additive grow to a committed shared home): const reference-count
    // accessor added for the StateManager::RegisteredContent slot teardown
    // (0x826EAC90), which reads mu32RefCount to (a) assert it is > 0 and (b)
    // detect the drop-to-zero before disposing. The X360 reads the raw word
    // a1[1]; this read-only getter exposes the same value by NAME without
    // changing layout or any existing behaviour.
    u32 GetRefCount() const { return mu32RefCount; }

protected:
    // CgsObject.h:85. Outstanding reference count. The destructor at 0x826916F0
    // reads this (a1[1] on X360, i.e. the word right after the 4-byte vptr) and
    // asserts it is zero before teardown.
    u32 mu32RefCount;
};

}
}

#endif // CGS_SOUND_PLAYBACK_CGSOBJECT_H
