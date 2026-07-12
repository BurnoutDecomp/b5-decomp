#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectModelDynamic definition home.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (no leak
// source / DecFIGS). Stores/branches are reproduced against the X360
// disassembly; see MassiveAdClient3ObjectModelDynamic.h for the per-offset
// layout map and for why CreateSlaveM / GetNextAssetID / Initialize are blocked.
// The class installs its own X360 vftable (off_821872B4) over the
// CMassiveAdObject slot -- modelled by the virtual destructor, so no vftable
// store is written by hand.
//
// The composite lifecycle passes (Tic / Rep / Resume) all share the same X360
// shape: reset the slave-list cursor to the head, then for every slave under the
// cursor dispatch one virtual on the slave and advance. The X360 loop-continue
// test reads the list cursor (mSlaveList +0x08, i.e. host GetCurrent());
// GetCurrData() yields the node payload -- a CMassiveAdObject* slave -- which the
// composite drives by NAME. (This subtype has no Suspend override -- unlike the
// audio-dynamic sibling -- so it is not defined here.)
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::CMassiveAdObjectModelDynamic @ 0x82BDAD58
//
// asm:
//   mr   r6, r7                          ; base ctor arg4 <- a7 (incoming r7)
//   mr   r7, r8                          ; base ctor arg5 <- a8 (incoming r8)
//   lwz  r30, 0x60(r31)                  ; preserve muField60 across the base call
//   bl   CMassiveAdObject::CMassiveAdObject  ; r3=this, r4=name, r5=a3, r6=a7, r7=a8
//   clrlwi r9, r30, 16                   ; r9 = muField60 & 0xFFFF
//   lis/addi off_821872B4                ; install this class's vftable
//   stw  r9,  0x60(r31)                  ; restore muField60 (masked to 16 bits)
//   stw  off_821872B4, 0(r31)            ; vftable
//   stw  0,   0x64(r31)                  ; mnCurrentAssetID   = 0
//   sth  0,   0x68(r31)                  ; muActiveAssetCount = 0
//   stw  0,   0x6C(r31) .. stw 0, 0x78(r31); mSlaveList head/tail/cursor/count = 0
// The base ctor receives (name, a3, a7, a8) -- the incoming a6 (register r6) is
// overwritten by a7 before the chain, so it never reaches the base and is unused
// here. muField60 is loaded, masked to its low 16 bits, then written straight
// back around the base call (the base does not touch +0x60): a compiler-scheduled
// preserve of the caller-set value that is a semantic no-op for a 16-bit field,
// so it is not initialised here. The vftable install is modelled by the virtual
// dtor.
// ---------------------------------------------------------------------------
CMassiveAdObjectModelDynamic::CMassiveAdObjectModelDynamic(const char* pcName,
                                                           int a3, int a6, int a7,
                                                           int a8)
    : CMassiveAdObject(pcName, a3, a7, a8)  // bl base ctor (name, a3, a7, a8)
    , mnCurrentAssetID(0)                    // stw 0, 0x64
    , muActiveAssetCount(0)                  // sth 0, 0x68
    , mSlaveList()                           // stw 0, 0x6C..0x78 (empty list)
{
    (void)a6;  // a6 (r6) is clobbered before the base chain and never used
    // muField60 (+0x60) is preserved (loaded/masked/restored) across the base
    // ctor, not set by this ctor -- see the header layout note.
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::~CMassiveAdObjectModelDynamic @ 0x82BDE7B8
//   (the scalar deleting destructor @ 0x82BDADD0 wraps this)
//
// asm: install vftable (off_821872B4); GoToStart(mSlaveList); while (cursor):
//      slave = GetCurrData(); if (slave) (**slave)(slave, 1)  [slave scalar
//      deleting destructor with the delete flag == `delete slave`]; GoToNext();
//      RemoveAll(mSlaveList); ~CMassiveList(mSlaveList); ~CMassiveAdObject(this).
// The vftable install + the ~CMassiveList / base-dtor chain are the compiler-
// emitted virtual-dtor body -- mSlaveList's own CMassiveList destructor and the
// CMassiveAdObject base destructor run automatically, in that order, after this
// body. The explicit slave-teardown loop (deleting each spawned slave, then
// RemoveAll on the node list) is the reconstructed source body.
// ---------------------------------------------------------------------------
CMassiveAdObjectModelDynamic::~CMassiveAdObjectModelDynamic()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        CMassiveAdObject* pSlave =
            static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData());
        if (pSlave)
            delete pSlave;  // (**slave)(slave, 1): slave deleting destructor
        mSlaveList.GoToNext();
    }
    mSlaveList.RemoveAll();
    // mSlaveList's ~CMassiveList and the CMassiveAdObject base dtor run
    // automatically here (the X360 calls ~CMassiveList then ~CMassiveAdObject).
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::Tic @ 0x82BDEAD8
//
// Ticks each slave (slave vftable +0x0C) and returns the first non-zero result
// (a stop/error code); 0 when every slave returned 0.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::Tic()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        CMassiveAdObject* pSlave =
            static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData());
        int nResult = pSlave->Tic();
        if (nResult)
            return nResult;
        mSlaveList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::Rep @ 0x82BDEB50
//
// Reports each slave's impressions (slave vftable +0x10, ReportImpressions) and
// returns the first non-zero result; 0 when every slave returned 0.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::Rep()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        CMassiveAdObject* pSlave =
            static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData());
        int nResult = pSlave->ReportImpressions();
        if (nResult)
            return nResult;
        mSlaveList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::Resume @ 0x82BDEC38
//
// Resumes every slave (slave vftable +0x18). The X360 returns the last list-walk
// result, which is the exhausted cursor (null) once the loop drains -- modelled
// as 0.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::Resume()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        CMassiveAdObject* pSlave =
            static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData());
        pSlave->Resume();
        mSlaveList.GoToNext();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::SetAssetExpired @ 0x82BDEA58
//
// asm: chain base SetAssetExpired(this, nAssetId); if it returns 0, return 0.
//      Otherwise decrement muActiveAssetCount (leaving 0 when already 0), clear
//      mnCurrentAssetID if it equals the expiring id, and return 1.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::SetAssetExpired(int nAssetId)
{
    if (!CMassiveAdObject::SetAssetExpired(nAssetId))
        return 0;

    if (muActiveAssetCount)
        --muActiveAssetCount;
    else
        muActiveAssetCount = 0;

    if (mnCurrentAssetID == nAssetId)
        mnCurrentAssetID = 0;

    return 1;
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::Sub @ 0x82BDECA8
//
// asm: if (pSubscriber == 0) return SetLastError(this, -500, &unk_820046A7);
//      else CreateSlaveM(this, pSubscriber); return 0. (&unk_820046A7 is the
//      shared un-exported error-format rodata the leaf SetLastError ignores --
//      modelled as a null format, per the established MassiveAd convention.)
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::Sub(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, reinterpret_cast<const char*>(0)); // &unk_820046A7

    CreateSlaveM(pSubscriber);
    return 0;
}

} // namespace MassiveAdClient3
