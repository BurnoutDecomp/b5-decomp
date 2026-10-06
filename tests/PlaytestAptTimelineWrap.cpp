// Actual tick and jumpToFrame bodies, with terminal display-list I/O witnesses.
// The authored two-frame B5HelpItem uses char2/+39 in frame0, char4/-304 in frame1.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <new>

struct AptCIH;
struct AptMovie;
struct AptNativeHash;
struct AptPseudoDisplayList;
enum { AptVFT_CIHNone = 37 };
static int gnAptActionFrameId = 19;
static int gbAptRecorderGate = 1;

struct ChildState { int character; int alignment; float tx; };
static ChildState FrameState(int frame) {
    return frame == 0 ? ChildState{2,0,39.0f} : ChildState{4,1,-304.0f};
}
struct AptRenderItem { uint32_t mFlags = 0; AptMovie* movie = nullptr; };
struct AptDisplayList {
    ChildState child = FrameState(0);
    int childDirty = 0;
    int merges = 0;
    int tick(int,int) { return childDirty; }
    AptCIH* mergeState(void**,void*,char);
};
struct AptCharacterSpriteInstBase {
    uint32_t mTypeFlags = 5;
    uint32_t mnClipActionFlags = 0;
    int mnGotoFrame = -1;
    int mnLastActionFrame = -1;
    void* mpProperties = nullptr;
    AptRenderItem* mpRenderItem = nullptr;
    AptDisplayList mDisplayList;
    unsigned GetTypeTag() const { return mTypeFlags; }
};
struct AptPseudoDisplayList {
    AptCIH* parent;
    ChildState child = FrameState(0);
    explicit AptPseudoDisplayList(AptCIH* p) : parent(p) {}
};
struct Pool {
    int allocations = 0, releases = 0;
    void* Allocate(size_t n) { ++allocations; return std::malloc(n); }
    void Deallocate(void* p,size_t) { ++releases; std::free(p); }
} gPool;
static Pool* gpAptPseudoDataPool = &gPool;
struct AptMovie {
    int mnFrameCount = 2;
    int directCalls = 0, temporaryCalls = 0, queuedActions = 0;
    int lastQueued = -1;
    void doFrameControls(AptDisplayList* list,AptCIH*,int frame) {
        ++directCalls; list->child = FrameState(frame);
    }
    AptMovie* DoTemporaryFrameControls(AptPseudoDisplayList* scratch,int frame,int,void*) {
        ++temporaryCalls; scratch->child = FrameState(frame); return this;
    }
    void* queueFrameActions(AptCIH*,int frame) {
        ++queuedActions; lastQueued = frame; return this;
    }
};
struct AptCIH {
    AptCharacterSpriteInstBase* mpCharacterInst;
    uint32_t mFlagsA = 0x40;
    int valueTag = 12;
    int eventMembers = 0, events = 0;
    int getVtblIndex() const { return valueTag; }
    bool HasEventMember(int mask) const { return (eventMembers & mask) != 0; }
    void queueClipEvents(int mask,int,int) { events |= mask; }
    int tick();
    int jumpToFrame(int);
};
static AptMovie* AptGetClipMovie(AptCharacterSpriteInstBase* inst) { return inst->mpRenderItem->movie; }
AptCIH* AptDisplayList::mergeState(void** info,void*,char) {
    auto* scratch = reinterpret_cast<AptPseudoDisplayList*>(info);
    child = scratch->child; ++merges; return scratch->parent;
}
#include "playtest_apt_timeline_wrap_bodies.inc"

static unsigned checks = 0, failures = 0;
static void Check(bool ok,const char* message) {
    ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",message); }
}
int main() {
    // Multi-frame wrap must restore children AND queue frame0 actions through
    // the real production seek, even when its terminal callbacks are deferred.
    for (int frames : {2,3,8}) for (unsigned flags : {0x2000000u,0x3000000u,0x1000000u}) {
        AptMovie movie; movie.mnFrameCount = frames;
        AptRenderItem render; render.movie = &movie;
        AptCharacterSpriteInstBase inst; inst.mpRenderItem = &render;
        inst.mnGotoFrame = frames-1; inst.mnClipActionFlags = flags;
        inst.mDisplayList.child = FrameState(1);
        AptCIH node{&inst}; gbAptRecorderGate = 1;
        node.tick();
        Check(inst.mnGotoFrame == 0,"wrap restores play-head0");
        Check(inst.mDisplayList.child.character == 2,"wrap restores frame0 child character");
        Check(inst.mDisplayList.child.alignment == 0 && inst.mDisplayList.child.tx == 39.0f,"wrap restores authored left-field placement");
        Check(movie.temporaryCalls == 1 && inst.mDisplayList.merges == 1,"wrap traverses production frame0 replay and merge");
        Check(movie.queuedActions == 1 && movie.lastQueued == 0,"wrap queues original frame0 action via seek");
        Check(movie.directCalls == 0,"wrap does not also run normal direct controls");
    }
    // A one-frame clip takes the native hold branch, not the multi-frame seek.
    {
        AptMovie movie; movie.mnFrameCount = 1;
        AptRenderItem render; render.movie = &movie;
        AptCharacterSpriteInstBase inst; inst.mpRenderItem = &render;
        inst.mnGotoFrame = 0; inst.mnClipActionFlags = 0x2000000u;
        AptCIH node{&inst}; node.tick();
        Check(inst.mnGotoFrame == 0 && inst.mDisplayList.child.character == 2,"one-frame child and play-head remain0");
        Check(movie.temporaryCalls == 0 && movie.directCalls == 0 && movie.queuedActions == 0,"one-frame hold has no replay/actions");
    }
    // Ordinary next-frame progression and stopped/recorder-closed nodes retain
    // their native paths; restoring wrap must not force unrelated seeks.
    {
        AptMovie movie; AptRenderItem render; render.movie = &movie;
        AptCharacterSpriteInstBase inst; inst.mpRenderItem = &render;
        inst.mnGotoFrame = 0; inst.mnClipActionFlags = 0x2000000u;
        AptCIH node{&inst}; node.tick();
        Check(inst.mnGotoFrame == 1 && inst.mDisplayList.child.character == 4,"normal advance composes authored right frame");
        Check(movie.directCalls == 1 && movie.temporaryCalls == 0 && movie.lastQueued == 1,"normal advance dispatch remains direct");
    }
    for (unsigned flags : {0u,0x1000000u}) {
        AptMovie movie; AptRenderItem render; render.movie = &movie;
        AptCharacterSpriteInstBase inst; inst.mpRenderItem = &render;
        inst.mnGotoFrame = 1; inst.mnClipActionFlags = flags;
        inst.mDisplayList.child = FrameState(1);
        AptCIH node{&inst}; gbAptRecorderGate = 0; node.tick();
        Check(inst.mnGotoFrame == 1 && inst.mDisplayList.child.character == 4,"stopped/recorder-closed holds child state");
        Check(movie.directCalls == 0 && movie.temporaryCalls == 0 && movie.queuedActions == 0,"stopped/recorder-closed has no forced seek");
    }
    Check(gPool.allocations == gPool.releases,"temporary replay allocations are released");
    std::printf("PlaytestAptTimelineWrap: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
