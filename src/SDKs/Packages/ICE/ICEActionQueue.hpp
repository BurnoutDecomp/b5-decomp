#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"

// ICE::ActionRef / ICE::ActionQueue
// ----------------------------------------------------------------
// SHAPE (DecFIGS DWARF, SDKs/Packages/ICE/ICEActionQueue.hpp):
//     struct ICE::EControlToICEAction { s32 mAction; }
//     struct ICE::ActionRef           { s32 mID; f32 mData; }   // 8 bytes
//     struct ICE::ActionQueue : public CgsContainers::Stack<ICE::ActionRef,20> { ... }
//
// ActionRef is the 8-byte element the Stack<ICE::ActionRef,20> TU is built around: mID at
// +0x00, mData at +0x04 (the X360 Push copies both words: `*v4 = *a2; v4[1] = a2[1]`).
// float32_t in the DWARF maps to the project scalar f32 (types.hpp); there is no float32_t
// project type.
//
// ActionQueue contributes no data members of its own (it is exactly the base Stack); its
// own methods (PopAction/GetAction/Flush) are reconstructed in other TUs and are
// declaration-only here.

namespace ICE
{

// ICEActionQueue.hpp:70 -- a control-input -> ICE-action mapping entry (wraps a single action id).
struct EControlToICEAction
{
    s32 mAction;
};

// ICEActionQueue.hpp:76 -- one queued ICE action: an action id plus a scalar data payload (8 bytes).
struct ActionRef
{
    s32 ID()   const { return mID; }
    f32 Data() const { return mData; }
    void SetID(s32 liID)     { mID = liID; }
    void SetData(f32 lfData) { mData = lfData; }

    s32 mID;
    f32 mData;
};

// ICEActionQueue.hpp:91 -- a bounded (20-deep) stack of pending ICE actions. The base Stack
// supplies Push/Pop/Peek/IsFull/... ; these wrappers belong to other TUs.
struct ActionQueue : public CgsContainers::Stack<ActionRef, 20>
{
    void             PopAction();
    const ActionRef  GetAction();
    void             Flush();
};

} // namespace ICE
