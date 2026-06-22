// Embed check for CgsFile.h: exercises the boot-trace ledger function plus the inline
// lock-guarded accessor surface of BaseFile.
//   CgsFileSystem::BaseFile::GetStatus @ 0x8264AD10
#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"

namespace CgsFileSystem
{
    // BaseFile's members are protected; a tiny derived probe drives the public inline API.
    struct BaseFileEmbedProbe : public BaseFile
    {
        void Exercise()
        {
            volatile FileState leStatus = GetStatus();          // @0x8264AD10
            volatile u64       luSize   = GetLastOperationSize();
            volatile s32       liPrio   = GetPriority();
            SetPriority(3);
            IncreasePendingOperations();
            (void)leStatus; (void)luSize; (void)liPrio;
        }
    };
}

extern void CgsFile_embed_check_anchor(CgsFileSystem::BaseFileEmbedProbe&);
void CgsFile_embed_check_anchor(CgsFileSystem::BaseFileEmbedProbe& lrProbe)
{
    lrProbe.Exercise();
}
