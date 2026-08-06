// =====================================================================================
// rw::audio::core::Send -- part-file 01 of the TU: the one remaining ledger body,
//
//   Send::ConnectByNameHandler @0x82B9FF80
//
// It lands in this part-file rather than in the committed Send.cpp because that file is
// owned by another worker this wave. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative for every store.
//
// PROVENANCE (corrected -- an earlier banner here claimed "there is no Feb-2007 source
// and no DecFIGS DWARF for this type"; both halves of that were WRONG, and the claim was
// suppressing the highest-authority sources in the tree):
//   references/Feb-2007/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
//     rw/audio/core/plugins/send.h    -- declares `static int ConnectByNameHandler(
//     Command *pCommand);`, `struct ConnectByNameCommand : public Command,
//     ConnectByNameParams {};`, `void DisconnectImmediate();` and the Send member list.
//   .../plugins/submix.h              -- the COMPLETE SubMix class: `static ListDStack
//     sSubMixList; static ListDNode *spSubMixNextNode; float *mpSubMixBuffer; ListDStack
//     mSendList; ListDNode mSubMixListNode; float mDeClickValueTotal[MAX_CHANNELS];
//     char mName[64]; unsigned char mSubMixAdded; unsigned char mDeClickRequired;`
//     (= console +0x24/+0x28/+0x2C/+0x34/+0x4C/+0x8C/+0x8D -- every one of those matches
//     an ARTIST load/store in this function or its SubMix-side siblings), plus the two
//     enumerator bodies quoted at the MEASURED block below and `const char *GetName()
//     { return (mName); }`. .../channel.h:22 gives `MAX_CHANNELS = 6`, which is why
//     mDeClickValueTotal spans exactly 0x34..0x4B.
//   references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/audio/core/plugins/submix.h
//     -- emits `extern ListDStack sSubMixList;` (submix.h:151) and `extern ListDNode *
//     spSubMixNextNode;` (submix.h:152), independently confirming the two statics.
//   The NFS ProStreet08Milestone.pdb + .map (X360, Oct-2007, same rwaudiocore vocabulary)
//   agrees with all of the above and is kept only as corroboration, not as the basis.
// Every offset was still re-checked against the ARTIST asm (see the annotated walk below
// and scratchpad/waveL/Send.spec.md sections 1 and 3).
//
// ARTIST-vs-Feb-2007 DIVERGENCE (measured -- the leak must NOT be copied here). Feb-2007
// `struct ConnectByNameParams { const char *pName; };` puts a POINTER at command+0xC.
// ARTIST does not: `addi r3,r31,8 ; addi r4,r3,4` makes r4 = cmd+0xC and the compare loop
// then does `mr r9,r4 ; lbz r7,0(r9)` -- a byte load straight off cmd+0xC with NO
// intervening `lwz`. The ARTIST record therefore INLINES the name string, which is what
// EventEvent @0x82BA3F00 builds (12-byte header + strcpy'd name, size `(len+16)&~3` on
// the console). The tree's `char maName[1]` + `muRecordSize` model in Send.h is the
// correct one for this build; the 2.11.00 by-pointer form is a different revision.
//
// PATH NOTE (measured): the wave brief named
// `b5-decomp/vendor/renderware/audio/core/Send_wL_01.cpp`, but that directory does not
// exist in the tree -- the renderware vendor sources live under
// `b5-decomp/vendor/renderware/src/rw/audio/core/` (where Send.cpp itself and the
// previous wave's part-files EaXmaDec_wG_02/03/04.cpp sit). The basename is unchanged,
// so the wave's collision-free-by-construction property is preserved.
//
// GATE STATE: COMPILES. The shared-header change this file used to be blocked on has
// landed -- b5-decomp/vendor/renderware/include/rw/audio/core/SubMixConnector.h now
// carries `ListDNode mSubMixListNode` (console +0x2C), `char mName[64]` (console +0x4C),
// `static ListDStack sSubMixList` (off_8327EE68) and `static ListDNode *spSubMixNextNode`
// (dword_8327EE00), plus the `struct ListDStack { ListDNode *phead; }` definition.
// MEASURED (this pass): `PYTHONUTF8=1 py tools/work/selfcheck.py Send.cpp Send_wL_01.cpp`
// -> STATUS=pass. Do not re-open the header on this function's account.
//
// HISTORICAL NOTE ON THE UNBLOCK SPEC -- DO NOT APPLY ITS SECTION 2 VERBATIM (measured):
// scratchpad/waveL/Send.spec.md section 2(a) asks SubMixConnector.h to DEFINE a new
// `class ListDNode { pnext; pprev; }`. That type ALREADY HAS A REAL HOME:
// `struct rw::audio::core::ListDNode` at
// b5-decomp/vendor/renderware/include/rw/audio/core/ITask.h:31 (same members; Feb-2007
// homes both -- `class ListDNode` at private/linklist.h:54, `class ListDStack` at
// private/linklist.h:142 -- in rw/audio/core/private/linklist.h). Defining it a second
// time is a redefinition (C2011) in every TU that sees both headers -- MEASURED: Route.cpp
// and Send.cpp both FAILED to compile with the spec's verbatim text (Send.cpp reaches
// ITask.h through PlugIn.h). The header took the correct form instead: it `#include`s
// "rw/audio/core/ITask.h" and adds only `struct ListDStack { ListDNode *phead; };`
// (1 member -- Feb-2007's ListDStack has exactly one data member, `ListDNode *phead`, the
// rest of the class being inline accessors that ARTIST open-codes at every call site).
//
// LINK-TIME (cl /c cannot see these -- reported, not hidden): the DEFINITIONS of
// SubMix::sSubMixList and SubMix::spSubMixNextNode belong to the seeded-but-todo
// `class:rw::audio::core::SubMix` TU (GetSize @0x82B982F0, GetPlugInDescRunTime
// @0x82B9C370, CreateInstanceHandler @0x82B9C380, Process @0x82B9C480, ReleaseEvent
// @0x82BA0C18, scalar-deleting dtor @0x82BA1DE8 -- none reconstructed yet). They stay
// unresolved at link until that TU lands, exactly like any other not-yet-reconstructed
// callee. Send::DisconnectImmediate is already bodied in Send.cpp.
// =====================================================================================

#include "rw/audio/core/Send.h"            // Send, SendConnectByNameCommand
#include "rw/audio/core/SubMixConnector.h" // SubMix, SubMixConnector, ListDNode, ListDStack

#include <cstddef> // offsetof -- node-to-owner conversion on the HOST
#include <cstring> // std::strcmp -- the asm's inlined character loop, de-inlined

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// ConnectByNameHandler @0x82B9FF80 -- replay a queued NAME-connect command off the
// owning System's deferred-command ring (the consumer calls it through int (*)(void *)
// with the record's own address; EventEvent @0x82BA3F00 wrote the record).
//
// Annotated asm walk (raw listing: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B9FF80.json):
//   r31 = cmd ; r30 = *(cmd+4) = send ; bl Send::DisconnectImmediate(send)
//   r11 = off_8327EE68            ; sSubMixList.phead
//   dword_8327EE00 = r11          ; spSubMixNextNode = head  (EnumerateSubMixReset inlined)
//   r3  = cmd+8 (kept live for the return) ; r4 = r3+4 = cmd+0xC (the queued name)
//   b loc_82B9FFF4                ; the loop's node-null test is at the TOP
// loc_82B9FFF4:
//   cmplwi cr6, r11, 0 ; beq -> done          ; end of list
//   addic. r10, r11, -0x2C                    ; node -> owning SubMix (CR0 <- owner)
//   lwz r11, 0(r11)                           ; node = node->pnext
//   stw r11, dword_8327EE00                   ; spSubMixNextNode = node  (EnumerateSubMix inlined)
//   bne loc_82B9FFC0 ; else b done            ; owner == NULL -> stop
// loc_82B9FFC0 .. loc_82B9FFEC:               ; inlined strcmp(cmd+0xC, subMix+0x4C)
//   r8 = r10+0x4C ; r9 = r4 ; per char: r7=*r9, r5=*r8, r5 = r7 - r5
//   exit when *r9 == 0 or r5 != 0 ; then `cmpwi r5,0 ; beq loc_82BA0010` == equal strings
//   not equal -> fall through to loc_82B9FFF4 (next node)
// loc_82BA0010 (MATCH -- the same tail as ConnectByPointerHandler @0x82BA0010..0x82BA0048):
//   r11 = r30+0x30 = &send->mSubMixConnector
//   stw r10, 0xC(r11)                         ; conn->mpSubMix = subMix
//   lwz r8, 0x24(r10) ; stw r8, 8(r11)        ; conn->mpSubMixBuffer = subMix->mpSubMixBuffer
//   lbz r8, 0x21(r10) ; stb r8, 0x10(r11)     ; conn->mNumSubMixChannels = subMix->mbNumChannels
//   lwz r8, 0x28(r10) ; stw 0, 4(r11) ; stw r8, 0(r11)
//   lwz r9, 0x28(r10) ; if (r9) stw r11, 4(r9)  ; head->mppPrev = conn
//   stw r11, 0x28(r10)                        ; subMix->mpConnectorHead = conn
//   (falls through to done -- stops after the FIRST match)
// done: lwz r3, 0(r3) ; blr                   ; return *(cmd+8) == muRecordSize
//
// MEASURED. Every offset/width/side-effect above, the loop shape, the first-match break
// and the return operand come from the asm. The NAMES and TYPES are measured too, from
// Feb-2007 plugins/submix.h (see the provenance block at the top) and corroborated by the
// DecFIGS dwarfdump of the same header: `static ListDStack sSubMixList` and
// `static ListDNode *spSubMixNextNode` are SubMix's own private statics, and
// `mSubMixListNode` / `mName` are the members the asm's -0x2C and +0x4C reach.
//
// The two cursor writes are the inlined enumerator pair, also MEASURED rather than
// guessed: submix.h spells `static void EnumerateSubMixReset() { spSubMixNextNode =
// sSubMixList.GetHead(); }` -- which is exactly `dword_8327EE00 = *off_8327EE68`
// @0x82B9FFB4-B8 -- and declares `static SubMix *EnumerateSubMix();`, whose
// node->owner + advance + return-NULL contract is exactly the `addic. r10,r11,-0x2C /
// lwz r11,0(r11) / stw r11,cursor / bne` group @0x82B9FFFC-0x82BA0008.
//
// Still INFERRED (identification only, unaffected by the code): that the specific data
// addresses off_8327EE68 / dword_8327EE00 in THIS image are those two statics. The
// correspondence is exact in shape, offsets and order, and the ProStreet map manglings
// ?sSubMixList@SubMix@core@audio@rw@@0VListDStack@234@A /
// ?spSubMixNextNode@SubMix@core@audio@rw@@0PAVListDNode@234@A agree; either way the
// cursor writes are reproduced, because they are real stores to class-owned state.
//
// X360-LITERAL TRAPS handled: (a) the console `-0x2C` is offsetof(SubMix,
// mSubMixListNode) on the host, never the literal -- SubMix carries host-width pointers
// so the byte offset differs; (b) the console `+0x4C` name offset is `mName` by member
// access, not arithmetic; (c) the return is the record's stored muRecordSize, which
// EventEvent already wrote as the HOST record size, NOT the console `(len + 16) & ~3`.
// No floating point anywhere in this function, so no fcmpu/NaN-polarity concern.
// -------------------------------------------------------------------------------------
int Send::ConnectByNameHandler(void* cmd)
{
    SendConnectByNameCommand* lpCmd = static_cast<SendConnectByNameCommand*>(cmd);
    Send* lpSend = lpCmd->mpTarget; // lwz r30, 4(r31)

    DisconnectImmediate(lpSend); // bl @0x82B9FFA0

    // Walk the global by-name SubMix registry. The X360 build inlined
    // SubMix::EnumerateSubMixReset()/EnumerateSubMix() here (both declared in Feb-2007
    // plugins/submix.h), so the class-static cursor SubMix::spSubMixNextNode is advanced
    // exactly where the asm advances it (a real side effect on SubMix-owned state -- kept).
    ListDNode* lpNode = SubMix::sSubMixList.phead; // lwz off_8327EE68
    SubMix::spSubMixNextNode = lpNode;             // stw dword_8327EE00
    while (lpNode)                                 // cmplwi cr6, r11, 0 @0x82B9FFF4
    {
        // Node-to-owner (EnumerateSubMix's body). The console form is
        // `addic. r10, r11, -0x2C`; its CR0 side effect is the owner-is-NULL test consumed
        // by the `bne` below. The host form must use offsetof -- the console byte offset is
        // not the host one, because SubMix's leading members widen on LLP64.
        SubMix* lpSubMix = reinterpret_cast<SubMix*>(
            reinterpret_cast<char*>(lpNode) - offsetof(SubMix, mSubMixListNode));

        lpNode = lpNode->pnext;            // lwz r11, 0(r11)
        SubMix::spSubMixNextNode = lpNode; // stw r11, dword_8327EE00

        // `bne loc_82B9FFC0 / b done` -- NOT a degenerate guard. Feb-2007 submix.h
        // declares `static SubMix *EnumerateSubMix();`, so this is the caller's
        // `while ((pSubMix = EnumerateSubMix()) != NULL)` test: the enumerator's own
        // node-is-NULL exit is the `cmplwi cr6,r11,0` at the top of the loop, and this is
        // the returned-owner test the compiler could not fold away across the offsetof
        // subtraction. Kept because the asm keeps it.
        if (!lpSubMix)
            break;

        // Inlined strcmp @0x82B9FFC8..0x82B9FFE8, de-inlined per the de-optimization rule.
        // The asm's exit conditions (*name == 0, or a non-zero difference) plus the final
        // `difference == 0` test are exactly strcmp(...) == 0 for every prefix case.
        if (std::strcmp(lpCmd->maName, lpSubMix->mName) == 0)
        {
            SubMixConnector* lpConn = &lpSend->mSubMixConnector; // r11 = r30 + 0x30
            lpConn->mpSubMix = lpSubMix;                                     // stw 0xC(conn)
            lpConn->mpSubMixBuffer = lpSubMix->mpSubMixBuffer;               // lwz 0x24 -> stw 8
            lpConn->mNumSubMixChannels = static_cast<u8>(lpSubMix->mbNumChannels); // lbz 0x21 -> stb 0x10

            SubMixConnector* lpHead = lpSubMix->mpConnectorHead; // lwz 0x28(subMix)
            lpConn->mppPrev = 0;                                 // stw r9(=0), 4(conn)
            lpConn->mpNext = lpHead;                             // stw r8, 0(conn)
            if (lpHead)
                lpHead->mppPrev = reinterpret_cast<SubMixConnector**>(lpConn); // stw r11, 4(r9)
            lpSubMix->mpConnectorHead = lpConn;                  // stw r11, 0x28(subMix)
            break; // the asm falls straight through to the epilogue: FIRST match only
        }
    }

    // The record's own byte size == the ring advance. EventEvent stored the HOST size
    // here, so this is NOT the console `(strlen + 16) & ~3`.
    return static_cast<int>(lpCmd->muRecordSize); // lwz r3, 0(r3) where r3 == cmd+8
}

} // namespace core
} // namespace audio
} // namespace rw
