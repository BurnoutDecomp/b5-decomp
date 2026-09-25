#pragma once

// ============================================================================
// GameSource/Director/Utils/BrnPostOffice.h
//
// BrnDirector::PostOffice<T, N> (DWARF BrnPostOffice.h:43) -- the director's per-query-kind
// hand-off between the scene-query PRODUCER (which only sees a 16-bit query id) and the
// PostBox<T> that is waiting for the answer. The director module owns six of them
// (BrnDirectorModule.h, +0x9A4 .. +0xAFF on the console) and publishes their addresses in the
// per-frame BrnDirector::SceneQueryInterface; SceneQueryInterface::LineTestNearest & co. mint
// an id here (AddPostBox) and DirectorModule::ProcessSceneQueryResults routes every result
// back through Deliver.
//
// LAYOUT (DWARF): one member, Array<PostBox<T>*, N> mPostBoxMap (h:68). The console
// strides it as N 4-byte pointers + the length word (the LineTestNearest office's
// `lwz 0xA0` = 40 * 4, the ten-slot offices' `+0x28`, the one-slot office's `+0x04`); on this
// host the pointers are 8 bytes and parity is by named member.
//
// BODIES (the X360 emits one copy per instantiation; the PS3 names them):
//   Construct   PS3 @0xA1AFC   mPostBoxMap.Construct(); Clear()
//   Clear       PS3 @0xA1AF0   mPostBoxMap.Clear()  (X360: SceneQueryInterface::Clear
//               @0x8221CD38 inlines it as `stw 0` to the length word)
//               -- the <const OutEventLineTestFineResult*, 10> office is SPECIALISED
//               (PS3 @0x2CEA8, X360 sub_8221CC98 @0x8221CC98): a delivered box holds a
//               POINTER into this frame's results queue, so before forgetting its boxes the
//               office empties every one that GOT its package.
//   AddPostBox  X360 0x8222D288 (the LineTestNearest instantiation):
//               assert GetLength() < GetCapacity()   (BrnPostOffice.h:78)
//               assert !Contains(&lPostBox)          (BrnPostOffice.h:79)
//               lPostBox.WaitForPackage(); mPostBoxMap.Append(&lPostBox);
//               return GetLength() - 1               (the 16-bit PostBoxIDIntType, h:37)
//   Deliver     X360 0x8222D3B8 (nearest) / 0x82210A08 (fine) / 0x8222D488 / 0x8222D558 /
//               0x8222D758 / 0x8222D828:
//               assert lID < GetLength()            (BrnPostOffice.h:95)
//               assert mPostBoxMap[lID] != NULL      (BrnPostOffice.h:96)
//               mPostBoxMap[lID]->TakePackage(lrPackage)
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"     // Array<T,N> (global scope)
#include "GameSource/Director/Utils/BrnPostBox.h"           // BrnDirector::PostBox

namespace CgsSceneManager { namespace SceneManagerIO { struct OutEventLineTestFineResult; } }

namespace BrnDirector
{
    template <class Type, u32 N>
    class PostOffice
    {
    public:
        typedef u16 PostBoxIDIntType;   // h:37

        void Construct()                                                             // h:47
        {
            mPostBoxMap.Construct();
            Clear();
        }

        void Clear()                                                                 // h:50
        {
            mPostBoxMap.Clear();
        }

        PostBoxIDIntType AddPostBox(PostBox<Type>& lPostBox)                         // h:76
        {
            CGS_ASSERT(mPostBoxMap.GetLength() < mPostBoxMap.GetCapacity(),
                       "mPostBoxMap.GetLength() < mPostBoxMap.GetCapacity()");      // h:78
            CGS_ASSERT(!mPostBoxMap.Contains(&lPostBox), "!mPostBoxMap.Contains(&lPostBox)");  // h:79
            lPostBox.WaitForPackage();
            mPostBoxMap.Append(&lPostBox);
            return static_cast<PostBoxIDIntType>(mPostBoxMap.GetLength() - 1);
        }

        void Deliver(u16 lID, const Type& lrPackage)                                 // h:93
        {
            CGS_ASSERT(lID < mPostBoxMap.GetLength(), "lID < mPostBoxMap.GetLength()");  // h:95
            CGS_ASSERT(mPostBoxMap[lID] != 0, "mPostBoxMap[lID] != NULL");              // h:96
            mPostBoxMap[lID]->TakePackage(lrPackage);
        }

        u32 GetLength() const { return mPostBoxMap.GetLength(); }   // [PC accessor for tests]

    private:
        ::Array<PostBox<Type>*, N> mPostBoxMap;   // h:68 (CgsArray.h declares Array at global scope)
    };

    // The fine-line-test office's Clear (PS3 @0x2CEA8, X360 sub_8221CC98 -- also called by
    // DirectorModule::Construct): each held box that GOT its package is emptied first, because
    // the package is a pointer into a results queue that dies with the frame. The console loop
    // re-reads the length every iteration (`lwz 0x28` in the loop head) and leaves boxes still
    // WAITING untouched.
    template <>
    inline void PostOffice<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*, 10u>::Clear()
    {
        for (u32 luBox = 0; luBox < mPostBoxMap.GetLength(); ++luBox)
        {
            if (mPostBoxMap[luBox]->GetState() ==
                PostBox<const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult*>::E_STATE_GOT_PACKAGE)
            {
                mPostBoxMap[luBox]->Clear();
            }
        }
        mPostBoxMap.Clear();
    }
}
