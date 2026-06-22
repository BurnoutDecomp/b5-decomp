#pragma once

#include "types.hpp"

// renderengine namespace-scope helpers (the "renderengine" facade TU). Two free functions:
//
//   renderengine::VertexFormatGetStride  0x82B61310  -- bytes for a vertex-element format code
//   renderengine::TextureS               0x82856560  -- texture-dictionary lookup that returns
//                                                       the stored renderengine::Texture* (the
//                                                       IDA-demangled name is truncated; it is a
//                                                       dictionary find guarded by the table's
//                                                       initialised flag, returning entry + 0x0C)
//
// VertexFormatGetStride maps a RenderWare vertex-element format identifier (a 32-bit code) to the
// element's byte size; it is a pure switch reconstructed from the X360 image. TextureS is a thin
// wrapper over the texture dictionary's internal find (renderengine_, reconstructed in the
// dictionary's own TU); declared external here so the wrapper links by name.

namespace renderengine
{
    // Bytes occupied by one vertex element of the given format code (0 = unknown / unsized).
    int VertexFormatGetStride(int liFormatCode);

    // The texture dictionary's internal find (HashTable lookup). Returns the entry whose value
    // sits at +0x0C, or 0 if absent. HONEST PLACEHOLDER: reconstructed in the dictionary TU.
    int renderengine_dictionary_find(int liDictionary, unsigned int luKey, int* lpExtra);

    // Look a texture up in the dictionary at liDictionary; returns the stored Texture* (entry +
    // 0x0C) or 0. Asserts the dictionary's HashTable is initialised (flag at +0x12C).
    int TextureS(int liDictionary, int liKey);
}
