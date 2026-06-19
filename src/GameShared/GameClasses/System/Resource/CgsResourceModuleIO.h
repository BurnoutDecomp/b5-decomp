#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                       // CgsModule::IOBuffer base (+ protected IsBufferLockedForReading)
#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h"  // CgsResource::ResourceIO::ResourceRequestQueue<N>

// CgsResource::ResourceIO -- the per-frame resource-IO payload buffers exchanged between the
// game-data module and the resource module. Layout + members recovered from the DecFIGS DWARF
// (CgsResourceModuleIO.h):
//   FileSystemStatusInterface  -- 1-byte disk-error status block.
//   InputBuffer  : CgsModule::IOBuffer  -- carries the ResourceRequestQueue<16384> the game
//                                          module fills and the resource module reads.
//   OutputBuffer : CgsModule::IOBuffer  -- carries the FileSystemStatusInterface the resource
//                                          module writes back.
// Both buffers gate their const getters on the IOBuffer read lock (asserts at
// CgsResourceModuleIO.h:80 / :110 in the X360 build).
namespace CgsResource
{
    namespace ResourceIO
    {
        // CgsResourceModuleIO.h:46 -- 1-byte disk-error status block.
        struct FileSystemStatusInterface
        {
            void Construct();
            bool GetDiskErrorOccured() const;
            void SetDiskErrorOccured(bool lbDiskErrorOccured);

        private:
            bool mbDiskErrorOccured;   // CgsResourceModuleIO.h:56  [+0]
        };

        // CgsResourceModuleIO.h:68 -- input payload buffer (game module -> resource module).
        struct InputBuffer : public CgsModule::IOBuffer
        {
            void Construct();
            void Destruct();

            const ResourceRequestQueue<16384>* GetResourceQueue() const;   // CgsResourceModuleIO.h:80 (THIS TU)
            ResourceRequestQueue<16384>*       GetResourceQueue();          // CgsResourceModuleIO.h:81 (deferred)
            void AppendResourceQueue(const ResourceRequestQueue<16384>* lpResourceQueue);  // :82 (deferred)

        private:
            // CgsResourceModuleIO.h:86. Lands at +4: after the 1-byte IOBuffer base the
            // 4-aligned queue starts at offset 4 (3 bytes of base padding).
            ResourceRequestQueue<16384> mResourceQueue;
        };

        // CgsResourceModuleIO.h:98 -- output payload buffer (resource module -> game module).
        struct OutputBuffer : public CgsModule::IOBuffer
        {
            void Construct();
            void Destruct();

            const FileSystemStatusInterface* GetFileSystemStatusInterface() const;  // CgsResourceModuleIO.h:110 (THIS TU)
            FileSystemStatusInterface*       GetFileSystemStatusInterface();         // CgsResourceModuleIO.h:111 (deferred)

        private:
            // CgsResourceModuleIO.h:115. Lands at +1: directly after the 1-byte IOBuffer base.
            FileSystemStatusInterface mFileSystemStatusInterface;
        };
    }
}
