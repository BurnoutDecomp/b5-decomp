#ifndef GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H
#define GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"

#include "eathread/eathread_rwmutex.h"
#include "eathread/eathread_mutex.h"

// =============================================================================
// CgsSoundPlaybackModule.h  (HOME for CgsSound::Playback::Module::Module)
//
// The sound-playback Module: the runtime root of the low-level playback engine.
// It owns the active sound Environment, the RWAC + AEMS factories, a string-table
// list, and read/write/command locks. The seven X360 functions reconstructed in
// CgsSoundPlaybackModule.cpp are:
//
//   Module::Module            @ 0x827DFA98  (ctor -- EXECUTED in boot trace)
//   Module::GetEnvironment    @ 0x82694130
//   Module::DumpRegistries    @ 0x82694188
//   Module::ImportStringTable @ 0x826AD6B0
//   Module::AttachVoice       @ 0x826D7D80
//   Module::ConnectVoice      @ 0x826C14E0
//   Module::C                 (CreateVoice) @ 0x826D7B00
//
// LAYOUT (recovered from the ctor at 0x827DFA98 and the member-offset reads in the
// other six functions). On X360 the byte offsets are exact; on the LLP64 host the
// pointer members widen, so members are pinned BY NAME + SEQUENCE only and NO
// absolute-offset static_assert is emitted (the EA::Thread mutex sub-objects and
// the pointer-sized handle members are host-width-variant). The X360 offsets, kept
// in comments, are the load-bearing facts the asm reads:
//
//   +0x0000  primary   vptr   (off_820CFA30)
//   +0x0010  mInputLock        EA::Thread::RWMutex   RWMutex(NULL, true)
//   +0x0118  mOutputLock       EA::Thread::RWMutex   RWMutex(NULL, true)
//   +0x0228  secondary vptr   (off_820CFA24)
//   +0x022C  tertiary  vptr   (off_820CFA20)
//   +0x2258  mhEnvironment     Handle<Environment>   (the only handle read as +8792)
//   +0x225C  mhRwacFactory     Handle<RwacFactory>   (read as +8796 / a1[2199])
//   +0x2260  mhAemsFactory     Handle<AemsFactory>   (read as +8800 / a1[2200])
//   +0x2264  mhReserved        Handle<Object>        (a1[2201], zeroed by ctor)
//   +0x2268  mCommandLock      EA::Thread::Mutex     Mutex(NULL, true)
//   +0x2298  mbReserved        bool                  (zeroed by ctor)
//   +0x26FC  mpStringTableHead StringTableChunk*     (ImportStringTable list head)
//
// FLAG: this class has THREE distinct vtables installed by the ctor (a multiple-
// inheritance object: primary base at +0, two secondary interface bases at +0x228 /
// +0x22C). Their concrete polymorphic interfaces are NOT recovered by this TU, so
// the bases are modelled as raw vtable-pointer slots (mpVtbl / mpSecondaryVtbl /
// mpTertiaryVtbl) that the ctor writes by NAME. The ctor first installs an
// intermediate set (off_820CE500 / off_820CE350 / off_820CE358) then overwrites
// them with the final set (off_820CFA30 / off_820CFA24 / off_820CFA20) -- the X360
// emits both store pairs (base-then-derived ctor chaining), reconstructed here as
// the two sequential assignments the asm performs.
//
// FLAG: MINIMAL FLAGGED HOME. The dependency surface this TU calls into
// (Playback::Environment, Playback::Voice, Playback::VoiceSpec, Playback::Factory,
// Registry::GetEntity / Registry::Dump, operator<< stream + HACK_SetSnd9InitSubmix)
// is NOT yet homed in src. Those types/functions are modelled here as minimal
// slices / free declarations carrying ONLY the members + signatures the asm reads,
// so the seven Module methods body store-for-store. Each is tagged FLAG (DEFER) and
// must fold onto its real home when that TU lands.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// FLAG (DEFER): minimal Playback::Environment slice. The Module reads the
// environment's registry pointer (mpRegistry @ +0x4C on X360) and calls the
// playback-environment helpers Ge / GetV / Allocate. Full Environment is its own
// TU (CgsEnvironment.h:* in the Playback tree). Only mpRegistry and the helper
// signatures below are load-bearing here.
// ---------------------------------------------------------------------------
class Registry;

class Environment : public Object
{
public:
    // CgsEnvironment.h:661 -- the entity registry this environment owns. Read as
    // `*(env + 76)` in DumpRegistries / C; asserted non-null ("mpRegistry").
    Registry* GetRegistry() const
    {
        CGS_ASSERT(mpRegistry, "mpRegistry");
        return mpRegistry;
    }

    // Module::C @ 0x826D7B00 dispatches these two through the environment handle.
    // FLAG (DEFER): declared-only -- bodied in the Environment TU. `Ge` looks up an
    // owning Factory handle by a name key; `GetV` looks up a submix Voice by id;
    // `Allocate` carves environment-owned storage. Signatures taken from the asm
    // call sites (Hex-Rays truncates the names to Ge / GetV).
    static void GetFactoryHandle(Handle<class Factory>* lpHandleOut,
                                 Environment* lpEnvironment, const Name* lpFactoryName);
    static Object** GetSubmixVoice(Handle<class Voice>* lpHandleOut,
                                   Environment* lpEnvironment, u32 lu32SubmixId);
    static void* Allocate(Environment* lpEnvironment, size_t luSize, u32 lu32Align,
                          const char* lkpacTag);

private:
    // FLAG (host-variant layout): on X360 mpRegistry sits at the absolute byte
    // offset +0x4C, past several unrecovered environment-header words. That absolute
    // offset is pointer-width-variant on the LLP64 host, so per AGENTS.md the member
    // is pinned BY NAME only -- no padding to force +0x4C and no offset static_assert
    // (the unrecovered header words are DEFERRED to the Environment TU). Only the
    // accessor GetRegistry() is load-bearing for this Module TU.
    Registry* mpRegistry;   // CgsEnvironment.h:* (X360 +0x4C)
};

// ---------------------------------------------------------------------------
// FLAG (DEFER): minimal Playback::Voice slice. Module reaches Voice through a
// Handle and calls Attach / Connect; Module::C also writes the slot name at
// Voice +0xC after CreateVoice. Voice derives from Object (vptr + refcount @ +0/+4)
// like every other playback object; the asm `++*(v + 4)` increments that refcount.
// ---------------------------------------------------------------------------
class Voice : public Object
{
public:
    // Module::C @ 0x826D7B00: `*(voice + 12) = lSlotName` -- store the slot name
    // into the freshly created voice. Named by SEQUENCE (X360 +0xC).
    void SetSlotName(u32 lu32SlotName) { mu32SlotName = lu32SlotName; }

    // FLAG (DEFER): declared-only -- bodied in the Voice TU. Attach binds content
    // to a slot; Connect routes the voice to a submix. Return bool (asm tests the
    // low byte via clrlwi r11,r3,24).
    bool Attach(const u32* lpSlotName, Handle<Voice>* lphContent);
    bool Connect(const u32* lpSendName, Handle<Voice>* lphSubmixVoice);

private:
    // FLAG (host-variant layout): mu32SlotName is at the absolute X360 byte offset
    // +0xC, past unrecovered voice-header words after the Object base. That absolute
    // offset is host-variant, so it is pinned BY NAME only (no forcing pad / offset
    // static_assert); the intervening words are DEFERRED to the Voice TU.
    u32 mu32SlotName;                           // X360 +0x0C
};

// FLAG (DEFER): the voice-creation spec entity, looked up out of the registry by
// the requested factory name. Opaque here -- only used as an opaque key/handle.
class VoiceSpec;

// ---------------------------------------------------------------------------
// FLAG (DEFER): minimal Playback::Factory slice. Module::C dereferences the
// environment's factory handle and calls the templated CreateVoice. The concrete
// Factory home is CgsFactory.cpp (DoDispose only, so far). Only CreateVoice's
// signature is load-bearing here.
// ---------------------------------------------------------------------------
class Factory : public Object
{
public:
    // ??$CreateVoice@VVoice@... -- create a voice from a spec into an out-handle.
    // Returns -1 on failure (the asm `cmpwi r3,-1`). FLAG (DEFER): declared-only,
    // bodied in the Factory TU.
    template <typename T>
    int CreateVoice(const VoiceSpec* lpSpec, Handle<T>* lphVoiceOut, u32 lu32SlotName);
};

// FLAG (DEFER): the RWAC + AEMS factories each own their own registry, but at
// DIFFERENT offsets from the generic Factory's (DumpRegistries reads the RWAC
// factory's registry at X360 +0x401C and the AEMS factory's at +0x60). Modelled as
// free accessors keyed on the generic Factory pointer so the single Handle<Factory>
// member type stays uniform; each returns the factory's registry by NAME. Bodied in
// the respective factory TU (CgsGenericRwacFactory / CgsAemsFactory).
Registry* GetRwacFactoryRegistry(Factory* lpRwacFactory);  // X360 +0x401C
Registry* GetAemsFactoryRegistry(Factory* lpAemsFactory);  // X360 +0x60

// FLAG (DEFER): GenericRwacFactory::SK_NAME -- the reserved factory name the
// init-submix path asserts the request matches (the X360 compares lFactoryName
// against dword_83008650). Returned by NAME; bodied in the GenericRwacFactory TU.
const Name& GenericRwacFactorySkName();

// FLAG (DEFER): registry entity lookup + dump. GetEntity<T> returns the spec
// pointer (or 0); Dump logs the table. Bodied in the Registry TU.
template <typename T>
const T* RegistryGetEntity(Registry* lpRegistry, const Name& lName);
void     RegistryDump(Registry* lpRegistry);

// FLAG (DEFER): the playback "Snd9 init submix" hack-hook the asm calls
// HACK_SetSnd9InitSubmix on the E_COMMAND_VOICE_CREATE special path (slot name
// == K_INIT_SND9_SUBMIX_IDENT, 0xFFFFFFF0). Declared-only.
void HACK_SetSnd9InitSubmix(Handle<Voice>* lphVoice);

// CgsCommon.h. K_INIT_SND9_SUBMIX_IDENT -- the reserved slot name that flags the
// init-submix path in Module::C. The asm compares the requested slot name against
// -0x10 (cmpwi r24,-0x10); as a u32 that is 0xFFFFFFF0.
const u32 KU_INIT_SND9_SUBMIX_IDENT = 0xFFFFFFF0u;

// ---------------------------------------------------------------------------
// The string-table chunks Module::ImportStringTable allocates and chains. Each
// chunk is `mpNext` followed by the copied string bytes; the list head is
// mpStringTableHead. FLAG: shape modelled from the ImportStringTable asm
// (store env-list head at +0, payload from +4).
// ---------------------------------------------------------------------------
struct StringTableChunk
{
    StringTableChunk* mpNext;   // +0x00  previous list head
    char              macData[1];  // +0x04  copied string-table bytes (flexible)
};

namespace Module
{

// CgsSoundPlaybackModule.h. The low-level sound-playback module.
class Module
{
public:
    // Module::Module @ 0x827DFA98. Installs the three vtables, default-constructs
    // the read/write/command locks, and zeroes the environment + factory handles,
    // the reserved handle, and the reserved flag. EXECUTED in the boot trace.
    Module();

    // Module::GetEnvironment @ 0x82694130. Returns the owned environment (asserts
    // the handle is non-null first -- fires CgsHandle.h:305).
    Environment* GetEnvironment();

    // Module::DumpRegistries @ 0x82694188. Dumps the environment, RWAC-factory and
    // AEMS-factory registries (each asserted non-null along the way).
    void DumpRegistries();

    // Module::ImportStringTable @ 0x826AD6B0. Copies a CSIS string-table blob into
    // an environment-allocated chunk, chains it onto mpStringTableHead, and interns
    // every NUL-terminated string in it via Name::MakeHash. Returns the new chunk
    // (or the unchanged `this`-derived value on the no-op paths). FLAG: the X360
    // returns the last-touched pointer in r3; modelled as void-effect with the
    // chunk chained, matching the stores.
    void ImportStringTable(const u32* lpStringTableResource);

    // Module::AttachVoice @ 0x826D7D80. Attaches content to a voice slot through
    // the playback Voice handle, then releases both transient handle references.
    void AttachVoice(Handle<Voice>* lphVoice, Handle<Voice>* lphContent, u32 lu32SlotName);

    // Module::ConnectVoice @ 0x826C14E0. Looks up the submix voice for a submix id,
    // connects the given voice's send to it, then releases the transient handles.
    void ConnectVoice(Handle<Voice>* lphVoice, u32 lu32SendName, u32 lu32SubmixId);

    // Module::C (CreateVoice) @ 0x826D7B00. Resolves the owning factory + voice spec
    // out of the environment, creates a voice for the requested slot, runs the
    // init-submix hack on the reserved slot, and returns the new voice in lphVoiceOut.
    void C(Handle<Voice>* lphVoiceOut, u32 lu32SlotName, const Name& lFactoryName,
           u32 lu32SubmixName);

private:
    void*                mpVtbl;            // X360 +0x0000  primary vptr
    EA::Thread::RWMutex  mInputLock;        // X360 +0x0010
    EA::Thread::RWMutex  mOutputLock;       // X360 +0x0118
    void*                mpSecondaryVtbl;   // X360 +0x0228
    void*                mpTertiaryVtbl;    // X360 +0x022C
    Handle<Environment>  mhEnvironment;     // X360 +0x2258
    Handle<Factory>      mhRwacFactory;     // X360 +0x225C
    Handle<Factory>      mhAemsFactory;     // X360 +0x2260
    Handle<Object>       mhReserved;        // X360 +0x2264
    EA::Thread::Mutex    mCommandLock;      // X360 +0x2268
    bool                 mbReserved;        // X360 +0x2298
    StringTableChunk*    mpStringTableHead; // X360 +0x26FC
};

} // namespace Module
} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H
