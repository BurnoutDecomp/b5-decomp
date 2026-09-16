#ifndef BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/BrnResourceRegistrar.h"   // BrnSound::Logic::IResourceRequester + ResourceRegistrar (canonical home)

// =============================================================================
// BrnSound::Logic::BrnEffectControl
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnEffectControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnEffectControl is the sound-logic
// effect *control* sibling of BrnEffectObject: same dual-base shape. The DWARF
// shows `BrnEffectControl : public EffectControl`, but the X360 vector deleting
// destructor proves a SECOND base (IResourceRequester) exactly like BrnEffectObject:
//   - primary vptr written at this+0, IResourceRequester sub-object vptr at this+4
//   - the same final IResourceRequester sub-object vtable (off_820AA820, shared with
//     BrnEffectObject's dtor @ 0x826AF4C8) is stored at this+4
//   - the `vector deleting destructor adjustor{4}` does `this - 4` to recover the
//     primary object before forwarding to the real destructor
//   - identical member teardown offsets: meAttachState @ +0x24, meDetachState @
//     +0x28, mbResourcesReady @ +0x31
// So BrnEffectControl multiply-inherits the engine effect-control base (primary vptr
// @ this+0) and IResourceRequester (sub-object vptr @ this+4).
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// BrnEffectControl TU, whose only two recon'd functions are the vector deleting
// destructor (@ 0x826AEF68) and its adjustor{4} thunk (@ 0x82696868). The control
// hierarchy's full surface (Prepare/UpdateParams/Notify/GetBrnLogicModule/RTTI
// CreateObject, and the Brn3DEffectControl / Brn3DUserSpaceEffectControl subclasses
// with their transform/emitter members) is DEFERRED to its own TU(s). Only the
// members the destructor tears down are modelled BY NAME here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ASM accesses members by
// absolute byte offset (meAttachState @ +0x24, meDetachState @ +0x28,
// mbResourcesReady @ +0x31). Those offsets assume 4-byte pointers and a 4-byte
// vptr; on a 64-bit host pointer/vptr widths differ, so members are pinned BY NAME
// and SEQUENCE only and absolute offsets are NOT static_asserted across pointers.
//
// ODR: this home models its own minimal EffectControl/EffectBase/IResourceRequester
// shape (mirroring the BrnEffectObject home for the object hierarchy). The two homes
// are never included in the same TU, so there is no redefinition clash. The full
// CgsEffectBase.h home (DWARF CgsEffectBase.h:379) is DEFERRED.
// =============================================================================

#if 0 // RETIRED: the former minimal rival engine-effect definitions; canonical CgsEffectBase.h is used above.
namespace CgsSound
{
namespace Logic
{

// SHARED minimal engine-effect types (ClassTypeInfo + EffectBase) -- see the matching
// note in BrnEffectObject.h. Both headers gate these on the SAME guard macro so they
// can be co-included (e.g. by BrnTrafficEngine.h, which homes both a BrnEffectObject-
// derived TrafficEngine and a BrnEffectControl-derived Traffic3DControl) without an ODR
// clash. Whichever header is parsed first defines them; the other skips. BrnEffectObject
// .h's EffectBase is a superset of this one, so it satisfies EffectControl's members too.
// ⛔⛔ THE REDUCED CgsSound::Logic::EffectBase / EffectObject / EffectControl BLOCK THAT
// STOOD HERE IS GONE -- IT WAS UNREACHABLE, AND IT LIED ABOUT THE BASE SURFACE.
//
// This header includes the CANONICAL engine home,
// GameShared/GameClasses/Sound/Logic/CgsEffectBase.h, at the top of the file. That header
// defines EffectBase (the full DWARF surface: mpNextEffectBase / mpState / mu16RefCount /
// mu16AttachCount / mu16AllocatorIndex / miObjectId / mfRunningTime / mfDeltaTime /
// meAttachState / meDetachState / mpLogicModule / mbEnabled / mbHasLoadedData /
// mpDynamicMixIo, and the virtuals Prepare / Attach / GetController / AttachController /
// SetupLoadData / UpdateParams / ProcessUpdate / Detach / Notify / Destroy) plus
// EffectObject and EffectControl. The block that used to sit here re-declared the same
// three types, in the same namespace, with FEWER members and NO virtuals, behind
// `#ifndef CGS_SOUND_LOGIC_EFFECT_ENGINE_TYPES_DEFINED`.
//
// PROVEN DEAD 2026-09-16 by compile gate: a probe deriving from CameraControl (this
// header's family) and from SubmixesEffect (the sibling's) resolves mu16AttachCount,
// mu16RefCount, mfRunningTime and mpDynamicMixIo and overrides Attach / UpdateParams /
// ProcessUpdate / Notify. Every one of those exists ONLY in the canonical header, so the
// canonical definition is what every leaf has always seen; if a reduced block had ever
// won, each of those would have been a compile error instead.
//
// ⚠️ THE COST OF LEAVING IT: the reduced block's own banner claimed it was the definition
// in use, and grepping THIS FILE for the base surface is what produced the false finding
// "BrnEffectControl declares neither `virtual bool Attach()` nor mu16AttachCount, so
// CameraControl::Attach cannot be written without growing every sound control" -- a
// blocker that never existed and that stopped a wave. Derive the base surface from
// CgsEffectBase.h, or from a compile gate; never from a re-declaration.

// CgsEffectBase.h:765 (DWARF). EffectControl : public EffectBase. The control-side
// counterpart of EffectObject; carries the per-class RTTI hook used by
// BrnEffectControl's GetTypeInfo/CreateObject.
struct EffectControl : public EffectBase
{
    EffectControl() {}
    virtual ~EffectControl() {}

    virtual ClassTypeInfo<EffectControl>* GetTypeInfo() const;
    virtual const char*                   GetTypeName() const;
};

} // namespace Logic
} // namespace CgsSound
#endif

namespace BrnSound
{
namespace Logic
{

// IResourceRequester + ResourceRegistrar now live in their canonical home
// (GameSource/Sound/BrnResourceRegistrar.h, included above) -- folded out of here to resolve the
// cross-header ODR.

// BrnEffectControl.h:56 (DWARF). Multiply inherits the engine effect-control base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4).
struct BrnEffectControl : public CgsSound::Logic::EffectControl,
                          public IResourceRequester
{
    BrnEffectControl();
    virtual ~BrnEffectControl();

    // BrnEffectControl.cpp:38 — per-class RTTI (DEFERRED bodies; declared for the
    // control vtable shape).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const;
    virtual const char*                                                     GetTypeName() const;

    // BrnEffectControl.h:220 — IResourceRequester override. DEFERRED (outside this
    // TU's recon'd function set); declared for the second-base vtable shape.
    virtual void ResourcesAreReady();

    // BrnEffectControl.h:231 — IResourceRequester override. DEFERRED; declared for
    // the second-base vtable shape.
    virtual ResourceRegistrar& GetResourceRegistrar();

    // BrnEffectControl.h:239 — override of the effect Detach. DEFERRED; declared
    // for the control vtable shape.
    virtual bool Detach();


    // ================= [FLAG PC bring-up] THE SET HALF OF THE +0x2D LATCH =================
    // Detach above is X360-attested (0x826EBF88): it TESTS mbResourceRequestActive, and only
    // then pulls this requester's rows out of the registrar. Nothing in this tree ever set
    // that byte to true, so the release arm was DEAD CODE and every request an effect ever
    // made stayed on the resource's requester list forever.
    //
    // MEASURED (scratch/flow_run/carW, -StartEvent -AIDrive, 275 s, five AI rivals):
    // 137 AI sound attaches, 16 detaches, and 196 x "We've run out of nodes." out of
    // LinkedListHelper<IResourceRequester*,16>::AddTail via ResourceRegistrar::UpdateRequests.
    // UpdateRequests appends the requester unconditionally, so a re-request by a requester
    // that never released takes a SECOND node; the fixed 16-node per-resource pool is then
    // exhausted by ~16 attach/detach cycles. The witness named both ends:
    //   [reg-pool] ... 16/16 for bundle 'sound\aems\InAir.bundle' ... requester 0x676ECD30
    //   [reg-pool] ... 16/16 for bundle 'Engines\af355519.bundle' ... requester 0x66C678B0
    // -- the SAME requester pointer re-added for the SAME bundle, which is exactly the
    // signature of a missing release.
    //
    // WHAT IS AND IS NOT INVENTED. The latch's test-and-clear half is read from the console;
    // its name is "a resource request is active"; and the only event that can make that true
    // is issuing one. The set is placed at the single choke point through which an effect
    // issues a request -- IResourceRequester::LoadAsset @0x826E2348 -- by hiding it with a
    // forwarding overload, so every unqualified LoadAsset in an effect leaf latches. No leaf
    // is edited and no request is changed; only the flag the console's own Detach reads.
    // The console's own store site for +0x2D has NOT been located in the image (a 43-site
    // `stb ...0x2D` sweep found five non-zero writers, none in the sound range), so the
    // PLACEMENT is inferred and marked; the EXISTENCE of a setter is not in doubt, because a
    // latch that is only ever tested and cleared cannot be what the console shipped.
    // DELETE-WHEN the console's store site is found and this moves to it verbatim.
    void LoadAsset(const char* lpcBundleName, const char* lpcResourceName,
                   ResourceRegistrar::EType leType)
    {
        mbResourceRequestActive = true;
        IResourceRequester::LoadAsset(lpcBundleName, lpcResourceName, leType);
    }

    void LoadAsset(const char* lpcResourceName, EResourcePool lePool,
                   ResourceRegistrar::EType leType)
    {
        mbResourceRequestActive = true;
        IResourceRequester::LoadAsset(lpcResourceName, lePool, leType);
    }

    // Members observed in the X360 resource-request and dtor paths (by name).
    //   +0x2D -> mbResourceRequestActive (Detach tests/clears it)
    //   +0x31 -> mbResourcesReady        (dtor stores false)
    // FLAG: X360 byte offset (+0x31) not asserted on the 64-bit host.
    bool mbResourceRequestActive;
    bool mbResourcesReady;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_EFFECT_CONTROL_H
