#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"

// ===========================================================================
//  NFSMixMaster -- ctor/dtor bodies, store-for-store from BURNOUT_X360_ARTIST.XEX.
//  See NFSMixMaster.hpp for the layout rationale (ARTIST-verified).
// ===========================================================================

// off_83250008 -- the process-wide NFSMixMaster singleton pointer. The ctor stores
// `this` here; the dtor clears it. Modelled as a TU-local global (the only writers
// are these two bodies in the ARTIST asm).
NFSMixMaster* g_pNFSMixMaster = 0;

// ---------------------------------------------------------------------------
// NFSMixMaster::NFSMixMaster @0x82B45740
//   off_83250008 = this;                       (install the singleton)
//   for (i=0; i<25; ++i) m_StateRefCount[i]=0; (addi r11,r3,0xC; bdnz x0x19)
//   m_pMainMixMapData = 0;  (+0x04)   m_pMainMixMap = 0;  (+0x00)
//   mNumStates = 0;         (+0x08)   m_bMapReady = 0;    (+0x70, stb)
//   m_pMixMaster = 0;       (+0x7C)
// (the X360 leaves +0x74 m_pSBActiveMasks and +0x78 m_LoadMapID untouched here;
//  CreateMainMainMap sets +0x74, and the load path sets +0x78.)
// ---------------------------------------------------------------------------
NFSMixMaster::NFSMixMaster()
{
    g_pNFSMixMaster = this;

    for (int li = 0; li < 25; ++li)
        m_StateRefCount[li] = 0;

    m_pMainMixMapData = 0;
    m_pMainMixMap     = 0;
    mNumStates        = 0;
    m_bMapReady       = false;
    m_pMixMaster      = 0;
}

// ---------------------------------------------------------------------------
// NFSMixMaster::~NFSMixMaster @0x82B45780
//   off_83250008 = 0;                          (clear the singleton)
//   if (m_pMainMixMap) { (*vt[0])(m_pMainMixMap, 1); m_pMainMixMap = 0; }
//     -- the X360 calls the main map's scalar-deleting destructor (vtable slot 0,
//        deleting-flag = 1).
// FLAG: the virtual destruction of m_pMainMixMap is deferred -- NFSMixMap is only
// forward-declared in this slice (it is the 564-byte processor homed in a follow-on),
// so `delete`/the virtual-dtor call cannot be expressed on the incomplete type yet.
// The faithful effect (singleton cleared, pointer nulled) is reproduced; the map
// teardown is wired in when NFSMixMap lands.
// ---------------------------------------------------------------------------
NFSMixMaster::~NFSMixMaster()
{
    g_pNFSMixMaster = 0;

    if (m_pMainMixMap)
    {
        // (*m_pMainMixMap->vtable[0])(m_pMainMixMap, /*deleting=*/1);  // pending NFSMixMap
        m_pMainMixMap = 0;
    }
}
