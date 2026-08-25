#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"        // complete type
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"   // off_83250004 mixer allocator
#include <new>                                              // placement new

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
//        deleting-flag = 1), i.e. `delete m_pMainMixMap`. NFSMixMap is now homed (with
//        a virtual dtor), so the faithful virtual delete is expressed directly.
// ---------------------------------------------------------------------------
NFSMixMaster::~NFSMixMaster()
{
    g_pNFSMixMaster = 0;

    if (m_pMainMixMap)
    {
        // (*vt[0])(m_pMainMixMap, /*deleting=*/1) == the NFSMixMap vector-deleting dtor
        // @0x82B4C1F0: run ~NFSMixMap then Free the block through the mixer allocator
        // (off_83250004 vtable+0xC). Reproduced explicitly.
        m_pMainMixMap->~NFSMixMap();
        g_pMixerAllocator->Free(m_pMainMixMap, 0);
        m_pMainMixMap = 0;
    }
}

// ---------------------------------------------------------------------------
// NFSMixMaster::CreateMainMainMap @0x82B45868 -- CreateMainMainMap(this, a2, a3)
//   m_pMixMaster = this;        (+0x7C, stw r30)
//   m_bMapReady  = 0;           (+0x70, stb)
//   r3 = mixerSystem->Allocate(0x234, 0, "NFSMixMap");   (off_83250004 vtable+8)
//   if (r3) NFSMixMap::NFSMixMap(r3);                     (placement ctor)
//   m_pMainMixMap = r3;         (+0x00)
//   NFSMixMap::Init(r3, this);                            (Init(map, this))
//   m_pSBActiveMasks  = a3;     (+0x74)
//   m_pMainMixMapData = a2;     (+0x04)
//   return r3;                  (the new map)
//
// FLAG: the X360 allocates the 0x234-byte NFSMixMap from the EATech mixer-system heap
// (off_83250004, a mixer System singleton; vtable slot 2 = Allocate). PC uses `new`
// (allocate + ctor -- the faithful effect); it will route through MixerMemBase's
// mixer-heap operator new once that allocator is wired. Marked PC-alloc boundary.
// ---------------------------------------------------------------------------
NFSMixMap* NFSMixMaster::CreateMainMainMap(int* lpMapData, int* lpSBActiveMasks)
{
    m_pMixMaster = this;        // +0x7C
    m_bMapReady  = false;       // +0x70

    // X360: mixerSystem->Allocate(0x234, 0, "NFSMixMap") + placement ctor. On PC we
    // allocate sizeof(NFSMixMap) (the x64 size; X360 was 0x234 with 4-byte pointers)
    // through the mixer allocator and placement-construct.
    void* lpMem = g_pMixerAllocator->Allocate(sizeof(NFSMixMap), 16, "NFSMixMap");
    NFSMixMap* lpMap = lpMem ? new (lpMem) NFSMixMap() : 0;
    m_pMainMixMap = lpMap;      // +0x00

    lpMap->Init(this);          // NFSMixMap::Init(map, this)

    m_pSBActiveMasks  = lpSBActiveMasks; // +0x74
    m_pMainMixMapData = lpMapData;       // +0x04
    return lpMap;
}

// ---------------------------------------------------------------------------
// NFSMixMaster::ProcessMixMap @0x82B45A88 -- per-frame drive.
//   if (m_bMapReady == 0) return;                 (lbz 0x70; beqlr)
//   m_pMainMixMap->ProcessMixMap(dt, camstate);   (tail-call vtable slot 2)
// ---------------------------------------------------------------------------
void NFSMixMaster::ProcessMixMap(float lfDeltaTime, int liCamState)
{
    if (!m_bMapReady)            // +0x70
        return;
    m_pMainMixMap->ProcessMixMap(lfDeltaTime, liCamState); // vtable[2]
}

// ---- RELOCATED HOME (2026-08-25, audio-faithfulness wave 2; from AptRenderLinkStubs.cpp,
// a 2026-08-07 targeted-export placement artifact) ----
// NFSMixMaster::InitMixMap @0x82B45920 -- the asm is exported and decoded, and its
// callee NFSMixMap::CreateMainMapState @0x82B49680 is now homed (below), but the
// body still needs NFSMixMap::AllocateInputArrays @0x82B4A120 and InitMainMapStates
// @0x82B4ABD0 (declared-only in NFSMixMap.hpp; bodies deferred in the NFSMix
// cluster -- both now have per-address dossiers in the export dir), so a faithful
// body here would still trade the stub for unresolved externals.
void NFSMixMaster::InitMixMap()                  {}   // FLAG link-stub (blocked on AllocateInputArrays/InitMainMapStates)

// NFSMixMaster::DestroyMainMainMap @0x82B457E0 (targeted export 2026-08-07) -- tear
// down the owned main map + the load bookkeeping. The console: DestroyMainMixMap on
// the map, then (*vt[0])(map, 1) == the NFSMixMap scalar-deleting dtor, null the
// slot; m_bMapReady = 0 (unconditional stb 0x70); null m_pMainMixMapData if set.
// The virtual delete is expressed as the explicit dtor + mixer-allocator Free -- the
// committed ~NFSMixMaster idiom (NFSMixMaster.cpp). The console reloads
// m_pMainMixMap between the two calls; it is the same pointer either way
// (DestroyMainMixMap does not clear it).
// FLAG (deferred): the console FIRST calls NFSMixMap::DestroyMainMixMap @0x82B47E78
// (the AllocateMixerMemory block-free inverse) -- un-exported in this dossier set
// AND undeclared in NFSMixMap.hpp (out of this TU's file set); until it lands the
// mixer-memory blocks are not freed here (the PC boot path never tears the map
// down mid-run).
void NFSMixMaster::DestroyMainMainMap()
{
    if (m_pMainMixMap)                    // +0x00
    {
        // FLAG deferred (see above): m_pMainMixMap->DestroyMainMixMap() @0x82B47E78.
        m_pMainMixMap->~NFSMixMap();                   // (*vt[0])(map, 1) ...
        g_pMixerAllocator->Free(m_pMainMixMap, 0);     // ... == dtor + mixer-heap free
        m_pMainMixMap = 0;                             // stw 0 -> +0x00
    }
    m_bMapReady = false;                  // stb 0x70 (unconditional)
    if (m_pMainMixMapData)                // +0x04 (the lwz/beq store guard, reproduced)
        m_pMainMixMapData = 0;
}
// NFSMixMaster::AssignSFXCallbacks @0x82B45A80 -- forward the SFX-callback owner into
// the main map (tail-call NFSMixMap::AssignSFXCallbacks @0x82B481B8, homed; the
// console derefs m_pMainMixMap unguarded -- reproduced).
void NFSMixMaster::AssignSFXCallbacks(void* lpOwner)
{
    m_pMainMixMap->AssignSFXCallbacks(lpOwner);
}
