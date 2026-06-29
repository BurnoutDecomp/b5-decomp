// ===========================================================================
// EATech Apt -- AptAnimationTarget class-static data layer.
//
// A small set of FILE-STATIC tables the Apt input/update layer shares across all
// animation targets (X360 globals dword_8324E534..dword_8324E550), plus the
// static accessors over them. Reconstructed from the X360 ARTIST pseudocode/asm
// (SetupStaticData @0x82AE41F0 builds them, CleanupStaticData @0x82AE42A8 frees
// them) via the decompile->verify workflow.
//
// (The per-player analog-stick tables saAStickLeft/Right @unk_8324D750/unk_8324E2D8
// are a follow-on -- their fixed element bound is not yet determined.)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"

#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager::Allocate/Deallocate

#include <cstring>   // memset

// The shared Apt fixed-size pool (X360 off_8324D808); defined by the Apt pseudo-data
// layer (same handle AptActionQueue / AptCharacterSpriteInstBase allocate from).
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

namespace
{
    // X360 globals dword_8324E534..dword_8324E550 (the shared static-data layer).
    int   ssMousePosX             = 0;        // dword_8324E534 -- cursor X fed to AS
    int   ssMousePosY             = 0;        // dword_8324E538 -- cursor Y fed to AS
    void* spStaticBlock           = nullptr;  // dword_8324E53C -- 80-byte scratch block
    int   snMaxNewMovieClips      = 0;        // dword_8324E540 -- table element count
    void* spNewInsts              = nullptr;  // off_8324E544   -- new-instance table (4*count)
    int   snNewInstSize           = 0;        // dword_8324E548 -- new-instance fill counter
    void* spDelayedReleaseList    = nullptr;  // off_8324E54C   -- delayed-release table (4*count)
    int   snDelayedReleaseListSize = 0;       // dword_8324E550 -- delayed-release fill counter
}

// ---- class-static accessors (X360 one-liners over the globals above) -----------
int   AptAnimationTarget::GetXMousePos()             { return ssMousePosX; }              // @0x82AD5F98
int   AptAnimationTarget::GetYMousePos()             { return ssMousePosY; }              // @0x82AD5FA8
int   AptAnimationTarget::GetMaxNewMovieClips()      { return snMaxNewMovieClips; }       // @0x82AD5E90
void* AptAnimationTarget::GetNewInsts()              { return spNewInsts; }               // @0x82AD5EA0
int   AptAnimationTarget::GetNewInstSize()           { return snNewInstSize; }            // @0x82AD5EB0
void* AptAnimationTarget::GetDelayedReleaseList()    { return spDelayedReleaseList; }     // @0x82AD5ED8
int   AptAnimationTarget::GetDelayedReleaseListSize(){ return snDelayedReleaseListSize; } // @0x82AD5EE8

// DecNewInstSize @0x82AD5EC0 -- despite the name, hands out the next new-instance
// index and POST-INCREMENTS the fill counter (returns the pre-increment value).
int   AptAnimationTarget::DecNewInstSize()           { return snNewInstSize++; }

// SetupStaticData @0x82AE41F0 -- allocate the shared static tables, sized to the
// max-new-movie-clips count. (The X360 reads the count from its params struct at
// +0x18; taken here as the explicit count so no opaque param layout is needed.)
void AptAnimationTarget::SetupStaticData(int nMaxNewMovieClips)
{
    spStaticBlock = gpAptPseudoDataPool->Allocate(80);
    memset(spStaticBlock, 0, 80);

    snMaxNewMovieClips   = nMaxNewMovieClips;
    spNewInsts           = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    spDelayedReleaseList = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    memset(spDelayedReleaseList, 0, 4 * nMaxNewMovieClips);

    snNewInstSize            = 0;
    snDelayedReleaseListSize = 0;
}

// CleanupStaticData @0x82AE42A8 -- free the three tables SetupStaticData built.
void AptAnimationTarget::CleanupStaticData()
{
    gpAptPseudoDataPool->Deallocate(spNewInsts, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spDelayedReleaseList, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spStaticBlock, 80);
}
