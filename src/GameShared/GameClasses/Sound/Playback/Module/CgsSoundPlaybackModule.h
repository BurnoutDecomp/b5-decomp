#ifndef GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H
#define GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"  // the REAL Environment (+ Registry/Voice/Content via its includes)
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"      // the REAL Factory (CreateVoice<T>/CreateContent)
#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"  // SubmixVoice (ConnectVoice's real Connect target)

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
//   Module::CreateVoice       @ 0x826D7B00  (the IDA-truncated "Module::C")
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
//   +0x225C  mhRwacFactory     Handle<Factory>       (read as +8796 / a1[2199])
//   +0x2260  mhAemsFactory     Handle<Factory>       (read as +8800 / a1[2200])
//   +0x2264  mhSplicerFactory  Handle<Factory>       (a1[2201], zeroed by ctor)
//   +0x2268  mStreamMutex      EA::Thread::Mutex     Mutex(NULL, true)
//   +0x2298  mbReserved        bool                  (zeroed by ctor)
//   +0x26FC  mpStringTable     Module::StringTable*  (ImportStringTable list head)
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
// (2026-08-25, audio-faithfulness wave 6): the four minimal DEFER-slice rivals
// this header used to define (Environment / Voice / Content / Factory) are FOLDED
// onto their real homes, included above. Method reconciliation against the asm:
//   * `Environment::Ge`  == Environment::GetFactory(Name)   (DWARF CgsEnvironment.h:280)
//   * `Environment::GetV`== Environment::GetVoice(Ident)    (DWARF h:289)
//   * the rival Voice::SetSlotName(+0xC) was the real Voice::SetIdent (DWARF
//     CgsVoice.h:567) -- the "slot name" vocabulary was invented; the +0xC word is
//     Voice::mIdent, and the requested "slot name" IS the voice ident.
//   * Voice::Attach(Name, Handle<Content>&) (DWARF :501) / Voice::Connect(Name,
//     Handle<SubmixVoice>&) @0x826ACC90 replace the u32*/Handle<Voice>* shims.
//   * Factory::CreateVoice<T>/CreateContent use the REAL reference signatures
//     (u32 return, compared against (u32)-1 -- the asm `cmpwi r3,-1`).
//   * Registry::GetEntity<T>(Name&) is called BY SYMBOL in the asm
//     (`bl Registry::GetEntity<ContentSpec>`), and Registry::Dump is real (DWARF
//     CgsRegistry.h:137) -- the free RegistryGetEntity/RegistryDump shims are gone.
// STILL FLAG (DEFER), keystone-blocked: the per-factory registry accessors + the
// GenericRwacFactory name + the Snd9 hack hook below (AemsFactory does not derive
// Factory yet and its registry member is untyped -- the ledgered AEMS keystone).
// =============================================================================

namespace CgsSound
{
namespace Playback
{

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

// FLAG (DEFER): the playback "Snd9 init submix" hack-hook the asm calls
// HACK_SetSnd9InitSubmix on the E_COMMAND_VOICE_CREATE special path (slot name
// == K_INIT_SND9_SUBMIX_IDENT, 0xFFFFFFF0). Declared-only.
void HACK_SetSnd9InitSubmix(Handle<Voice>* lphVoice);

// CgsCommon.h. K_INIT_SND9_SUBMIX_IDENT -- the reserved slot name that flags the
// init-submix path in Module::C. The asm compares the requested slot name against
// -0x10 (cmpwi r24,-0x10); as a u32 that is 0xFFFFFFF0.
const u32 KU_INIT_SND9_SUBMIX_IDENT = 0xFFFFFFF0u;

namespace Module
{

// CgsSoundPlaybackModule.h. The low-level sound-playback module.
class Module
{
public:
    // DWARF CgsSoundPlaybackModule.h:489 names the member `StringTable*
    // mpStringTable` -- the chunks ImportStringTable allocates and chains. Each
    // chunk is `mpNext` followed by the copied string bytes. FLAG: interior shape
    // modelled from the ImportStringTable asm (store list head at +0, payload
    // from +4); the DWARF nested StringTable's own field names are un-dumped.
    struct StringTable
    {
        StringTable* mpNext;      // +0x00  previous list head
        char         macData[1];  // +0x04  copied string-table bytes (flexible)
    };

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

    // Module::ImportStringTable @ 0x826AD6B0 (DWARF h:486 signature: `void
    // ImportStringTable(const Registry&)` -- the blob the asm word-views IS a
    // serialised Registry: word[1]=entity capacity, word[2]=data-blob size,
    // word[4]=string-table size (the outer guard), word[5]=the string-region end
    // pointer, and the region start = base + 4*(capacity+7) + dataSize, exactly
    // the wave-4 serialised-Registry formula). Copies the string region into an
    // environment-allocated chunk, chains it onto mpStringTable, and interns every
    // NUL-terminated string via Name::MakeHash. Kept as the raw serialised-blob
    // word view (serialised record; the committed Registry's pointer members widen
    // on host, so the console word indices cannot be taken through the real type).
    void ImportStringTable(const u32* lpStringTableResource);

    // Module::AttachVoice @ 0x826D7D80 (DWARF h:345: AttachVoice(Handle<Voice>,
    // Handle<Content>, u32) -- the content param is a CONTENT handle; the old rival
    // typed it Handle<Voice>). Attaches content to a voice slot through the real
    // Voice::Attach, then releases both transient handle references.
    void AttachVoice(Handle<Voice>* lphVoice, Handle<Content>* lphContent, u32 lu32SlotName);

    // Module::ConnectVoice @ 0x826C14E0 (DWARF h:336). Looks up the submix voice
    // for a submix id, connects the given voice's send to it, then releases the
    // transient handles.
    void ConnectVoice(Handle<Voice>* lphVoice, u32 lu32SendName, u32 lu32SubmixId);

    // Module::CreateVoice @ 0x826D7B00 (DWARF h:318: `Handle<Voice>
    // CreateVoice(u32,u32,u32)` -- the by-value Handle return is lowered on PPC as
    // the sret out-pointer this signature spells; IDA truncated the symbol to
    // `Module::C`). Resolves the owning factory + voice spec out of the
    // environment, creates a voice for the requested ident, runs the init-submix
    // hack on the reserved ident, and returns the new voice in lphVoiceOut.
    void CreateVoice(Handle<Voice>* lphVoiceOut, u32 lu32SlotName, const Name& lFactoryName,
                     u32 lu32SubmixName);

    // Module::CreateContent @ 0x826C12A8. Resolve the owning factory (by class name)
    // + the content spec (by name) out of the environment, ask the factory to create
    // the content, wire its ident + owner iface, ref/release it, and release the
    // transient factory handle. Mirror of Module::C. Returns lppContentOut.
    Content** CreateContent(Content** lppContentOut, u32 lu32Ident,
                            const Name& lContentClassName,
                            const Name& lContentSpecName);

private:
    // (2026-08-25 wave 6: member names reconciled to the DWARF -- the tertiary
    // interface base at +0x22C is the `protected IContentLoadService` base the
    // DWARF declares (h:165), which is exactly the +0x22C sub-object pointer
    // CreateContent wires into each new content's mpLoadService. The rival names
    // mhReserved/mCommandLock/mpStringTableHead were the DWARF's mhSplicerFactory
    // (h:374, Handle<SplicerFactory>) / mStreamMutex (h:379) / mpStringTable
    // (h:489). The DWARF handle types are per-factory subclasses (GenericRwac/
    // Aems/Splicer); held as Handle<Factory> until those subclass homes are
    // includable here -- the ledgered AEMS keystone.)
    void*                mpVtbl;            // X360 +0x0000  primary vptr
    EA::Thread::RWMutex  mInputLock;        // X360 +0x0010
    EA::Thread::RWMutex  mOutputLock;       // X360 +0x0118
    void*                mpSecondaryVtbl;   // X360 +0x0228
    void*                mpTertiaryVtbl;    // X360 +0x022C (the IContentLoadService base)
    Handle<Environment>  mhEnvironment;     // X360 +0x2258 (DWARF h:362)
    Handle<Factory>      mhRwacFactory;     // X360 +0x225C (DWARF h:367, Handle<GenericRwacFactory>)
    Handle<Factory>      mhAemsFactory;     // X360 +0x2260 (DWARF h:370, Handle<AemsFactory>)
    Handle<Factory>      mhSplicerFactory;  // X360 +0x2264 (DWARF h:374, Handle<SplicerFactory>)
    EA::Thread::Mutex    mStreamMutex;      // X360 +0x2268 (DWARF h:379)
    bool                 mbReserved;        // X360 +0x2298 (un-named: inside the DWARF
                                            //  mDeferredResourceRequestQueue span)
    StringTable*         mpStringTable;     // X360 +0x26FC (DWARF h:489)
};

// CgsSoundPlaybackModule.h. The Module's content-preparation pipeline stage counter.
// operator++ @ 0x82681C70 post-increments it (unconditionally, before the bound
// assert) and returns the old stage. E_PREPARESTAGE_DONE == 4 (distinct from the
// CgsSound::Logic operator++ bound of 6).
enum EPrepareStage
{
    E_PREPARESTAGE_0    = 0,
    E_PREPARESTAGE_1    = 1,
    E_PREPARESTAGE_2    = 2,
    E_PREPARESTAGE_3    = 3,
    E_PREPARESTAGE_DONE = 4
};

// @ 0x82681C70. Post-increment the prepare-stage enum (stored unconditionally,
// before the bound assert); returns the saved OLD stage.
EPrepareStage operator++(EPrepareStage& leEnumIndex, int);

} // namespace Module
} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H
