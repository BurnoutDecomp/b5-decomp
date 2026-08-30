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
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModuleIO.h" // the Io::InputBuffer/OutputBuffer pair (phase B4)
#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/CgsStreamingPlugin.h" // IStreamProvider (the +0x228 interface base)

#include "eathread/eathread_rwmutex.h"
#include "eathread/eathread_mutex.h"

namespace CgsMemory { class LinearMalloc; }   // Prepare's stream-buffer bump path

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
// (2026-08-25, faithful-audio-engine phase B4): the two interface bases at
// +0x228 / +0x22C are IDENTIFIED and REAL -- IStreamProvider (CgsStreamingPlugin.h;
// its DoOpenStream/DoCloseStream slots are the Module's stream-buffer service
// @0x826FA020/@0x826FA2B8, and the +0x228 sub-object is what the RWAC-stage
// off_82FFBA0C publish hands the SndPlayer1 side) and IContentLoadService
// (CgsContent.h:70; DoServiceContentLoadRequest @0x826F9F88, the sub-object
// CreateContent wires into each new content's mpLoadService). The former raw
// mpSecondaryVtbl/mpTertiaryVtbl slots are RETIRED: the console ctor's paired
// vtable installs (the intermediate off_820CE500/off_820CE350/off_820CE358 set
// overwritten by the final off_820CFA30/off_820CFA24/off_820CFA20 set) are
// exactly what the compiler emits for this base-then-derived MI construction.
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
class Module : public CgsModule::ModuleSingleBuffered,
               public IStreamProvider,      // console sub-object +0x228 (phase B4)
               public IContentLoadService   // console sub-object +0x22C (DWARF h:165, protected on X360)
{
public:
    // DWARF CgsSoundPlaybackModule.h:347 (nested). The prepare stage counter
    // (real enumerator names recovered phase B4; the operator++ below walks it,
    // bound E_PREPARESTAGE_DONE == 4).
    enum EPrepareStage
    {
        E_PREPARESTAGE_START       = 0,
        E_PREPARESTAGE_MANAGER     = 1,
        E_PREPARESTAGE_ENVIRONMENT = 2,
        E_PREPARESTAGE_FACTORIES   = 3,
        E_PREPARESTAGE_DONE        = 4
    };

    // DWARF CgsSoundPlaybackModule.h:355 (nested). The release COUNTDOWN cursor:
    // Construct seeds DONE(6) = nothing to release; each Prepare stage lowers it
    // to its own unwind rung (MANAGER/ENVIRONMENT/FACTORIES/START), and Release
    // runs FORWARD from it (@0x826C0FB8), bumping with the same +1/bound-6 idiom
    // the .h:501 assert names ("leEnumIndex <= Module::E_RELEASESTAGE_DONE").
    // (Real enumerator names recovered phase B4.)
    enum EReleaseStage
    {
        E_RELEASESTAGE_START          = 0,
        E_RELEASESTAGE_STRING_TABLE   = 1,
        E_RELEASESTAGE_FACTORIES      = 2,
        E_RELEASESTAGE_ENVIRONMENT    = 3,
        E_RELEASESTAGE_STREAM_BUFFERS = 4,
        E_RELEASESTAGE_MANAGER        = 5,
        E_RELEASESTAGE_DONE           = 6
    };

    // DWARF CgsSoundPlaybackModule.h:382.
    static const u8 SKU_NUMBER_OF_STREAM_BUFFERS = 3;

    // DWARF CgsSoundPlaybackModule.h:374-465 (nested; mStreamBuffers[3] @ console
    // +0x26A8, 24-byte records). One SndPlayer1 stream-buffer record + its status
    // machine (real field/enum names recovered phase B4):
    //   E_FREE_BUFFER --DoOpenStream--> E_USING_BUFFER (voice id + read-stream
    //   cleared, buffer handed out through the spec) --open response (receiver
    //   event 16)--> E_STREAM_OPEN --DoCloseStream--> E_WAITING_FOR_CLOSE (close
    //   request posted; or mbQueuedForClose when not yet open) --close response
    //   (receiver event 18; buffer memset 0xF0)--> E_WAITING_GRACE_PERIOD
    //   --UpdateStreamBuffers (stomp scan + one dt tick)--> freed (voice id
    //   appended to the output buffer's FreedBuffersArray) + E_FREE_BUFFER.
    struct StreamBuffer
    {
        // CgsCommon.h:91 (spelled through this nested typedef by the DWARF).
        typedef u32 Ident;

        // CgsSoundPlaybackModule.h:402.
        enum EStreamBufferStatus
        {
            E_USING_BUFFER         = 0,
            E_STREAM_OPEN          = 1,
            E_WAITING_FOR_CLOSE    = 2,
            E_WAITING_GRACE_PERIOD = 3,
            E_FREE_BUFFER          = 4
        };

        // The DWARF accessor surface; trivial field access, inline (the X360
        // inlines every one at its call sites -- e.g. the .h:651
        // "E_USING_BUFFER == GetStatus()" and .h:659 "!mbQueuedForClose"
        // asserts read through GetStatus/GetQueuedForClose).
        void* GetBuffer() const                        { return mpBuffer; }        // h:397
        Ident GetVoiceId() const                       { return mVoiceId; }        // h:400
        EStreamBufferStatus GetStatus() const          { return mBufferStatus; }   // h:420
        CgsFileSystem::ReadStream& GetReadStream()     { return mReadStream; }     // h:423
        bool  GetQueuedForClose() const                { return mbQueuedForClose; }// h:430
        void  SetStatus(EStreamBufferStatus leStatus)  { mBufferStatus = leStatus; } // h:435
        void  SetReadStream(const CgsFileSystem::ReadStream& lrReadStream)          // h:443
        {
            mReadStream = lrReadStream;
        }
        void  SetQueuedForClose()                      { mbQueuedForClose = true; } // h:450

        // h:387 -- reset to the at-rest record (the Module::Construct seed:
        // {0, E_FREE_BUFFER, 0, 0, 0, 0.0}).
        void Construct()
        {
            mpBuffer         = 0;
            mBufferStatus    = E_FREE_BUFFER;
            mReadStream      = CgsFileSystem::ReadStream();
            mVoiceId         = 0;
            mbQueuedForClose = false;
            mfGraceWaitTime  = 0.0f;
        }

        // h:391 / h:394 / h:414 / h:417 -- their own ledger surface (the buffer
        // adopt/release slices); declared for the DWARF method set.
        bool Prepare(void* lpBuffer);
        bool Release();
        void AquireBuffer(Ident lVoiceId);   // (DWARF spelling)
        void ReleaseBuffer();

        // The Module's service bodies write these directly (member access within
        // the enclosing class on the X360; kept public to the enclosing TU via
        // the accessor set above plus these fields -- the DWARF marks them
        // private to StreamBuffer, whose only writers ARE the Module methods).
        void*                     mpBuffer;         // h:455  +0x00  the carved stream buffer
        EStreamBufferStatus       mBufferStatus;    // h:456  +0x04
        CgsFileSystem::ReadStream mReadStream;      // h:457  +0x08  (by-value one-pointer handle)
        Ident                     mVoiceId;         // h:458  +0x0C
        bool                      mbQueuedForClose; // h:459  +0x10
        f32                       mfGraceWaitTime;  // h:462  +0x14
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

    // Module::Prepare @ 0x826E90C0 (virtual -- the console vtable slot the
    // RootSoundModule PLAYBACK_MODULE Prepare stage dispatches; phase B3). The
    // 5-rung prepare stage machine: base+queues (1), Environment::Create with
    // the DWARF-constant spec (2), factories + stream buffers + the 8
    // environment CPU monitors (3), done (4). Returns false while still
    // preparing. The stage-3 factory creates are DEFERRED (the AEMS keystone --
    // see the .cpp banner); the handles stay null until that slice lands.
    virtual bool Prepare(rw::IResourceAllocator* apAllocator,
                         CgsMemory::LinearMalloc* apLinearMalloc);

    // Module::Update @ 0x826E9700 (virtual -- the per-frame pump; phase B4).
    // Bracketed by the environment's Playback/Environment CPU monitors: attach +
    // lock the buffer pair, drain the deferred resource requests into the output
    // buffer's request queue under the stream mutex, tick the stream-buffer
    // state machine into the output buffer's freed list, process the resource
    // receiver queue, then Environment::Update(mf32TimeStep) and teardown.
    virtual void Update(Io::InputBuffer* apInputBuffer, Io::OutputBuffer* apOutputBuffer);

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

    // The per-frame time pair (phase C2): the RootSoundModule::Update @0x826FB238
    // paired store at +0x2700/+0x2704 -- mf32TimeStep = dt (the dt this module's
    // Update hands Environment::Update) and mf32TotalTime += dt (read-before-store
    // on the console). Additive by-name access over the DWARF h:491/:492 fields.
    void AdvanceTime(f32 af32TimeStep)
    {
        mf32TimeStep   = af32TimeStep;
        mf32TotalTime += af32TimeStep;
    }

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
    void ImportStringTable(const Registry& arRegistry);

    // @0x826C7BE8. Fix, self-resolve and merge a loaded serialized registry into
    // the environment (1), AEMS (2), or generic-RWAC (3) factory registry.
    void AddRegistry(Registry& arRegistry, u32 au32RegistryId);

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

    // ---- the IContentLoadService override (console sub-object +0x22C) ----
    // @ 0x826F9F88 (DWARF cpp:1167). Service a content load request: for the
    // RESOURCE_MODULE method, post a resource request (receiver route =
    // &mResourceReceiverQueue, the user data, mi32PoolId, the hashed name) into
    // the attached output buffer's request queue (event type 4); any other
    // method is refused (returns false).
    virtual bool DoServiceContentLoadRequest(u32 lu32Ident, EContentLoadMethod leMethod,
                                             const char* lpcName, void* lpUserData) override;

    // ---- the IStreamProvider overrides (console sub-object +0x228) ----
    // @ 0x826FA020 (DWARF cpp:1200). Claim the first E_FREE_BUFFER stream record
    // (assert "We've run out of Audio Stream Buffers." cpp:1219 otherwise),
    // resolve the requesting plug-in's content (Environment::GetR, assert
    // "lHandle" cpp:1227), seed the record (E_USING_BUFFER, the content's ident
    // as voice id) and hand its carve buffer back through the spec, then post
    // the OpenReadStreamRequest (event type 16, assert cpp:1240) into the
    // deferred queue. Returns &record.mReadStream (a pointer INTO the record
    // table -- what FindStreamBuffer/DoCloseStream key on), or null when no
    // buffer is free. All under the stream mutex.
    virtual CgsFileSystem::ReadStream* DoOpenStream(IStreamProvider::StreamSpec& lrSpec) override;

    // @ 0x826FA2B8 (DWARF cpp:1257). Close a stream by record pointer: find the
    // record (assert "Can't Find this Stream Index" cpp:1262); when the stream
    // is E_STREAM_OPEN, post the CloseReadStreamRequest (event type 18, assert
    // cpp:1278) and advance to E_WAITING_FOR_CLOSE; otherwise the stream is
    // still opening (assert E_USING_BUFFER cpp:1268 + !mbQueuedForClose .h:659)
    // -- queue the close for UpdateStreamBuffers to re-dispatch once open. All
    // under the stream mutex.
    virtual void DoCloseStream(const CgsFileSystem::ReadStream* lpReadStream) override;

private:
    // @ 0x826A25C0 (DWARF cpp:~579, `using namespace CgsResource::ResourceIO`).
    // Drain mResourceReceiverQueue: event 4 = content-load response (bind the
    // resource memory via BaseResourcePtr::CreateFromHandle, assert cpp:598);
    // event 16 = stream-open response (record.mReadStream = the opened stream,
    // E_USING_BUFFER -> E_STREAM_OPEN, asserts cpp:611/.h:651) and event 18 =
    // stream-close response (memset the buffer 0xF0, E_WAITING_FOR_CLOSE ->
    // E_WAITING_GRACE_PERIOD, asserts cpp:632/:633), both under the RWAC system
    // lock + the stream mutex; anything else asserts ("Unanticiapated Resource
    // Response. Eek.\n" cpp:652 -- the typo is the X360's). Then Clear.
    void ProcessResourceReceiverQueue();

    // @ 0x826A28E8 (DWARF cpp:1366). Tick the 3 stream-buffer records:
    // re-dispatch a queued close once its stream opens (the virtual
    // DoCloseStream through the IStreamProvider base -- the console `vtbl+4` on
    // the +0x228 sub-object); for E_WAITING_GRACE_PERIOD records, accumulate
    // mf32TimeStep, scan the 0xF0-scrubbed buffer for stomps (assert
    // "STOMP OCCURRED" cpp:1395), and after the first tick append the voice id
    // to the output buffer's freed list and reset the record to E_FREE_BUFFER.
    void UpdateStreamBuffers(Io::OutputBuffer::FreedBuffersArray& arFreedBuffers);

    // @ 0x82689CE0 (DWARF cpp:1424). Map a ReadStream record pointer (the
    // DoOpenStream return) back to its stream-buffer index; -1 when it is not
    // one of the 3 records (assert "0 != lpReadStream" cpp:1426).
    s32 FindStreamBuffer(const CgsFileSystem::ReadStream* lpReadStream);

    // Full DWARF member list (phase B1; the base ModuleSingleBuffered owns the
    // console +0x00..+0x227 span -- vtable + the two RWMutexes + the DataBuffer
    // pair). The +0x228/+0x22C interface bases are the REAL IStreamProvider /
    // IContentLoadService bases since phase B4 (see the banner). The DWARF
    // handle types are per-factory subclasses (GenericRwac/Aems/Splicer); held
    // as Handle<Factory> until those subclass homes are includable here -- the
    // ledgered AEMS keystone.
    EPrepareStage        mePrepareStage;    // X360 +0x0230  (DWARF h:354)
    EReleaseStage        meReleaseStage;    // X360 +0x0234  (DWARF h:355; the countdown cursor)
    CgsModule::EventReceiverQueue<0x2000, 16>
                         mResourceReceiverQueue; // X360 +0x0238 hdr + 0x0250 storage (DWARF h:357,
                                            //  EventReceiverQueue<8192,16>; Construct binds cap
                                            //  0x2000 / align 16 / storage this+0x250 + Clear)
    Io::InputBuffer*     mpInputBuffer;     // X360 +0x2250 (DWARF h:359)
    Io::OutputBuffer*    mpOutputBuffer;    // X360 +0x2254 (DWARF h:360)
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
