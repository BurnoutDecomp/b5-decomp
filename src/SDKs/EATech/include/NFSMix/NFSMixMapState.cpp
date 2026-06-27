#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"

// ===========================================================================
//  NFSMixMapState -- ctor/dtor + the ARTIST-verified Initialize / GetStateRefCount.
//  Store-for-store from BURNOUT_X360_ARTIST.XEX (see NFSMixMapState.hpp).
// ===========================================================================

NFSMixMapState::NFSMixMapState() {}   // vtable install (reproduced by the virtual dtor)
NFSMixMapState::~NFSMixMapState() {}  // vtable slot 0

// ---------------------------------------------------------------------------
// NFSMixMapState::Initialize @0x82B4C6B0 -- Initialize(this, lpMap, stateIdx, copies, objIdx)
//   m_pNFSMixMap = lpMap;        (+0x04)   m_StateIndex     = stateIdx; (+0x38)
//   m_NumStateCopies = copies;   (+0x44)   m_ObjectIndex    = objIdx;   (+0x40)
//   if (objIdx == 0) { m_ThisStateRefCnt = 1; return; }   (+0x3C)
//   else { ++m_pFirstInstance->m_ThisStateRefCnt;          (the "master" copy)
//          for (i=0; i<objIdx; ++i)                          (propagate to all copies,
//              m_pFirstInstance[i].m_ThisStateRefCnt = m_pFirstInstance->m_ThisStateRefCnt; }
//                                                            X360 instance stride 0x60)
// ---------------------------------------------------------------------------
void NFSMixMapState::Initialize(NFSMixMap* lpMap, int liStateIndex,
                                int liNumStateCopies, int liObjectIndex)
{
    m_pNFSMixMap     = lpMap;            // +0x04
    m_StateIndex     = liStateIndex;    // +0x38
    m_NumStateCopies = liNumStateCopies;// +0x44
    m_ObjectIndex    = liObjectIndex;   // +0x40

    if (liObjectIndex == 0)
    {
        m_ThisStateRefCnt = 1;          // +0x3C
        return;
    }

    // A copy: bump the master instance's ref-count, then stamp it onto every copy.
    ++m_pFirstInstance->m_ThisStateRefCnt;
    for (int li = 0; li < liObjectIndex; ++li)
        m_pFirstInstance[li].m_ThisStateRefCnt = m_pFirstInstance->m_ThisStateRefCnt;
}

// ---------------------------------------------------------------------------
// NFSMixMapState::GetStateRefCount @0x82B4C718
//   return m_pFirstInstance->m_ThisStateRefCnt;   (lwz 0x5C ; lwz 0x3C)
// ---------------------------------------------------------------------------
int NFSMixMapState::GetStateRefCount()
{
    return m_pFirstInstance->m_ThisStateRefCnt;
}
