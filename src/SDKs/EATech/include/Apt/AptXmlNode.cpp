// ===========================================================================
// EATech Apt -- AptXmlNode bodies.   Reconstructed from the X360 ARTIST.XEX.
//   ctor            @ 0x82AF1B08
//   operator new    @ 0x82AE6F78   |  operator delete   @ 0x82AF1B60
//   `scalar deleting destructor'   @ 0x82AF5C28  (compiler thunk -- dropped;
//                                                 ~AptXmlNode below is the real
//                                                 destructor body)
//
// The ActionScript XMLNode object value: an AptObject (property hash + the
// mClassFlags class-flags word). It is a garbage-collected value, so it allocates
// from the GC value pool and participates in the GC mark/teardown of the base.
// The ctor's raw `AptValueWithHash(24,8) + inline mClassFlags zero/mask` asm is the
// compiler's INLINED AptObject(AptVFT_XmlNode, 8) ctor body.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptXmlNode.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue base
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"        // AllocateAptValueGC / DeallocateAptValueGC

// ---------------------------------------------------------------------------
// operator new @0x82AE6F78 -- GC-pool allocate + mark allocated.
//   v1 = DOGMA_PoolManager::Allocate(off_8324D834, size);          // off_8324D834 = gpGCPoolManager
//   AptValueGC_MemItem::SetIsAllocated(v1, byte_8324D804, 1);      // byte_8324D804 = gAptValueGCSizeOffset
//   return v1;
// The Allocate + SetIsAllocated(.,1) pair is the GC allocator's alloc operation
// (AllocateAptValueGC), folded inline by the X360. Guarded for null before
// AptAllocatorInitialize @0x82ADD118 (AptInit.cpp) wires gpGCPoolManager.
// ---------------------------------------------------------------------------
void* AptXmlNode::operator new(size_t size)
{
    return (gpGCPoolManager != nullptr) ? gpGCPoolManager->AllocateAptValueGC(size) : nullptr;
}

// ---------------------------------------------------------------------------
// operator delete @0x82AF1B60 -- GC-pool free + clear allocated (on success).
//   if (DOGMA_PoolManager::Deallocate(off_8324D834, p, size))      // off_8324D834 = gpGCPoolManager
//       AptValueGC_MemItem::SetIsAllocated(p, byte_8324D804, 0);
// That Deallocate + gated SetIsAllocated(.,0) is exactly DeallocateAptValueGC.
// ---------------------------------------------------------------------------
void AptXmlNode::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != nullptr)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF1B08
//   AptValueWithHash::AptValueWithHash(this, AptVFT_XmlNode /*24*/, 8);
//   this->mClassFlags = 0;                       // stb 0, 0x1C
//   this->mClassFlags &= 0xFF3FFFFF;             // rlwinm 0,10,7 -> clears bits 22,23 (no-op on 0)
//   *this = off_82145E54;                         // AptXmlNode vtable
// The class-flags word is initialised to zero, then bits 22,23 are masked off
// (the inlined hasClass/implemented-objects clear, identical to the AptObject /
// AptGlobalExtensionObject ctor idiom); the mask is a no-op on the just-zeroed
// word but is reproduced for parity with the asm. The vtable store the X360
// emits between the base ctor and the field setup is compiler-generated as part
// of entering the most-derived ctor body and is not hand-written.
// ---------------------------------------------------------------------------
AptXmlNode::AptXmlNode()
    : AptObject(AptVFT_XmlNode, KI_HASH_CAPACITY)
{
    // The X360 ctor's `mClassFlags = 0; mClassFlags &= 0xFF3FFFFF` is the inlined
    // AptObject base-ctor body (it already zeroes + masks mClassFlags), so nothing
    // extra is hand-written here now that AptObject is the base.
}

// Derived-class base ctor -- AptObject(eType, nHashCapacity) with the requested
// vtable index + property-hash capacity (e.g. AptXml: AptVFT_Xml, 0). Same inlined
// AptObject construction as the default ctor, just with the values forwarded.
AptXmlNode::AptXmlNode(AptVirtualFunctionTable_Indices eType, int nHashCapacity)
    : AptObject(eType, nHashCapacity)
{
}

// ---------------------------------------------------------------------------
// ~AptXmlNode -- empty: the embedded AptNativeHash (in the AptValueWithHash base)
// tears itself down. The `scalar deleting destructor' thunk @0x82AF5C28 sets this
// object's vtable, chains to the base teardown, then (with the delete flag)
// operator delete(this, 32) -- confirming AptXmlNode adds nothing over the base
// destructor and pinning sizeof == 0x20. The asm never touches +0x1C in the
// destructor (mClassFlags is a plain POD word).
// ---------------------------------------------------------------------------
AptXmlNode::~AptXmlNode()
{
}
