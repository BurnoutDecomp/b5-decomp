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
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // the REAL ModuleSingleBuffered base (phase B1)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // EventReceiverQueue<0x2000,16> (mResourceReceiverQueue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<1024,16> (mDeferredResourceRequestQueue)

#include "eathread/eathread_rwmutex.h"
#include "eathread/eathread_mutex.h"

// Pointer-only members (the phase-B4 Io pair retypes them).
namespace CgsModule { struct IOBuffer; }

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
//   +0x0000..0x0227  the ModuleSingleBuffered BASE (primary vptr off_820CFA30 +
//                    the two RWMutexes @+0x10/+0x118 + the DataBuffer pair) --
//                    a REAL base class since phase B1
//   +0x0228  secondary vptr   (off_820CFA24; un-identified interface base)
//   +0x022C  tertiary  vptr   (off_820CFA20; the DWARF IContentLoadService base)
//   +0x0230.. the full DWARF member list -- see the class below (the layout
//             closes with zero gaps against the Construct/Prepare/Update/Release
//             asm; the ctor's +0x2298 byte-zero is the deferred queue's count)
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
//
// (2026-08-25, faithful-audio-engine phase B1: the class now derives the REAL
// CgsModule::ModuleSingleBuffered -- the +0 vtable + the two RWMutexes at console
// +0x10/+0x118 the old raw-slot model hand-carried ARE that base (Construct
// @0x826C0EC0 chains ModuleSingleBuffered::Construct; the base's console extent is
// exactly 0x228, where the first interface vptr sits) -- and carries the FULL
// DWARF member list. The layout closes with ZERO gaps against the Construct/
// Prepare/Update/Release asm: receiver storage 0x250+0x2000 == mpInputBuffer
// 0x2250; mStreamMutex 0x2268+0x30 == the deferred queue 0x2298; queue+pad ==
// maStreamBuffers 0x26A8; +0x48 == the size words 0x26F0. Host layout is by-name;
// console offsets are comments.)
class Module : public CgsModule::ModuleSingleBuffered
{
public:
    // DWARF CgsSoundPlaybackModule.h (nested). The content-preparation stage
    // counter (bound E_PREPARESTAGE_DONE == 4; the operator++ below walks it).
    enum EPrepareStage
    {
        E_PREPARESTAGE_0    = 0,
        E_PREPARESTAGE_1    = 1,
        E_PREPARESTAGE_2    = 2,
        E_PREPARESTAGE_3    = 3,
        E_PREPARESTAGE_DONE = 4
    };

    // DWARF CgsSoundPlaybackModule.h:355 (nested EReleaseStage). The release
    // COUNTDOWN cursor: Construct seeds DONE(6) = nothing to release; each Prepare
    // stage lowers it to its own unwind rung (5/3/2/0), and Release runs FORWARD
    // from it (@0x826C0FB8), bumping with the same +1/bound-6 idiom the .h:501
    // assert names ("leEnumIndex <= Module::E_RELEASESTAGE_DONE"). Enumerator
    // names beyond DONE are unrecovered; the raw rungs are stamped with comments.
    enum EReleaseStage
    {
        E_RELEASESTAGE_DONE = 6
    };

    // DWARF CgsSoundPlaybackModule.h:465 (nested, StreamBuffer[3] @ console
    // +0x26A8, 24-byte records). One SndPlayer1 stream-buffer record. FLAG: only
    // the fields Construct/Prepare touch are named from evidence (mpBuffer +
    // the constant-4 word both seed; Prepare stage 3 re-stores them); the three
    // middle words + the trailing float are zero-seeded, meanings owned by the
    // UpdateStreamBuffers/FindStreamBuffer slice (phase B4).
    struct StreamBuffer
    {
        void* mpBuffer;        // +0x00  the carved stream buffer (Prepare stage 3)
        u32   mu32Reserved04;  // +0x04  seeded 4 by Construct AND Prepare
        u32   mu32Reserved08;  // +0x08
        u32   mu32Reserved0C;  // +0x0C
        u32   mu32Reserved10;  // +0x10
        f32   mf32Reserved14;  // +0x14
    };

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

    // Module::Module @ 0x827DFA98. The base sub-object ctor (vtable + the two
    // RWMutexes = ModuleSingleBuffered), the two interface-base vptr installs
    // (X360 off_820CE350/58 then the final off_820CFA24/20), the zeroed handles,
    // the stream mutex, and the nulled string-table head. EXECUTED in the boot
    // trace. (Phase B1: the base half is now the REAL inherited ctor.)
    Module();

    // Module::Construct(int) @ 0x826C0EC0 (vtable +0x40 -- RootSoundModule::
    // Construct step [3] calls it with module-id 6). Seeds every own member:
    // stages (prepare 0 / release DONE), binds+clears the receiver queue,
    // constructs the deferred-request queue, zeroes the stream-buffer records
    // (mu32Reserved04 = 4), the buffers/sizes/string-table head, the time fields,
    // stores the pool id, and sets the base's mbIsNewModule.
    // (a NEW virtual -- the console vtable slot +0x40; the base's no-arg
    // Construct() keeps its own slot and is hidden by name, as on the X360.)
    virtual void Construct(s32 li32PoolId);
    using CgsModule::ModuleSingleBuffered::Construct;   // keep the base overload visible

    // Module::Release @ 0x826C0FB8 (virtual). The release COUNTDOWN machine run
    // forward from meReleaseStage: free the string-table chain through the
    // environment allocator's DoFree; null the three factory handles; snapshot
    // the environment allocator + null the environment handle; free the stream
    // buffers (only when mbStreamsUsingMainAllocator); release the deferred
    // queue + receiver + base. Returns false while still releasing.
    bool Release() override;

    // Module::Destruct @ 0x826C1268 (virtual). Queue destruct + receiver clear +
    // base destruct.
    void Destruct() override;

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
    // Full DWARF member list (phase B1; the base ModuleSingleBuffered owns the
    // console +0x00..+0x227 span -- vtable + the two RWMutexes + the DataBuffer
    // pair -- so the raw mpVtbl/mInputLock/mOutputLock slots the old model
    // hand-carried are GONE). The tertiary interface base at +0x22C is the
    // `protected IContentLoadService` base the DWARF declares (h:165) -- the
    // exact sub-object pointer CreateContent wires into each new content's
    // mpLoadService; the +0x228 base is un-identified. Both stay raw vptr slots
    // until those interfaces are typed. The DWARF handle types are per-factory
    // subclasses (GenericRwac/Aems/Splicer); held as Handle<Factory> until those
    // subclass homes are includable here -- the ledgered AEMS keystone.
    void*                mpSecondaryVtbl;   // X360 +0x0228  (interface base 1, un-identified)
    void*                mpTertiaryVtbl;    // X360 +0x022C  (the IContentLoadService base)
    EPrepareStage        mePrepareStage;    // X360 +0x0230  (DWARF h:354)
    EReleaseStage        meReleaseStage;    // X360 +0x0234  (DWARF h:355; the countdown cursor)
    CgsModule::EventReceiverQueue<0x2000, 16>
                         mResourceReceiverQueue; // X360 +0x0238 hdr + 0x0250 storage (DWARF h:357,
                                            //  EventReceiverQueue<8192,16>; Construct binds cap
                                            //  0x2000 / align 16 / storage this+0x250 + Clear)
    CgsModule::IOBuffer* mpInputBuffer;     // X360 +0x2250 (DWARF h:359, InputBuffer* -- retyped
                                            //  to the Io type with the phase-B4 IO pair)
    CgsModule::IOBuffer* mpOutputBuffer;    // X360 +0x2254 (DWARF h:360, OutputBuffer* -- ditto)
    Handle<Environment>  mhEnvironment;     // X360 +0x2258 (DWARF h:362)
    Handle<Factory>      mhRwacFactory;     // X360 +0x225C (DWARF h:367, Handle<GenericRwacFactory>)
    Handle<Factory>      mhAemsFactory;     // X360 +0x2260 (DWARF h:370, Handle<AemsFactory>)
    Handle<Factory>      mhSplicerFactory;  // X360 +0x2264 (DWARF h:374, Handle<SplicerFactory>)
    EA::Thread::Mutex    mStreamMutex;      // X360 +0x2268 (DWARF h:379)
    CgsModule::VariableEventQueue<1024, 16>
                         mDeferredResourceRequestQueue; // X360 +0x2298 (DWARF h:380, the
                                            //  ResourceRequestQueue<1024> typedef; Update drains
                                            //  it into the output buffer's 4096 queue)
    StreamBuffer         maStreamBuffers[3]; // X360 +0x26A8 (DWARF h:465, 24B records)
    u32                  muStreamBufferSize; // X360 +0x26F0 (DWARF h:466)
    u32                  muStreamNumBlocks;  // X360 +0x26F4 (DWARF h:467)
    bool                 mbStreamsUsingMainAllocator; // X360 +0x26F8 (DWARF h:468; word-stored)
    StringTable*         mpStringTable;     // X360 +0x26FC (DWARF h:489)
    f32                  mf32TimeStep;      // X360 +0x2700 (DWARF h:491; Environment::Update's dt)
    f32                  mf32TotalTime;     // X360 +0x2704 (DWARF h:492)
    s32                  mi32PoolId;        // X360 +0x2708 (DWARF h:494; Construct's module id)
};

// @ 0x82681C70. Post-increment the prepare-stage enum (stored unconditionally,
// before the bound assert -- E_PREPARESTAGE_DONE == 4, distinct from the
// CgsSound::Logic operator++ bound of 6); returns the saved OLD stage.
Module::EPrepareStage operator++(Module::EPrepareStage& leEnumIndex, int);

// @ 0x82681CD0 (the Release-machine sibling; the case-4 rung of Release inlines
// it with the .h:501 assert "leEnumIndex <= Module::E_RELEASESTAGE_DONE", bound
// 6). Post-increment the release-stage cursor; returns the saved OLD stage.
Module::EReleaseStage operator++(Module::EReleaseStage& leEnumIndex, int);

} // namespace Module
} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULE_H
