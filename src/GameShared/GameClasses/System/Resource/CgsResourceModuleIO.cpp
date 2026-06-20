#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"  // the 3 real-shape structs
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (class:CgsResource::ResourceIO) -- the two CONST read-lock-gated getters:
//   CgsResource::ResourceIO::InputBuffer::GetResourceQueue()             const @ 0x828E1190
//   CgsResource::ResourceIO::OutputBuffer::GetFileSystemStatusInterface() const @ 0x82663BE0
// Each asserts the owning IOBuffer is read-locked, then returns the address of its embedded
// member (X360: this+4 for InputBuffer's queue, this+1 for OutputBuffer's status interface --
// matches the DWARF member offsets, see CgsResourceModuleIO.h).
//
// Also folded in here (already reconstructed earlier from the XEX):
//   CgsResource::ResourceIO::FileSystemStatusInterface::Construct @ 0x82676600
//   CgsResource::ResourceIO::OutputBuffer::Construct              @ 0x828EC978
//
// *** RECONCILIATION (FLAG) ***
// The previous revision of this .cpp defined PROVISIONAL inline structs that DRIFTED from the
// DWARF:
//   - FileSystemStatusInterface { u8 mbInUse; }          (Construct set mbInUse=0)
//   - OutputBuffer              { u8 mbOpen; u8 mbFull; } (Construct set mbOpen=1, mbFull=0)
// Those names/shapes are WRONG vs the DecFIGS DWARF and would ODR-collide with the real structs
// now declared in CgsResourceModuleIO.h, so they are REMOVED. The two Construct bodies are
// re-expressed against the REAL layout while preserving the exact BYTE behaviour the X360 wrote:
//   * OutputBuffer::Construct's old "mbOpen = 1" was the X360 writing the IOBuffer
//     eStatusConstructed bit (+0, bit0) -> now expressed as IOBuffer::Construct().
//   * The old "mbFull = 0" / FileSystemStatusInterface "mbInUse = 0" was the disk-error flag
//     being cleared (+1) -> now expressed as mFileSystemStatusInterface.Construct(), which
//     clears mbDiskErrorOccured. Net on-disk bytes are unchanged.

namespace CgsResource
{
    namespace ResourceIO
    {
        // -------- FileSystemStatusInterface::Construct @ 0x82676600 --------
        // Real layout (DWARF): single bool mbDiskErrorOccured @ +0. Clear it.
        void FileSystemStatusInterface::Construct()
        {
            mbDiskErrorOccured = false;
        }

        // -------- OutputBuffer::Construct @ 0x828EC978 --------
        // Construct the IOBuffer base (sets the eStatusConstructed status bit @ +0), then
        // construct the embedded FileSystemStatusInterface @ +1 (clears its disk-error flag).
        // Reproduces the prior provisional mbOpen=1/mbFull=0 bytes.
        void OutputBuffer::Construct()
        {
            CgsModule::IOBuffer::Construct();
            mFileSystemStatusInterface.Construct();
        }

        // -------- InputBuffer::GetResourceQueue() const @ 0x828E1190 (THIS TU) --------
        // Read-lock gate, then return the embedded request queue (X360 this+4).
        const ResourceRequestQueue<16384>* InputBuffer::GetResourceQueue() const
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
            return &mResourceQueue;
        }

        // -------- OutputBuffer::GetFileSystemStatusInterface() const @ 0x82663BE0 (THIS TU) --------
        // Read-lock gate, then return the embedded status interface (X360 this+1).
        const FileSystemStatusInterface* OutputBuffer::GetFileSystemStatusInterface() const
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
            return &mFileSystemStatusInterface;
        }
    }
}
