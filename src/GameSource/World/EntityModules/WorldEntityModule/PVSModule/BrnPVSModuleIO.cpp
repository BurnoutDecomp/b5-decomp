// ============================================================================
// GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModuleIO.cpp
//
// BrnWorld::PVSIO::OutputBuffer out-of-line member bodies, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This slice bodies OutputBuffer::Release @ 0x822EE6D8.
//
// The FEB-2007 partial source models Release() as `{ Clear(); return true; }`,
// where OutputBuffer::Clear() == `mZoneResponseQueue.Clear(); mGameDataRequestInterface.Clear();`.
// The X360 ARTIST build inlines that to exactly two effects then returns true:
//   *(this + 8) = 0;                                  // mZoneResponseQueue.miLength = 0  (stw 0,8(this))
//   mGameDataRequestInterface.mRequestQueue.Clear();  // VariableEventQueue<512,16>::Clear @ this+0x1718
// Confirmed against CgsBaseEventQueue.h: BaseEventQueue<T> lays out mpEvents@+0,
// miMaxLength@+4, miLength@+8; the EventQueue<GetZoneResponse,8> base is the first
// member of OutputBuffer (@+0), so the +8 store is that queue's length-clear ==
// mZoneResponseQueue.Clear(). mGameDataRequestInterface (RequestInterface<512>) holds its
// RequestQueue<512> at offset 0 (==this+0x1718); RequestQueue<512> derives
// ResourceRequestQueue<512> : VariableEventQueue<512,16>, so mRequestQueue.Clear()
// resolves to the shared VariableEventQueue<512,16>::Clear instantiation (the asm bl target).
// ============================================================================

#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h"

namespace BrnWorld
{
namespace PVSIO
{
    // X360 0x822D8C20. Readies the input buffer for a new frame and reports success.
    // Store-for-store: reset the zone-request queue length (this+8 -> miLength=0),
    // return true (li r3,1).
    bool InputBuffer::Prepare()
    {
        mZoneRequestQueue.Clear();   // miLength = 0  -> stw 0,8(this)
        return true;
    }

    // X360 0x822D8C38. Tears down the input buffer. Store-for-store: reset the
    // zone-request queue length (this+8 -> miLength=0). No return value in the asm
    // (li r11,0; stw r11,8(r3); blr).
    void InputBuffer::Destruct()
    {
        mZoneRequestQueue.Clear();   // miLength = 0  -> stw 0,8(this)
    }

    // X360 0x822EE6A0. Readies the output buffer for a new frame and reports success.
    // Same store-for-store effects as Release @0x822EE6D8: reset the zone-response
    // queue length (this+8 -> miLength=0), Clear the embedded game-data request
    // interface (this+0x1718), return true.
    bool OutputBuffer::Prepare()
    {
        mZoneResponseQueue.Clear();                      // miLength = 0  -> stw 0,8(this)
        mGameDataRequestInterface.mRequestQueue.Clear(); // VariableEventQueue<512,16>::Clear @ this+0x1718
        return true;
    }

    // X360 0x822EE710. Tears down the output buffer. Store-for-store: reset the
    // zone-response queue length (this+8 -> miLength=0), then tail-call
    // VariableEventQueue<512,16>::Clear on the embedded game-data request interface
    // (this+0x1718). No own return value (the asm `b` tail-jumps into Clear).
    void OutputBuffer::Destruct()
    {
        mZoneResponseQueue.Clear();                      // miLength = 0  -> stw 0,8(this)
        mGameDataRequestInterface.mRequestQueue.Clear(); // tail: VariableEventQueue<512,16>::Clear @ this+0x1718
    }

    // X360 0x822EE6D8. Clears the output buffer for reuse and reports success.
    // Store-for-store: reset the zone-response queue length (this+8 -> miLength=0),
    // Clear the embedded game-data request interface (this+0x1718), return true.
    bool OutputBuffer::Release()
    {
        mZoneResponseQueue.Clear();                  // miLength = 0  -> stw 0,8(this)
        mGameDataRequestInterface.mRequestQueue.Clear(); // VariableEventQueue<512,16>::Clear @ this+0x1718
        return true;
    }
}
}
