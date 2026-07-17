#pragma once

// ===========================================================================
// EATech Apt -- AptMovie: a movie clip's timeline.
//
// The timeline embedded in a sprite/animation character (char+16). It is an array
// of frames, each frame a list of display-list commands (place/remove/action/
// label), plus a label->frame-index map. The player walks it each tick to drive
// the clip's display list + run its frame actions.
//
// SHAPE + the clean method from the PS3 EXTERNAL ELF (8AptMovie): labelToFrame
// @0x7F936C. LAYOUT (from AptMovie::resolve @0x80BC30): [0] frame count, [1] frame
// table, [2] label hash (name -> AptInteger frame index).
//
// SCOPE: the timeline data model + label lookup (VM-free). The resolve (the
// timeline relocate/transcode), doFrameControls (drive the display list), and
// runFrameActions/queueFrameActions (run the frame's ActionScript) all need the
// AptActionInterpreter VM and are the follow-on.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptConstFile;   // the serialised const chunk (the resolve64 parse ctx)

struct AptNativeHash;
class AptValue;
class  EAStringC;
class  AptPseudoDisplayList;
struct AptDisplayList;
struct AptCIH;

// One frame: its display-list commands (place/remove/action/label). The command
// records are part of the serialised .apt timeline (resolved by AptMovie::resolve);
// kept opaque here.
struct AptMovieFrame
{
    int32_t mnCommandCount;   // [0]
    void**  mpCommands;       // [1] (command records; tag at +0: 1=action/2=label/3=place/...)
};

struct AptMovie
{
    int32_t        mnFrameCount;   // [0]
    AptMovieFrame* mpFrames;       // [1]
    AptNativeHash* mpLabelHash;    // [2] -- label name -> frame index (AptInteger)

    // @0x7F936C -- the frame index for a label, or -1.
    int labelToFrame(const EAStringC* pLabel) const;

    // ---- the timeline driver / relocate pass (X360 ARTIST.XEX) -------------
    // The methods that drive the clip's display list + run its frame actions, and
    // the (un)resolve relocation pass the loader runs over the serialised timeline.
    // The per-frame command records they walk are the in-place .apt blob (no
    // recovered struct yet) -- see the .cpp for the FLAGged opaque-record access
    // and the deferred VM/display-list callees they reach.

    // @0x82AEEB98 -- build the interpreter's pseudo display list for frame nFrame
    // (place/remove commands), for AptCIH::jumpToFrame's skip path.
    AptMovie* DoTemporaryFrameControls(AptPseudoDisplayList* pPseudoList, int nFrame, int a4, void* a5);

    // @0x82B0B7A0 -- run frame nFrame's commands against a live CIH: run its action
    // streams, then apply its place/remove/back-to-script commands. pDisplayList is the
    // owning clip's child display list (a2, the place/remove target); pParent is the
    // owning sprite CIH (a3). Native-8: the frame table + command records are read at the
    // relocated native-8 offsets (mpFrames now a live pointer via resolve64), and the
    // movie char-anim def-base is reached through pParent's named char-inst chain.
    AptMovie* doFrameControls(AptDisplayList* pDisplayList, AptCIH* pParent, int nFrame);

    // @0x82AE0228 -- queue frame nFrame's action commands onto the director queue.
    AptMovie* queueFrameActions(void* pCIH, int nFrame);

    // @PS3 0x820FA4 -- run frame nFrame's tag-1 action commands IMMEDIATELY on the
    // VM singleton (push a register-block window, runStream each action stream with
    // pCIH's root-animation character-inst as the run scope, pop the window). The
    // CallFrame opcode's synchronous sibling of queueFrameActions. Body in AptMovie.cpp.
    const AptMovie* runFrameActions(AptCIH* pCIH, int nFrame) const;

    // @0x82AF80B0 -- relocate the just-loaded timeline against the load base (and
    // build the label hash + parse each action stream). Returns the last sub-result.
    void* resolve(int nBase, void* a3, int a4);

    // resolve64 -- the x64 (native-8 GUIAPT "1:7:8") fork of resolve @0x82AF80B0.
    // Identical relocation walk as resolve(), but every serialised POINTER slot is a
    // full 8-byte word holding a file-relative OFFSET (offset -> offset+base, in place),
    // the frame table stride is 16 (AptMovieFrame {count@0, cmds@8}) and the command-
    // pointer array stride is 8. Drives the ROOT + sub-movie frame-table relocation the
    // console-32 resolve() truncates on x64 (it truncates the high x64 base into an int).
    // Called from AptCharacterAnimation::Fixup case-5/9 on the embedded movie (char+0x20).
    //
    // nBase = the 8-byte load base; nResBase/nResSize bound the resource so every slot is
    // validated (a slot holds a file-relative offset iff 0 < off < nResSize) -- a garbage/
    // already-relocated slot is left untouched (no wild-pointer write/deref).
    //
    // pConstFile / pnParsedValues (ACTIVATED 2026-07-01, the XB1 resolve twin
    // sub_1408567D0's a3/a4): the "Apt constant file" chunk (the _parseStream const ctx --
    // its record table lives at ctx+0x28, relocated by Resolve; string payloads rebase
    // ctx-relative) and the resolved-value counter (the root def's +0x50, zeroed by
    // CompleteLoad) every action/clipActions/morph stream parse accumulates into.
    void resolve64(uintptr_t nBase, uintptr_t nResBase, uint32_t nResSize,
                   AptConstFile* pConstFile, int64_t* pnParsedValues);

    // @0x82AF4830 -- the inverse of resolve (un-relocate + tear down the label hash).
    void* unresolve(int nBase, int a3);
};

// AptValue_toInteger removed -- it was a forwarder to the real member
// AptValue::toInteger() (declared in AptValue.h). labelToFrame now calls that
// member directly on the label-hash's stored frame-index values.
