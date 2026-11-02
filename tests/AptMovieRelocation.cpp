#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "SDKs/EATech/include/Apt/AptControl.h"
#include "SDKs/EATech/include/Apt/AptMovie.h"

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++guChecks;
    if (!lbPass)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcMessage);
    }
}

// Parsing, label values and the pool are observed boundaries. The record types
// and both relocation walks are production code, including GetBody().
class EAStringC
{
public:
    const char* mpText;
    explicit EAStringC(const char* lpText) : mpText(lpText) {}
};
class AptValue {};
struct AptInteger
{
    static AptValue* Create(int liFrame)
    {
        Check(liFrame == 0, "label retains its frame index");
        static AptValue lValue;
        return &lValue;
    }
};

static unsigned guHashDestructions = 0, guHashReleases = 0, guLabels = 0;
struct AptNativeHash
{
    int32_t mnCapacity;
    void* mpTable;
    AptValue* mp__Proto__;
    AptValue* mpPrototype;
    int32_t mnEventHandlerMask;

    void Set(const EAStringC& lrLabel, AptValue*)
    {
        Check(std::strcmp(lrLabel.mpText, "frame-label") == 0, "label pointer is resolved before registration");
        ++guLabels;
    }
    void DestroyGCPointers() { ++guHashReleases; }
    ~AptNativeHash() { ++guHashDestructions; }
};

struct Pool
{
    void* mpLive = nullptr;
    void* Allocate(size_t luSize)
    {
        Check(mpLive == nullptr && luSize == 0x28, "label hash uses the native pool allocation");
        mpLive = std::malloc(luSize);
        return mpLive;
    }
    void Deallocate(void* lpBlock, size_t luSize)
    {
        Check(lpBlock == mpLive && luSize == 0x28, "hash returns to its allocating pool with its size");
        std::free(lpBlock);
        mpLive = nullptr;
    }
};
static Pool gPool;
static Pool* gpAptPseudoDataPool = &gPool;

static uintptr_t guBase;
static unsigned guResolves = 0, guUnresolves = 0;
struct AptActionInterpreter
{
    static void _parseStream(unsigned char* lpStream, uintptr_t luBase,
                             void* lpConstFile, int64_t* lpCount)
    {
        Check(luBase == guBase && lpStream >= reinterpret_cast<unsigned char*>(guBase + 0x500) &&
              lpStream <= reinterpret_cast<unsigned char*>(guBase + 0x520),
              "parser receives a full-width relocated stream and base");
        Check(lpStream[0] == 0, "stream contents survive relocation");
        if (lpConstFile != nullptr)
            ++guResolves;
        else
            ++guUnresolves;
        ++*lpCount;
    }
};

#include "apt_movie_relocation_types.inc"
#include "apt_movie_relocation_bodies.inc"

template<class T> static T* Relative(T* lpPointer)
{
    return lpPointer ? reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lpPointer) - guBase) : nullptr;
}

int main()
{
    alignas(8) std::array<unsigned char, 4096> laBlob = {};
    guBase = reinterpret_cast<uintptr_t>(laBlob.data());
    Check(guBase > UINT32_MAX, "fixture exercises a base above 4 GiB");
    auto* lpFrames = reinterpret_cast<AptMovieFrame*>(laBlob.data() + 0x80);
    auto** lpCommands = reinterpret_cast<void**>(laBlob.data() + 0x100);
    auto* lpAction = reinterpret_cast<AptControl*>(laBlob.data() + 0x200);
    auto* lpLabel = reinterpret_cast<AptControl*>(laBlob.data() + 0x220);
    auto* lpPlace = reinterpret_cast<AptPlaceObjectInfo_t*>(laBlob.data() + 0x240);
    auto* lpUnalignedPlace = reinterpret_cast<AptPlaceObjectInfo_t*>(laBlob.data() + 0x2a4);
    auto* lpEmptyPlace = reinterpret_cast<AptPlaceObjectInfo_t*>(laBlob.data() + 0x300);
    auto* lpInit = reinterpret_cast<AptControl*>(laBlob.data() + 0x360);
    auto* lpRemove = reinterpret_cast<AptControl*>(laBlob.data() + 0x380);
    auto* lpClip = reinterpret_cast<AptClipEventHandlerList*>(laBlob.data() + 0x400);
    auto* lpOtherClip = reinterpret_cast<AptClipEventHandlerList*>(laBlob.data() + 0x420);
    auto* lpHandlers = reinterpret_cast<AptClipEventHandler*>(laBlob.data() + 0x440);
    auto* lpOtherHandler = reinterpret_cast<AptClipEventHandler*>(laBlob.data() + 0x460);
    std::strcpy(reinterpret_cast<char*>(laBlob.data() + 0x540), "frame-label");
    std::strcpy(reinterpret_cast<char*>(laBlob.data() + 0x550), "instance");

    lpAction->eType = AptControlType_DoAction;
    lpAction->action.actions.aActionStream = Relative(laBlob.data() + 0x500);
    lpLabel->eType = AptControlType_FrameLabel;
    lpLabel->frameLabel.szLabel = Relative(reinterpret_cast<char*>(laBlob.data() + 0x540));
    for (auto* lpRecord : {lpPlace, lpUnalignedPlace, lpEmptyPlace})
        lpRecord->muTag = AptControlType_PlaceObject2;
    lpPlace->GetBody()->mpName = Relative(reinterpret_cast<char*>(laBlob.data() + 0x550));
    lpPlace->GetBody()->mpClipActions = Relative(lpClip);
    lpUnalignedPlace->GetBody()->mpClipActions = Relative(lpOtherClip);
    lpClip->mnCount = 2;
    lpClip->mpHandlers = Relative(lpHandlers);
    lpOtherClip->mnCount = 1;
    lpOtherClip->mpHandlers = Relative(lpOtherHandler);
    // Native readers ignore padding. It must never select a pointer layout.
    const uint32_t luPadding = 0x12345678;
    std::memcpy(reinterpret_cast<char*>(lpClip) + 4, &luPadding, sizeof(luPadding));
    lpHandlers[0].mpActionStream = Relative(laBlob.data() + 0x508);
    lpHandlers[1].mpActionStream = Relative(laBlob.data() + 0x510);
    lpOtherHandler->mpActionStream = Relative(laBlob.data() + 0x518);
    lpInit->eType = AptControlType_DoInitAction;
    lpInit->initAction.nSpriteID = -17;
    lpInit->initAction.actions.aActionStream = Relative(laBlob.data() + 0x520);
    lpRemove->eType = AptControlType_RemoveObject2;
    void* laCommands[] = {lpAction, lpLabel, lpPlace, lpUnalignedPlace, lpEmptyPlace, lpInit, lpRemove};
    for (unsigned luCommand = 0; luCommand < 7; ++luCommand)
        lpCommands[luCommand] = Relative(laCommands[luCommand]);
    lpFrames[0].mnCommandCount = 7;
    lpFrames[0].mpCommands = Relative(lpCommands);
    AptMovie lMovie = {};
    lMovie.mnFrameCount = 2;
    lMovie.mpFrames = Relative(lpFrames);
    const auto laOriginal = laBlob;

    for (unsigned luCycle = 0; luCycle < 2; ++luCycle)
    {
        int64_t lnParsed = 0;
        lMovie.resolve64(guBase, guBase, static_cast<uint32_t>(laBlob.size()),
                         reinterpret_cast<AptConstFile*>(guBase), &lnParsed);
        Check(lMovie.mpFrames == lpFrames && lpFrames[0].mpCommands == lpCommands,
              "frame and command tables resolve through named members");
        Check(lpCommands[2] == lpPlace && lpCommands[3] == lpUnalignedPlace,
              "both place-record alignments retain their command pointers");
        Check(lpPlace->GetBody()->mpClipActions == lpClip && lpClip->mpHandlers == lpHandlers,
              "native clip pointers resolve independently of padding bytes");
        Check(lpUnalignedPlace->GetBody()->mpClipActions == lpOtherClip &&
              lpOtherClip->mpHandlers == lpOtherHandler,
              "four-byte-aligned place records use the shared body accessor");
        Check(lnParsed == 5, "all action, clip-handler and init-action streams resolve");
        Check(lpEmptyPlace->GetBody()->mpName == nullptr &&
              lpEmptyPlace->GetBody()->mpClipActions == nullptr,
              "optional place pointers remain null");
        lnParsed = 0;
        lMovie.unresolve(guBase, &lnParsed);
        Check(lnParsed == 5, "teardown un-parses the same five streams");
        Check(lMovie.mpFrames == Relative(lpFrames) && lMovie.mpLabelHash == nullptr,
              "teardown restores the frame offset and clears the live hash");
        Check(lpFrames[1].mpCommands == nullptr, "empty-frame command pointer stays null");
        Check(lpInit->initAction.nSpriteID == 17, "teardown restores a consumed init-action id");
        auto laExpected = laOriginal;
        reinterpret_cast<AptControl*>(laExpected.data() + 0x360)->initAction.nSpriteID = 17;
        Check(laBlob == laExpected, "complete serialized payload round-trips without changing padding");
    }
    Check(guResolves == 10 && guUnresolves == 10, "both reload cycles parse and un-parse exactly once");
    Check(guLabels == 2 && guHashDestructions == 2 && guHashReleases == 2 && gPool.mpLive == nullptr,
          "each reload owns and releases one label hash");
    std::printf("AptMovieRelocation: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
