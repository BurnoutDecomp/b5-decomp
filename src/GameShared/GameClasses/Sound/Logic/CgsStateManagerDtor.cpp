// ============================================================================
// CgsStateManagerDtor.cpp -- CgsSound::Logic::StateManager destructor home.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Logic::StateManager::`vector deleting destructor'  @ 0x826FAAB8
//
// Decoded from the asm at 0x826FAAB8 (this == r31):
//   stw  off_820B66E4, 0(this)        ; install StateManager's own vtable
//   addi r3, this, 0x30               ; &this->mContentPool (RegisteredContent pool)
//   bl   StateManager::RegisteredContent_4_int::~ObjectPool  ; destroy the pool
//   stw  off_820AA820, 0(this)        ; re-install the MemBase base-subobject vtable
//   if (flags & 1)                    ; deleting flavour
//       <sound allocator>.Free(this)  ; via off_82FFB954, vtable slot +0x14
//
// This is MSVC's vector-deleting-destructor for the StateManager keystone. The
// source-level ~StateManager() destroys the content-registry ObjectPool member at
// +0x30; the two vtable installs (StateManager's own off_820B66E4 on entry, then
// the MemBase base vtable off_820AA820 as the chain unwinds) and the conditional
// allocator-routed free are the compiler-synthesised parts of the thunk, which
// MSVC re-emits from this virtual destructor + the class's own operator delete.
// off_82FFB954 (the sound allocator) is not homed in this group, so the host
// toolchain's `delete` stands in for the custom-allocator dispatch.
//
// The recovered pool destructor is the already-homed
//   ObjectPool<StateManager::RegisteredContent, 4, int>::~ObjectPool  @ 0x826EAC90
// (instantiated in ObjectPool_StateManagerRegisteredContent_4.cpp; per-element
// RegisteredContent teardown homed in CgsStateManagerRegisteredContent.h). This TU
// reuses the SAME nested RegisteredContent element type so the destructor it calls
// is exactly that recovered one.
//
// FLAG (committed-home coexistence -- conductor please note): two sibling partial
// views of CgsSound::Logic::StateManager already exist:
//   * CgsStateManager.h            -- a MemBase-derived view that homes IsStateAlias
//                                     + the leading scalar fields (mfCurrentTime ..
//                                     meMapState).
//   * CgsStateManagerRegisteredContent.h -- a non-polymorphic view that hosts only
//                                     the nested RegisteredContent type for the pool
//                                     instantiation TU.
// This file adds a THIRD partial view: the polymorphic StateManager that owns the
// RegisteredContent ObjectPool member at +0x30 and whose virtual destructor is the
// function this TU owns. The three views never appear in the same TU (each .cpp
// includes only one), so they compile independently; they are NOT layout-compatible
// restatements of one another and must not be read as such. When StateManager gets a
// single real owning header, fold IsStateAlias, the leading scalars, the nested
// RegisteredContent, the content pool, and this destructor onto it. Done minimally
// here to avoid forking any existing surface. Member access is BY NAME; absolute
// offsets are NOT static_asserted (the host 64-bit vptr differs in width from the
// X360 32-bit vptr).
// ============================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Sound/CgsMemBase.h"          // CgsSound::MemBase
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"   // CgsContainers::ObjectPool
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // CgsSound::Playback::Object

namespace CgsSound
{
namespace Logic
{

// Polymorphic StateManager view owning the content-registry pool. Reconstructed
// from the destructor at 0x826FAAB8; see file header FLAG for the coexistence of
// the two sibling partial views.
class StateManager : public CgsSound::MemBase
{
public:
    StateManager() {}

    // The function this TU owns. Bodied out-of-line below so its vtable slot
    // (off_820B66E4) is emitted in this TU exactly once.
    virtual ~StateManager();

    // One slot of the StateManager content registry. Layout + teardown recovered
    // from the inlined per-element destructor in the pool dtor at 0x826EAC90 (see
    // CgsStateManagerRegisteredContent.h, which hosts the canonical view). Restated
    // here member-for-member so this TU's pool instantiation matches the committed
    // one element-for-element.
    class RegisteredContent
    {
    public:
        RegisteredContent() : muLeading(0), mpContent(0), muTrailing(0) {}

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
        u32                         muLeading;   // +0x00 opaque (untouched by recon'd code)
        CgsSound::Playback::Object* mpContent;   // +0x08 held refcounted content, or null
        u32                         muTrailing;  // +0x0C opaque (untouched by recon'd code)
    };

private:
    // +0x30 (proven by `addi r3, this, 0x30` before the pool destructor call). The
    // content-registry pool: capacity 4, int slot index. Its destructor is the
    // already-homed ObjectPool<RegisteredContent,4,int>::~ObjectPool @ 0x826EAC90.
    CgsContainers::ObjectPool<RegisteredContent, 4, int> mContentPool;
};

// ~StateManager() destroys mContentPool (the X360's `bl ~ObjectPool` on this+0x30);
// the surrounding vtable re-installs and the conditional allocator free are emitted
// by the compiler's deleting-destructor thunk.
StateManager::~StateManager()
{
}

} // namespace Logic
} // namespace CgsSound
