#ifndef GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H
#define GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"  // Object::Acquire / Release

// =============================================================================
// CgsHandle.h  (OWNING HEADER for CgsSound::Playback::Handle<T>)
//
// Home for the sound-playback smart handle. The DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Sound/Playback/CgsHandle.h)
// attests the templated struct Handle<T> (T = Content / Factory / Voice /
// Environment / ...) with a single pointer member and a ref-counting accessor API:
//
//   CgsHandle.h:50   struct Handle<T> {
//   CgsHandle.h:187      T* mpObject;                  // the only member (+0)
//   CgsHandle.h:199      Handle(T*);
//   CgsHandle.h:209      Handle(const Handle&);
//   CgsHandle.h:217      ~Handle();
//   CgsHandle.h:226      Handle& operator=(const Handle&);
//   CgsHandle.h:251      bool operator!() const;
//   CgsHandle.h:259      operator bool() const;
//   CgsHandle.h:268      bool operator==(const Handle&) const;
//   CgsHandle.h:277      bool operator!=(const Handle&) const;
//   CgsHandle.h:285      T* operator->();
//   CgsHandle.h:294      const T* operator->() const;
//   CgsHandle.h:303      T& operator*();
//   CgsHandle.h:312      const T& operator*() const;
//   CgsHandle.h:321      void AcquireObject() const;   // private
//   CgsHandle.h:332      void ReleaseObject() const;   // private
//
// Attested bodies (each instantiation is emitted out of line per T):
//   operator=   -- take the other handle's reference, drop our own through
//                  Object::Release, then copy the pointer (Environment /
//                  GenericRwacFactory / SplicerFactory instantiations are called
//                  from the playback Module's Prepare and Release).
//   operator->  -- assert "mpObject" (CgsHandle.h) and return the pointer
//                  (Factory / Environment / AemsFactory instantiations).
//   operator*   -- assert "mpObject" (CgsHandle.h) and return the pointee
//                  (the AemsRWSampleFactory player-voice instantiation).
// The copy constructor and destructor still follow the interim ref model
// described beside them: the committed call sites pair Acquire/Release
// explicitly around adopt-constructed and destroyed handles.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsHandle.h:50. Ref-counting smart handle to a playback object of type T.
// sizeof == sizeof(T*) (a single pointer member at +0).
template <typename T>
struct Handle
{
public:
    // FLAG (additive grow): default ctor -- null-initialise the owned pointer.
    // The sound-playback Module ctor (0x827DFA98) embeds several handles and the
    // X360 simply stores 0 into each handle word (`stw r30, 0x2258(r31)` with
    // r30 == 0), i.e. the handle slot is value-initialised to a null mpObject with
    // NO Acquire. This trivial null-init ctor matches that store exactly and lets a
    // handle be a default-constructible aggregate member. Shape-only (the X360
    // never emits a distinct Handle() out-of-line -- it is the inlined zero store).
    Handle() : mpObject(0) {}

    // CgsHandle.h:199 -- adopt a raw pointer. Inline plain store (phase B5): every
    // committed call site either passes null (the GetVoice/GetR/CreateContent empty
    // handles) or takes its reference explicitly beside the store (the Acquire the
    // :199 ctor's ref-counting implies is applied manually in those bodies) --
    // consistent with the empty inline dtor below. Revisit with the handle slice.
    explicit Handle(T* lpObject) : mpObject(lpObject) {}

    // CgsHandle.h -- copy (acquires the copied object). Declared-only: the
    // build never odr-uses it (handle returns are elided).
    Handle(const Handle& lkrOther);
    // CgsHandle.h -- the console destructor drops the owned reference
    // (ReleaseObject). Here it is empty because the committed call sites pair
    // Acquire/Release explicitly around adopt-constructed and destroyed handles
    // (e.g. Logic::Voice's mVoiceHandle, the playback Module's create temps);
    // moving it onto ReleaseObject means moving those sites with it.
    ~Handle() {}

    // CgsHandle.h -- assign. The other handle's object gains a reference
    // first (Object +4 count), then our current object is released, then the
    // pointer is copied; acquiring before releasing keeps self-assignment safe.
    Handle& operator=(const Handle& lkrOther)
    {
        lkrOther.AcquireObject();
        ReleaseObject();
        mpObject = lkrOther.mpObject;
        return *this;
    }

    // CgsHandle.h:251 / 259. Validity -- the owned-pointer test (the X360
    // inlines the null compare at every site). Made header-inline phase B5.
    bool operator!() const     { return mpObject == 0; }
    operator bool() const      { return mpObject != 0; }

    // CgsHandle.h:268 / 277. Equality. Declared-only.
    bool operator==(const Handle& lkrOther) const;
    bool operator!=(const Handle& lkrOther) const;

    // CgsHandle.h / 294. Member-access: assert "mpObject" (the non-const
    // form fires at CgsHandle.h) and return the pointer.
    T* operator->()
    {
        CGS_ASSERT(mpObject, "mpObject");
        return mpObject;
    }
    const T* operator->() const
    {
        CGS_ASSERT(mpObject, "mpObject");
        return mpObject;
    }

    // CgsHandle.h. Dereference: assert "mpObject" (fires CgsHandle.h)
    // and return *mpObject.
    T& operator*()
    {
        CGS_ASSERT(mpObject, "mpObject");
        return *mpObject;
    }

    // CgsHandle.h:312. const dereference -- same guard + return. Declared-only
    // beyond this inline mirror (no distinct address attested), kept inline so the
    // const-correct form is usable.
    const T& operator*() const
    {
        CGS_ASSERT(mpObject, "mpObject");
        return *mpObject;
    }

    // FLAG (additive grow, by-name): raw owned-pointer accessor. The sound-LOGIC
    // wrappers (e.g. CgsSound::Logic::Voice) read the handle's owned pointer
    // DIRECTLY as `*(this+4)` on X360 -- both to null-check ("Voice not yet
    // created!") WITHOUT dereferencing and to step its ref count / reach into the
    // Playback::Voice fields. This getter exposes mpObject BY NAME so dependents
    // don't reach past the access boundary; it matches the raw `lwz r11, 4(r3)`
    // load and adds no layout or behaviour. No own X360 address (the load is
    // inlined at every call site).
    T* GetObject() const { return mpObject; }

    // FLAG (additive grow, by-name): raw owned-pointer SETTER. The sound-playback
    // Module out-params write the freshly created object straight into the caller's
    // handle word -- the X360 `stw r22, 0(r25)` store of the new Voice* into the
    // out-handle's mpObject (no Acquire; the ref was already taken by CreateVoice).
    // This setter exposes that store BY NAME; it matches the raw store and adds no
    // layout or behaviour. No own X360 address (inlined at the store site).
    void SetObject(T* lpObject) { mpObject = lpObject; }

private:
    // CgsHandle.h / 332. Ref-counting helpers: a null handle owns nothing;
    // otherwise bump the object's count / drop it through Object::Release.
    void AcquireObject() const
    {
        if (mpObject)
            mpObject->Acquire();
    }
    void ReleaseObject() const
    {
        if (mpObject)
            mpObject->Release();
    }

    // CgsHandle.h:187. The owned object (+0). The dereference guard reads it as
    // `*a1` in the X360 asm (lwz r11, 0(r31)).
    T* mpObject;
};

} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H
