#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"

#include <new>  // placement new (the MassiveAd heap-hook construction idiom)

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"            // CMassiveList(Node), MassiveLog
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModel.h" // CMassiveAdObjectModel (the slave)

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::CreateSlaveM @ 0x82BDE850
//
// Spawns one CMassiveAdObjectModel "slave", seeds it from the master's own
// fields, hands it the next delivered asset id (and the subscriber, when there
// is one), then links it into mSlaveList. Returns the new slave, or 0 when the
// slave allocation failed.
//
// RAW asm (r31 = this, r28 = pSubscriber, r30 = the slave) -- transcribed from
// the .ida-exports listing, addresses inclusive:
//   82BDE85C  mr      r31, r3
//   82BDE860  li      r3, 0x64                  ; CONSOLE sizeof(CMassiveAdObjectModel)
//   82BDE864  mr      r28, r4                   ; pSubscriber
//   82BDE868  bl      CMassiveListNode::operator new
//   82BDE86C  mr.     r30, r3
//   82BDE870  beq     loc_82BDE8A8              ; allocation failed -> slave = 0
//   82BDE874  li      r7, 0                     ; base ctor arg5 (zone) = 0
//   82BDE878  lwz     r29, 0x60(r31)            ; the master's own min-size word
//   82BDE87C  mr      r3, r30
//   82BDE880  lwz     r6, 0x38(r31)             ; base ctor arg4 = mnField38
//   82BDE884  lwz     r5, 0x48(r31)             ; base ctor arg3 = mnInvElementID
//   82BDE888  lwz     r4, 0x14(r31)             ; base ctor arg2 = mpcAdObjectName
//   82BDE88C  bl      CMassiveAdObject::CMassiveAdObject   ; the INLINED slave ctor...
//   82BDE890  lis     r11, off_82187214@ha
//   82BDE894  clrlwi  r10, r29, 16              ; ...min size & 0xFFFF...
//   82BDE898  addi    r11, r11, off_82187214@l
//   82BDE89C  stw     r10, 0x60(r30)            ; ...stored on the slave...
//   82BDE8A0  stw     r11, 0(r30)               ; ...and the slave's own vftable
//   82BDE8A4  b       loc_82BDE8AC
//   82BDE8A8  li      r30, 0
//   82BDE8AC  cmplwi  cr6, r30, 0
//   82BDE8B0  bne     cr6, loc_82BDE8DC
//   82BDE8B4  lis     r11, aAllocationFail_38@ha
//   82BDE8B8  lwz     r6, 0x14(r31)             ; log arg = mpcAdObjectName
//   82BDE8BC  li      r3, 2                     ; log level
//   82BDE8C0  lwz     r4, 0xC(r31)              ; the BASE object name (GetName())
//   82BDE8C4  addi    r5, r11, aAllocationFail_38@l
//   82BDE8C8  bl      STUB                      ; the MassiveAd trace hook (MassiveLog)
//   82BDE8CC  li      r11, 0
//   82BDE8D0  li      r3, 0
//   82BDE8D4  stw     r11, 0x10(r31)            ; SetValid(0)
//   82BDE8D8  b       loc_82BDE954              ; return 0
//   82BDE8DC  stw     r31, 0x34(r30)            ; slave->mpMaster = this
//   82BDE8E0  mr      r3, r31
//   82BDE8E4  lwz     r11, 0(r31)
//   82BDE8E8  lwz     r11, 0x20(r11)
//   82BDE8EC  mtctr   r11
//   82BDE8F0  bctrl                             ; this->GetNextAssetID() (vftable +0x20)
//   82BDE8F4  mr      r4, r3                    ; r4 = the id
//   82BDE8F8  mr      r3, r30                   ; r3 = the SLAVE
//   82BDE8FC  stw     r4, 0x64(r31)             ; mnCurrentAssetID = that id
//   82BDE900  bl      CMassiveAdObject::AssetIDAdd  ; DIRECT bl, on the slave
//   82BDE904  cmplwi  cr6, r28, 0
//   82BDE908  beq     cr6, loc_82BDE924         ; if (pSubscriber)
//   82BDE90C  lwz     r11, 0(r30)
//   82BDE910  mr      r4, r28
//   82BDE914  mr      r3, r30
//   82BDE918  lwz     r11, 0x1C(r11)
//   82BDE91C  mtctr   r11
//   82BDE920  bctrl                             ; slave->SubscriberAdd(sub) (+0x1C)
//   82BDE924  li      r3, 0xC                   ; CONSOLE sizeof(CMassiveListNode)
//   82BDE928  bl      CMassiveListNode::operator new
//   82BDE92C  cmplwi  r3, 0
//   82BDE930  beq     loc_82BDE944
//   82BDE934  mr      r4, r30
//   82BDE938  bl      CMassiveListNode::CMassiveListNode  ; (node, slave)
//   82BDE93C  mr      r4, r3
//   82BDE940  b       loc_82BDE948
//   82BDE944  li      r4, 0
//   82BDE948  addi    r3, r31, 0x6C             ; &mSlaveList
//   82BDE94C  bl      CMassiveList::Append
//   82BDE950  mr      r3, r30                   ; return slave
//
// CONSOLE-vs-HOST (the recurring killer): both `li` sizes above -- 0x64 for the
// slave and 0xC for the list node -- are 32-bit X360 object widths. The host is
// LLP64, so every pointer in both objects is wider; the allocations below use
// `sizeof` and the console immediates stay in these comments. There is no
// console offset arithmetic in this body: the master's +0x60/+0x38/+0x48/+0x14
// reads and the slave's +0x34/+0x60/+0x00 writes are all reproduced as NAMED
// member accesses, and `addi r3, r31, 0x6C` is `&mSlaveList`.
//
// PPC float-ABI check (gotcha 3): the base-ctor chain loads r4, r5, r6, r7 with
// no gap and touches no FPR anywhere in the function, so no argument is a float
// here. The sibling AudioDynamic::CreateSlaveM is the SAME shape -- its base-ctor
// call is likewise four GPR args with no float argument; only its +0x60 MEMBER is
// a float (lfs f31). There is no floating-point compare in this body either,
// so the NaN-polarity rule does not apply.
//
// The Hex-Rays decode of this function drops the `clrlwi` (it renders the min
// size copy as a plain `v5[24] = v6`) and drops AssetIDAdd's second argument --
// the raw disassembly above is what is reproduced here.
// ---------------------------------------------------------------------------
CMassiveAdObject* CMassiveAdObjectModelDynamic::CreateSlaveM(
    CMassiveAdObjectSubscriber* pSubscriber)
{
    void* lpSlaveMem = CMassiveListNode::operator new(sizeof(CMassiveAdObjectModel)); // li r3, 0x64
    CMassiveAdObjectModel* lpSlave = lpSlaveMem
        ? ::new (lpSlaveMem) CMassiveAdObjectModel(
              mpcAdObjectName,  // lwz r4, 0x14(this)
              mnInvElementID,   // lwz r5, 0x48(this)
              muField60,        // lwz r29, 0x60(this) -- the ctor applies the clrlwi mask
              mnField38,        // lwz r6, 0x38(this)
              0)                // li  r7, 0 (the slave takes no zone; the base ctor
                                //            falls back to the current zone)
        : 0;

    if (!lpSlave)
    {
        // aAllocationFail_38 -- the rodata literal verbatim, TWO spaces after the
        // full stop. r4 is the BASE object name (+0x0C, i.e. GetName()) and r6 the
        // ad-object name (+0x14): two DIFFERENT strings on the X360, not a
        // duplicate.
        MassiveLog(2, GetName(), "ALLOCATION Failed for CMassiveAdObjectModel.  MAO: %s",
                   mpcAdObjectName);
        SetValid(0);  // stw 0, 0x10(this)
        return 0;
    }

    lpSlave->mpMaster = this;               // stw this, 0x34(slave)
    mnCurrentAssetID = GetNextAssetID();    // virtual on THIS (vftable +0x20); stw +0x64
    lpSlave->AssetIDAdd(mnCurrentAssetID);  // direct bl on the SLAVE (r4 = the id just stored)
    if (pSubscriber)
        lpSlave->SubscriberAdd(pSubscriber);  // virtual on the SLAVE (vftable +0x1C)

    void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode)); // li r3, 0xC
    CMassiveListNode* lpNode =
        lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpSlave) : 0;
    mSlaveList.Append(lpNode);  // Append(this + 0x6C, node); a null node is the
                                // X360's own no-op path (Append returns 0)
    return lpSlave;
}

} // namespace MassiveAdClient3
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"

#include <cstring>  // strlen (the X360 calls it and discards the result)

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"            // CMassiveList
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"  // Instance / GetCurrentZone
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h" // mPreSubscriberList
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"  // CMassiveAdObjectSubscriber::mpcName

// External MassiveAd string-compare helper (`bl CompareStrings` @ 0x82BDEF50).
// Returns 0 when the two NUL-terminated strings are equal -- the strcmp
// convention the X360 branches on. It demangles WITHOUT a namespace (a free
// vendor helper), so it is declared at FILE scope, before the namespace block,
// exactly as MassiveAdClient3ZoneManager.cpp declares it.
extern int CompareStrings(const char* pcA, const char* pcB);

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::Initialize @ 0x82BDEEF8
//
// Binds this composite to the client's CURRENT zone, then spawns one slave per
// already-queued pre-subscriber whose slot name matches this ad object's name,
// and finally writes 2 into the base state dword.
//
// RAW asm (r31 = this, r30 = the current subscriber):
//   82BDEF0C  mr      r31, r3
//   82BDEF10  bl      CMassiveClientCore::Instance
//   82BDEF14  bl      CMassiveClientCore::GetCurrentZone   ; chained on the instance
//   82BDEF18  cmplwi  r3, 0
//   82BDEF1C  stw     r3, 0x18(r31)          ; mpZone = <zone>  (stored either way)
//   82BDEF20  beq     loc_82BDEF84           ; no zone -> straight to the tail
//   82BDEF24  addi    r3, r3, 0x28           ; &zone->mPreSubscriberList
//   82BDEF28  bl      CMassiveList::GoToStart
//   82BDEF2C  lwz     r3, 0x14(r31)          ; mpcAdObjectName
//   82BDEF30  bl      strlen                 ; result DISCARDED (a real dead call)
//   82BDEF34  b       loc_82BDEF74           ; -> the bottom-tested loop
// loc_82BDEF38:
//   82BDEF38  lwz     r11, 0x18(r31)
//   82BDEF3C  addi    r3, r11, 0x28
//   82BDEF40  bl      CMassiveList::GetCurrData
//   82BDEF44  mr      r30, r3                ; the queued subscriber
//   82BDEF48  lwz     r3, 0x14(r31)          ; mpcAdObjectName
//   82BDEF4C  lwz     r4, 4(r30)             ; subscriber->mpcName
//   82BDEF50  bl      CompareStrings
//   82BDEF54  cmplwi  r3, 0
//   82BDEF58  bne     loc_82BDEF68           ; 0 == the names match
//   82BDEF5C  mr      r4, r30
//   82BDEF60  mr      r3, r31
//   82BDEF64  bl      CMassiveAdObjectModelDynamic::CreateSlaveM
// loc_82BDEF68:
//   82BDEF68  lwz     r11, 0x18(r31)
//   82BDEF6C  addi    r3, r11, 0x28
//   82BDEF70  bl      CMassiveList::GoToNext
// loc_82BDEF74:
//   82BDEF74  lwz     r11, 0x18(r31)
//   82BDEF78  lwz     r11, 0x30(r11)         ; zone+0x30 == mPreSubscriberList's cursor
//   82BDEF7C  cmplwi  r11, 0                 ; (list +0x28, its mpCurrent at +0x08)
//   82BDEF80  bne     loc_82BDEF38           ; while (cursor)
// loc_82BDEF84:
//   82BDEF84  li      r11, 2
//   82BDEF88  stw     r11, 0x10(r31)         ; base state dword = 2  -> SetValid(2)
//   82BDEFA0  blr                            ; r3 is NOT set by this function
//
// CONSOLE-vs-HOST (gotcha 1): every console offset in the listing is reproduced
// as a NAMED member on the host -- +0x18 is mpZone, +0x14 is mpcAdObjectName,
// zone+0x28 is mPreSubscriberList, zone+0x30 is that list's own mpCurrent
// (GetCurrent(), list +0x08), subscriber+0x04 is mpcName and +0x10 is the base
// state dword. None of those immediates survives into the code. There is no
// floating-point compare in this body, so the NaN-polarity rule does not apply.
//
// The instance pointer is used WITHOUT a null check (the two calls are chained
// back to back), and the zone is stored into mpZone BEFORE the null test
// (`stw` @ 0x82BDEF1C sits between the compare and the branch), so a null
// current zone still clears the member -- both reproduced below.
//
// SetValid(2): the STORE is MEASURED (li 2 / stw 0x10). The MEANING of the
// value 2 is INFERRED and NOT corroborated -- the base CMassiveAdObject::
// Initialize @ 0x82BD6FD0 stores no state value of its own -- it is
// `mr r4, r3; b BaseFindPreSubscribers` @ 0x82BD5B30, which tail-routes into
// CMassiveZoneManager::PreSubscriberAssignT; no state dword is written anywhere
// on that path, so nothing in the recovered code labels 2 as
// "initialised"; it is simply the value this override writes into the shared
// base state dword. Do not build on that reading without further evidence.
//
// Return value: the X360 leaves whatever the last helper put in r3. On the
// no-zone path that is GetCurrentZone's 0, and on the drained-loop path it is
// GoToNext's exhausted cursor, also 0; only the never-entered-loop path (a zone
// with an empty pre-subscriber queue) leaves strlen's length there. No recovered
// caller reads the result, so this is modelled as `return 0` -- the same stance
// the committed Suspend/Resume take for their exhausted-cursor returns.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::Initialize()
{
    mpZone = CMassiveClientCore::Instance()->GetCurrentZone();  // stw r3, 0x18(this)
    if (mpZone)
    {
        mpZone->mPreSubscriberList.GoToStart();  // zone + 0x28
        (void)std::strlen(mpcAdObjectName);      // bl strlen -- the result is discarded

        while (mpZone->mPreSubscriberList.GetCurrent())  // lwz zone+0x30 (the cursor)
        {
            CMassiveAdObjectSubscriber* lpSubscriber =
                static_cast<CMassiveAdObjectSubscriber*>(
                    mpZone->mPreSubscriberList.GetCurrData());

            // CompareStrings == 0 is the match (strcmp convention).
            if (!CompareStrings(mpcAdObjectName, lpSubscriber->mpcName))  // subscriber + 0x04
                CreateSlaveM(lpSubscriber);

            mpZone->mPreSubscriberList.GoToNext();
        }
    }

    SetValid(2);  // stw 2, 0x10(this) -- store MEASURED, the value's meaning INFERRED
    return 0;     // see the return-value note above
}

} // namespace MassiveAdClient3
