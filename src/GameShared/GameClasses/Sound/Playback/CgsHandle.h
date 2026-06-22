#ifndef GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H
#define GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
// The only attested function in this dossier is the dereference for the Factory
// instantiation:
//   CgsSound::Playback::Handle<CgsSound::Playback::Factory>::operator*  @ 0x8268E178
// whose X360 body is:
//
//   if (!*this->mpObject-cell)  // i.e. mpObject == 0
//       CGS_ASSERT(mpObject, "mpObject")  // fires CgsHandle.h:287
//   return *mpObject;
//
// (Hex-Rays renders mpObject as `*a1` and the load returns it.) The FireAssert
// line is 0x11F == 287 -- the assert macro site that the inlined dereference guard
// expands at; operator* itself is declared at line 303. The assert message string
// is exactly "mpObject".
//
// operator* is template-inline (no separate out-of-line address per instantiation
// beyond the one the linker keeps), so it is bodied here in the header. The
// remaining API is declared-only -- those are other-TU surface (ctors/dtor do the
// Acquire/Release ref-counting, not attested here). FLAG: MINIMAL FLAGGED HOME;
// grow additively as the ctor/assign/acquire TUs land.
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
    // CgsHandle.h:199 / 209 / 217 / 226 -- ctor/copy/dtor/assign do the
    // Acquire/Release ref-counting. Declared-only (other-TU surface).
    explicit Handle(T* lpObject);
    Handle(const Handle& lkrOther);
    ~Handle();
    Handle& operator=(const Handle& lkrOther);

    // CgsHandle.h:251 / 259 / 268 / 277. Validity / equality. Declared-only.
    bool operator!() const;
    operator bool() const;
    bool operator==(const Handle& lkrOther) const;
    bool operator!=(const Handle& lkrOther) const;

    // CgsHandle.h:285 / 294. Member-access. Declared-only.
    T*       operator->();
    const T* operator->() const;

    // CgsHandle.h:303. Dereference. @ 0x8268E178 for T = Factory. Asserts that
    // mpObject is non-null (fires CgsHandle.h:287) then returns *mpObject.
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

private:
    // CgsHandle.h:321 / 332. Ref-counting helpers. Declared-only.
    void AcquireObject() const;
    void ReleaseObject() const;

    // CgsHandle.h:187. The owned object (+0). The dereference guard reads it as
    // `*a1` in the X360 asm (lwz r11, 0(r31)).
    T* mpObject;
};

} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_CGSHANDLE_H
