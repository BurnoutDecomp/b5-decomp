#pragma once

#include "types.hpp"

// renderengine::VertexDescriptor -- the platform vertex-format / declaration object, sibling to
// renderengine::VertexBuffer and renderengine::TextureState (same X360 renderengine vendor layer).
// It owns the parsed vertex-element table (stream / offset / type / method / usage per element), the
// per-stream stride table, and the created Direct3D vertex declaration. Initialize parses the
// RenderWare vertex-format parameters into this table, computes per-stream strides via
// renderengine::VertexFormatGetStride, then CreateD3DObject marshals the table into a
// D3DVERTEXELEMENT9 array and calls D3DDevice_CreateVertexDeclaration. GetResourceDescriptor sizes
// the rw resource (used by every per-vertex-type Construct), and GetParameters / Release read the
// table back out and tear the D3D object down.
//
//   CreateD3DObject          0x82B611A0
//   GetParameters            0x82B61260
//   GetResourceDescriptor    0x82B637D0
//   Initialize               0x82B63060
//   Release                  0x82B63250
//
// The reconstruction preserves the X360 object layout and the asm store order so the
// resource-descriptor / fix-up pipeline marshals the same bytes; member accesses are by name.

namespace renderengine
{
    // Opaque platform handles (XDK D3D9 / Xenos). HONEST PLACEHOLDER: external platform types with
    // no Burnout-side home; only what the call sites need is declared. Not reconstructed here.
    struct D3DVertexDeclaration;

    // One parsed vertex element, 16 (0x10) bytes. Mirrors the X360 record the engine lays out at
    // descriptor +0x10 + 0x10*index: a u16 stream, u16 byte-offset-within-stream, u32 RenderWare
    // format/type code, then method / usage / usageIndex bytes. (The trailing bytes overlap the D3D
    // marshalling view; the named fields are the ones the bodies read/write.)
    struct VertexElement
    {
        u16 muStream;       // +0x00  stream index
        u16 muOffset;       // +0x02  byte offset within the stream
        u32 muType;         // +0x04  RenderWare element format code (fed to VertexFormatGetStride)
        u8  mauTail[4];     // +0x08  method @+0x08, usage @+0x09, usageIndex @+0x0A, pad @+0x0B
        u32 muReserved0C;   // +0x0C  (written from the parameters block in Initialize)
    };

    // The vertex-format descriptor object (the struct the wrapper's [0] points at).
    //   +0x00  mpDeclaration   created D3DVertexDeclaration*
    //   +0x04  muStreamMask    bitmask of used usages (|= 1<<usage)
    //   +0x08  muElementCount  number of valid elements (u16)
    //   +0x0A  muInitialised   set to 1 once built (u16)
    //   +0x0C  muType2Mask     bitmask of streams whose param[3]==2 (u16)
    //   +0x10  maElements[16]  the VertexElement table (0x10 each)
    //   +0x110 mauStreamStride[..]  per-stream stride bytes (one byte per element), laid out at
    //                          +0x10*(count+1); named separately because the X360 packs it after
    //                          the element table. Modelled inside the element-table window below.
    struct VertexDescriptorData
    {
        D3DVertexDeclaration* mpDeclaration;   // +0x00
        u32                   muStreamMask;    // +0x04
        u16                   muElementCount;  // +0x08
        u16                   muInitialised;   // +0x0A
        u16                   muType2Mask;     // +0x0C
        u16                   muPad0E;         // +0x0E
        VertexElement         maElements[16];  // +0x10 .. +0x10F
        u8                    mauStreamStride[16]; // +0x110  per-element stride byte
    };

    // The wrapper Initialize is handed: [0] -> the descriptor object above. (X360: a1[0] = object.)
    struct VertexDescriptorWrapper
    {
        VertexDescriptorData* mpData;   // +0x00
    };

    class VertexDescriptor
    {
    public:
        // 0x82B611A0 -- marshal the parsed element table into a D3DVERTEXELEMENT9 array (terminated
        // by a D3DDECL_END sentinel) and create the D3D vertex declaration; store it at +0x00.
        static D3DVertexDeclaration* CreateD3DObject(VertexDescriptorData* lpData);

        // 0x82B61260 -- read the element table back out into a 16-record buffer, padding the unused
        // tail records with the D3D end sentinel pattern. Returns lpData.
        static VertexDescriptorData* GetParameters(VertexDescriptorData* lpData, u32* lpParamsOut);

        // 0x82B637D0 -- build the rw resource descriptor (four {size=0, align=1} seeded entries, then
        // slot0 = {size=4, align = 0x11*validElements + 0x10}). Returns lpDescriptorOut.
        static u64* GetResourceDescriptor(u64* lpDescriptorOut, int a2);

        // 0x82B63060 -- parse the RenderWare vertex-format parameters at a2 into the element table,
        // compute per-stream strides, set the masks, then CreateD3DObject. Returns the object.
        static VertexDescriptorData* Initialize(VertexDescriptorWrapper* lpWrapper, int a2);

        // 0x82B63250 -- release the D3D declaration if present and clear the pointer. Returns lpData.
        static VertexDescriptorData* Release(VertexDescriptorData* lpData);
    };

    // X360 D3D9 externals the VertexDescriptor paths call. HONEST PLACEHOLDER: external platform APIs
    // with no Burnout-side home; only the signatures the call sites need are declared.
    D3DVertexDeclaration* D3DDevice_CreateVertexDeclaration(const void* lpVertexElements);
    int                   D3DResource_Release(D3DVertexDeclaration* lpThis);

    // Decrement / detach the D3D resource ref before release (X360 sub_82B61F90, a renderengine
    // state helper reconstructed elsewhere). Declared so Release can call it by name.
    void VertexDescriptor_PreRelease(D3DVertexDeclaration* lpDeclaration);

    // The per-format default type/method lookup table the X360 reads from &unk_8214AD50 when a
    // parameter byte is 0xFF. HONEST PLACEHOLDER: a const data blob in the X360 image; declared so
    // the parse reads it by name. Sized generously; only indexed entries are touched.
    extern const u8 gauVertexFormatDefaults[256];
}
