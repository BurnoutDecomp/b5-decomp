// Explicit instantiation home for the LinearSOAHashTable<int,int> (KeyType=int) methods.
// Bodies are inline in CgsLinearSOAHashTable.h (mirroring the AOS sibling LinearHashTable in
// CgsLinearHashTable.h, which keeps AddEntry/FindEntry inline in its header). This TU forces the
// per-instantiation out-of-line emission that the X360 build produced at 0x828DF260 (AddEntry)
// and 0x828DF3E0 (FindEntry) -- the PatchManager path-hash map (its call sites AddFileToCurrentPatch
// / RemapFilename).
//
// AddEntry @0x828DF260 store-for-store (base a1 = unsigned int*, all displacements are true byte
// offsets since KeyType=int is 4 bytes):
//   assert  a2(key) == [a1+4](miInvalidKey)  -> CGS_ASSERT(key != miInvalidKey, "...table\n")
//   r9=[a1+0]=len ; start = key - (key/len)*len = key % len                (divwu/mullw/subf)
//   phase1 [start,len): r7=[a1+8]=keys ; probe keys[pos] vs [a1+4]=invalidKey ; ++pos,+=4
//     store: keys[pos]=key (stwx a2,keys,pos*4) ; if a3: values[pos]=*a3 (values=[a1+0xC])
//            return &values[pos] = [a1+0xC] + 4*pos
//   phase2 (LABEL_7): pos=0 ; if start==0 return 0 ; probe keys[0..start) ; same store ; else 0
//
// FindEntry @0x828DF3E0 store-for-store (base a1 = unsigned int*, KeyType=int, 4-byte offsets):
//   r8=[a1+0]=len ; start = key % len
//   phase1 [start,len): r10=[a1+8]=keys ; == key -> hit: values=[a1+0xC], return + 4*pos ; ==
//     invalidKey -> return 0 ; else ++pos,+=4 ; wrap to phase2 at len ; NO assert.
#include "GameShared/GameClasses/Containers/CgsLinearSOAHashTable.h"

namespace CgsContainers
{
    // Force emission of the <int,int> AddEntry/FindEntry bodies (X360 0x828DF260 / 0x828DF3E0).
    template int* LinearSOAHashTable<int, int>::AddEntry(int, const int*);
    template int* LinearSOAHashTable<int, int>::FindEntry(int);
}
