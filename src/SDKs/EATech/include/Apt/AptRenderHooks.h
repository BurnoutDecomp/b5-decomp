#pragma once

// ===========================================================================
// EATech Apt -- the Apt -> RenderWare render-callback boundary.
//
// The Apt player does not rasterise anything itself: when it walks the render
// tree it sets the current transform/colour on the context (forwarded to the host
// via gAptFuncs.pfnSetVertexMatrix/pfnSetColourTransform) and, at each shape,
// calls a host "draw" callback that triangulates + fills the vector-shape
// geometry on the GPU. Those draw callbacks are the boundary between this engine
// and the RenderWare 2D vector renderer (a SEPARATE subsystem -- the host's RW
// graphics integration installs them; on PC that is the RW/D3D 2D path).
//
// Console: the callbacks are global function pointers in the Apt user-function
// table, reached by AptCharacter::render (@0x810E74):
//   dword_1059C6B8  draw a shape's geometry             -> AptHook_DrawShape
//   dword_1059C6A4  draw an imported character's glyph   -> AptHook_DrawImportGlyph
//   dword_1059C69C  resolve an imported character id     -> AptHook_ResolveImport
//
// FLAG: these are homed by the host RW-render integration (not reconstructed
// here -- the RW 2D rasteriser is its own subsystem). Declared so the geometry
// dispatch in AptCharacter::render compiles + links against the boundary. Note:
// the console passes the shape's geometry sub-field (char[8]) directly; the PC
// hook takes the whole AptCharacter and reads the geometry from it (the host hook
// owns the loaded-shape layout), which keeps this side free of the .apt loaded
// layout (defined by the .apt parse).
// ===========================================================================

struct AptCharacter;
struct AptRenderingContext;
enum AptMaskRenderOperation : int;

void AptHook_DrawShape(AptCharacter* pShape, AptMaskRenderOperation eOp, int nTick);
void AptHook_DrawImportGlyph(AptCharacter* pImport, int nIndex, void* pGlyphData);
int  AptHook_ResolveImport(void* pImportFileData, int nImportId);
