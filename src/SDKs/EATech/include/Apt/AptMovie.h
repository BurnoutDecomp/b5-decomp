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

struct AptNativeHash;
struct AptValue;
class  EAStringC;
class  AptPseudoDisplayList;

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
    // streams, then apply its place/remove/back-to-script commands.
    AptMovie* doFrameControls(void* a2, void* pParent, int nFrame);

    // @0x82AE0228 -- queue frame nFrame's action commands onto the director queue.
    AptMovie* queueFrameActions(void* pCIH, int nFrame);

    // @0x82AF80B0 -- relocate the just-loaded timeline against the load base (and
    // build the label hash + parse each action stream). Returns the last sub-result.
    void* resolve(int nBase, void* a3, int a4);

    // @0x82AF4830 -- the inverse of resolve (un-relocate + tear down the label hash).
    void* unresolve(int nBase, int a3);
};

// FLAG (homed by the AptValue conversion layer / the VM): convert an AptValue to
// an integer (AptInteger -> its value, etc.). Used by labelToFrame on the
// label-hash's stored frame-index values.
int AptValue_toInteger(AptValue* pValue);
