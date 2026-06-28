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
};

// FLAG (homed by the AptValue conversion layer / the VM): convert an AptValue to
// an integer (AptInteger -> its value, etc.). Used by labelToFrame on the
// label-hash's stored frame-index values.
int AptValue_toInteger(AptValue* pValue);
