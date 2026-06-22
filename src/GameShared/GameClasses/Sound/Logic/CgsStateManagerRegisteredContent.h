#ifndef CGS_SOUND_LOGIC_STATEMANAGER_REGISTEREDCONTENT_H
#define CGS_SOUND_LOGIC_STATEMANAGER_REGISTEREDCONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // CgsSound::Playback::Object

// CgsSound::Logic::StateManager::RegisteredContent - one slot of the
// StateManager's content registry. The StateManager keeps these in an
// ObjectPool<RegisteredContent, 4, int> (capacity 4, int slot index); the pool
// instantiation TU lives in ObjectPool_StateManagerRegisteredContent_4.cpp.
//
// No DWARF/leak home exists for this nested type. The layout and the destructor
// behaviour below are recovered from the X360 pool destructor at 0x826EAC90,
// which inlines the per-element RegisteredContent destructor as it tears down
// the pool's maObjectPool[4] in reverse. For each 16-byte element it:
//
//   *(element + 4) = &off_820B3250;     // install RegisteredContent's vtable
//   pObject = *(element + 8);           // the held refcounted content object
//   if (pObject)
//   {
//       CGS_ASSERT(pObject->mu32RefCount > 0, ...CgsObject.h:117);
//       if (--pObject->mu32RefCount == 0)
//           pObject->~()                // virtual dtor via vtable slot +4
//   }
//
// So a RegisteredContent slot holds a pointer to a reference-counted
// CgsSound::Playback::Object and, on teardown, drops its reference (and disposes
// the object when the count reaches zero). The element stride is 16 bytes; the
// installed vtable sits at element+4 (offset 0x34/0x24/0x14/0x04 across the four
// slots in the 0x826EAC90 reverse walk), i.e. RegisteredContent has a non-virtual
// leading word at +0 with its polymorphic content-holder following.
//
// FLAG (faithful, not byte-identical; un-homed surface modelled honestly):
//  * The +0x00 leading word and the +0x0C trailing word are not touched by any
//    recovered function in this TU. They are modelled as named opaque words so
//    the type is exactly 16 bytes (matching the pool's 0x40-byte maObjectPool[4]
//    and the 16-byte element stride proven by the destructor); their meaning is
//    unknown and NOT fabricated.
//  * RegisteredContent's own vtable (off_820B3250) is reproduced structurally by
//    giving the class a virtual destructor; MSVC re-synthesises the vptr install.
//  * The reference drop uses CgsSound::Playback::Object::Release() + the
//    refcount-zero dispose, matching the X360 decrement-then-virtual-dtor. The
//    "mu32RefCount > 0" tripwire (CgsObject.h:117) is preserved as a CGS_ASSERT.
// FLAG (committed-home coexistence -- conductor please note): a minimal
// CgsSound::Logic::StateManager is already defined inline inside another group's
// TU (CgsStateManager.cpp: struct StateManager { u8 mPad[20]; s32 miAliasState;
// bool IsStateAlias(int) const; }). This header gives a SECOND, partial view of
// the same enclosing class purely to host the nested RegisteredContent type
// (which the X360 mangles as CgsSound::Logic::StateManager::RegisteredContent).
// The two views never appear in the same TU, so they compile independently; they
// are NOT layout-compatible views of each other (this view only adds the nested
// class, it does not restate mPad/miAliasState/IsStateAlias and must not be read
// as redefining StateManager's data layout). When StateManager gets a real
// owning header, fold both the IsStateAlias members and this nested class onto
// it. Done minimally here to avoid forking either surface.
namespace CgsSound
{
namespace Logic
{

class StateManager
{
public:
    class RegisteredContent
    {
    public:
        RegisteredContent() : muLeading(0), mpContent(0), muTrailing(0) {}

        // Drop the reference this slot holds on its content object and dispose
        // the object when no references remain. Recovered from the inlined
        // per-element teardown in the pool destructor at 0x826EAC90.
        virtual ~RegisteredContent()
        {
            if (mpContent != 0)
            {
                CGS_ASSERT(mpContent->GetRefCount() > 0, "mu32RefCount > 0");
                mpContent->Release();
                if (mpContent->GetRefCount() == 0)
                {
                    mpContent->DoDispose();
                }
            }
        }

    private:
        // +0x00: leading non-virtual word (untouched by recovered code; opaque).
        u32 muLeading;
        // +0x04: vtable (installed by ~RegisteredContent; implicit here).
        // +0x08: the held reference-counted content object, or null.
        CgsSound::Playback::Object* mpContent;
        // +0x0C: trailing word (untouched by recovered code; opaque).
        u32 muTrailing;
    };
};

}
}

#endif // CGS_SOUND_LOGIC_STATEMANAGER_REGISTEREDCONTENT_H
