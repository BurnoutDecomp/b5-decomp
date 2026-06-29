#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoaderModule.h" // canonical BundleLoaderModule home
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ======================================================================================
// CgsResource::BundleLoaderModule -- the ASYNC STREAMING STATE MACHINE
// (reconstructed-surface from BURNOUT_X360_ARTIST.XEX, source CgsBundleLoaderModule.cpp).
//
// This is the CgsBundleLoaderModule.cpp ledger TU. The BundleLoaderModule class has a single
// canonical home in the sibling CgsResourceBundleLoaderModule.h, which also owns the rw/file-
// INDEPENDENT spine (ctor / Prepare / Release / Destruct + the focused synchronous load/unload
// path) in the sibling CgsResourceBundleLoaderModule.cpp. The functions DEFINED here are the
// remaining per-frame ASYNC streaming FSM: the UpdateStream dispatcher, its StreamXxxFunc step
// handlers, the ProcessBundleEntryList / ProcessPoolResponses event paths, the SendPartialFixup
// request, and the MoveToFirst/NextResource cursor helpers. They are DECLARED on the class in the
// canonical header (this TU does NOT redeclare the class); this file only DEFINES them.
//
// THIS TU's functions (X360 addresses), with one-line recovered behaviour:
//   MoveToFirstResource            0x828D7DA8  reset the (memType,entry) cursor to entry 0 of memType 0;
//                                              if that slot has disk data + a live stream-index byte,
//                                              accept it, else MoveToNextResource.
//   MoveToNextResource             0x828D7CE0  advance the cursor (entry++, wrap to next memType at the
//                                              per-memType entry count, stop after E_MEMTYPE_NUMTYPES);
//                                              accept the first slot whose size-and-alignment word's low
//                                              28 bits != 0 and whose stream-index byte is live.
//   Prepare                        0x828E2678  (HOMED in the sibling .cpp -- the rw/file-independent
//                                              stage machine; NOT redefined here.)
//   Release                        0x828E27A0  (HOMED in the sibling .cpp; NOT redefined here.)
//   ProcessBundleEntryList         0x828FAF58  upper-case the bundle filename, build the 48-byte pool
//                                              CreateBank event (id 16) from the running-load record +
//                                              the bundle header, hash the bundle name (or the live-update
//                                              sentinel), and post it to the loader output queue.
//   ProcessPoolResponses           0x828EC148  drain the pool->loader response queue (VEQ<4096,16> at
//                                              +176260): on a CreateBank-done (id 17) MoveToFirstResource
//                                              + (compressed) begin the decompression stream and go to
//                                              state 5, else state 6; on a fixup-done (id 19, not while
//                                              still in DATA state) record the loaded-bundle info into the
//                                              first free bundle slot and go to state 9.
//   SendPartialFixupRequest        0x828FBFC0  if more disk data has been streamed than already fixed up,
//                                              post a partial-fixup event (id 18, 40 bytes) for the delta.
//   StreamClose                    0x828FB260  post the close event (id 18, 12 bytes) for this stream slot
//                                              and free the slot's file handle.
//   StreamCompressedDataAsJobFunc  0x82900FA0  pump compressed disk data through the decompression job:
//                                              wait/flush jobs, partial-fixup, then loop reading the disk
//                                              buffer and Append/Finish decompression entries per resource.
//   StreamDebugDataFunc            0x829043A0  read the bundle's debug-data block into the debug buffer.
//   StreamDoneFunc                 0x828FB178  post the final whole-resource fixup event (id 18, 40 bytes).
//   StreamHeaderFunc               0x829042A0  read the 40-byte BND2 header, validate the "bnd2" magic,
//                                              then ProcessBundleHeader.
//   StreamIdleFunc                 0x82900ED0  tick the load/unload priority queues, check for unloads,
//                                              then pop the next running-load (or check for new loads).
//   UpdateStream                   0x82906B30  the per-frame FSM dispatcher (PerfMon-bracketed): runs the
//                                              current state's StreamXxxFunc and advances on success
//                                              (idle->header->debug->entrylist->data->close->done->finished).
//
// ======================================================================================
// DEFER STATUS (honest, per AGENTS.md -- no fabricated bodies, no raw-offset pointer hacks):
//
//   Every one of these functions marches over the FULL (~180 KB) BundleLoaderModule object by raw
//   X360 byte/word offset, touching ~50 members that the canonical header has NOT yet laid out: the
//   per-frame stream state byte (+560), the BundleV2 header/entry pointers (+1608/+1616/+1620), the
//   (memType,entry,sub) resource cursor (+624/+628/+632), the running-load record (+1536..+1716), the
//   two file-stream slots (+1712 selector -> the StreamDeviceDiskRead* at a1[<sel>+426]), the
//   DecompressionJobInterface (+41472), the async-read scratch (+175232/+175236/+175240/+175241), the
//   loaded-bundle table (+41340/+41344/+41348), and the pool-response VEQ (+176260). Their member
//   TYPES are NOT recoverable from this dossier (Hex-Rays renders them all as int/_DWORD) and the
//   subsystems they drive -- the FileSystem async StreamDeviceDiskRead, the EA-Jobs-backed
//   DecompressionJobInterface, the BundleLoaderIO output queues -- are reconstructed by the
//   cgs-resource-pool group in separate passes.
//
//   Reconstructing these bodies by NAME therefore requires (a) inventing ~50 member names AND their
//   TYPES to grow the header, which AGENTS.md forbids ("Never fake a TYPE with a stub"), or (b) raw
//   *(int*)(this+off) offset hacks, which AGENTS.md also forbids. The rw/file-independent sibling pass
//   deliberately collapsed this whole async FSM to the synchronous BundleLoader leaf for exactly this
//   reason. So the bodies below are MARKED-DEFERRED inert stubs: they preserve the recovered
//   SIGNATURE + the documented behaviour above, but do NOT fabricate the offset logic or any of the
//   asm's literals. The full streaming-FSM body pass lands once the module layout + the FileSystem /
//   DecompressionJob / BundleLoaderIO subsystem types are reconstructed. (The asm asserts' source path
//   is CgsBundleLoaderModule.cpp; line numbers are recorded in the per-function comments above the asm
//   for that pass to consume.)
// ======================================================================================
namespace CgsResource
{
    // ---- cursor helpers (0x828D7DA8 / 0x828D7CE0) -------------------------------------------------
    // Advance the (memType, entryIndex) cursor over the bundle's per-pool ResourceEntry table, accepting
    // the next slot that has on-disk data (size-and-alignment low 28 bits != 0) and a live stream-index
    // byte. DEFERRED: needs the bundle pointer / per-memType entry counts / cursor members + the
    // BundleV2::ResourceEntry table walk (full module layout).
    bool BundleLoaderModule::MoveToFirstResource()
    {
        CGS_ASSERT(false, "BundleLoaderModule::MoveToFirstResource: deferred (streaming FSM layout pass)");
        return false;
    }

    bool BundleLoaderModule::MoveToNextResource()
    {
        CGS_ASSERT(false, "BundleLoaderModule::MoveToNextResource: deferred (streaming FSM layout pass)");
        return false;
    }

    // ---- event paths (0x828FAF58 / 0x828EC148) ---------------------------------------------------
    // ProcessBundleEntryList: marshal the 48-byte CreateBank event (queue id 16) from the running-load
    // record + bundle header and post it to the loader output queue (BundleLoaderIO::OutputBuffer::GetPool
    // -> VariableEventQueue<4096,16>::AddEvent). DEFERRED: needs the running-load record + output queue.
    void BundleLoaderModule::ProcessBundleEntryList(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::ProcessBundleEntryList: deferred (streaming FSM layout pass)");
    }

    // ProcessPoolResponses: drain the pool->loader response queue (VEQ<4096,16> at +176260), reacting to
    // CreateBank-done (id 17) and fixup-done (id 19) responses. DEFERRED: needs the response queue + the
    // loaded-bundle table + the decompression-stream begin.
    void BundleLoaderModule::ProcessPoolResponses()
    {
        CGS_ASSERT(false, "BundleLoaderModule::ProcessPoolResponses: deferred (streaming FSM layout pass)");
    }

    // ---- streaming step handlers (the UpdateStream dispatch cases) --------------------------------
    // SendPartialFixupRequest: when more disk data has streamed than has been fixed up, post a partial
    // fixup event (id 18, 40 bytes) for the delta. DEFERRED: needs the running-load record + output queue.
    void BundleLoaderModule::SendPartialFixupRequest(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::SendPartialFixupRequest: deferred (streaming FSM layout pass)");
    }

    // StreamClose: post the per-slot close event (id 18, 12 bytes) and free the slot's file handle.
    // DEFERRED: needs the file-stream slot selector + the slot handle array + output queue.
    bool BundleLoaderModule::StreamClose(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamClose: deferred (streaming FSM layout pass)");
        return false;
    }

    // StreamCompressedDataAsJobFunc: pump compressed disk data through the DecompressionJobInterface
    // (wait/flush jobs, partial-fixup, then loop the disk buffer Append/Finish-ing decompression entries
    // per resource). DEFERRED: needs StreamDeviceDiskRead async reads + DecompressionJobInterface + cursor.
    bool BundleLoaderModule::StreamCompressedDataAsJobFunc(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamCompressedDataAsJobFunc: deferred (streaming FSM layout pass)");
        return false;
    }

    // StreamDebugDataFunc: read the bundle's debug-data block into the debug buffer (chunked
    // StreamDeviceDiskRead::Read). DEFERRED: needs the debug buffer + the file-stream slot.
    bool BundleLoaderModule::StreamDebugDataFunc()
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamDebugDataFunc: deferred (streaming FSM layout pass)");
        return false;
    }

    // StreamDoneFunc: post the final whole-resource fixup event (id 18, 40 bytes) for the streamed bundle.
    // DEFERRED: needs the running-load record + output queue.
    bool BundleLoaderModule::StreamDoneFunc(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamDoneFunc: deferred (streaming FSM layout pass)");
        return false;
    }

    // StreamHeaderFunc: read the 40-byte BND2 header off disk, validate the "bnd2" magic, then
    // ProcessBundleHeader. DEFERRED: needs the header buffer + the file-stream slot + ProcessBundleHeader.
    bool BundleLoaderModule::StreamHeaderFunc()
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamHeaderFunc: deferred (streaming FSM layout pass)");
        return false;
    }

    // StreamIdleFunc: tick the load + unload priority queues, check for pending unloads, then either pop
    // the next running-load off the queue or check for new loads. DEFERRED: needs the priority queues +
    // the running-load queue + CheckForLoads/CheckForUnloads.
    bool BundleLoaderModule::StreamIdleFunc(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::StreamIdleFunc: deferred (streaming FSM layout pass)");
        return false;
    }

    // ---- the FSM dispatcher (0x82906B30) ----------------------------------------------------------
    // UpdateStream: the per-frame, PerfMon-bracketed streaming state machine. It reads the current stream
    // state byte and runs that state's StreamXxxFunc, advancing on success through:
    //   0 idle -> 1 header -> 2 debug -> 3 entrylist -> 4 (await pool) -> 5 data ->
    //   6 close -> 7 done -> 8 (await fixup) -> 9 finished -> 0.
    // DEFERRED: dispatches over the deferred step handlers + the state byte + running-load record.
    void BundleLoaderModule::UpdateStream(void* /*lpOutputBuffer*/)
    {
        CGS_ASSERT(false, "BundleLoaderModule::UpdateStream: deferred (streaming FSM layout pass)");
    }
}
