#ifndef RW_GPFX_COLOUR_CUBE_H
#define RW_GPFX_COLOUR_CUBE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5>

// rw::graphics::postfx::ColourCube -- the post-fx colour-grading lookup volume: an N x N x N RGB
// cube (3 bytes per cell) preceded by a 16-byte header. GetResourceDescriptor sizes the rw
// resource for it from the cube edge length carried in the Parameters block.
//
// Definition lives here so both the SDK home (rwgpfxcolourcube.cpp) and the game-side resource-
// type handler (CgsRwColourCubeResourceType.cpp) share one declaration.
namespace rw::graphics::postfx
{
    class ColourCube
    {
    public:
        // The colour-cube build parameters. DWARF rwgpfxcolourcube.h:17-18 declares exactly one
        // member -- `uint32_t size` -- and GetResourceDescriptor reads exactly that word (X360 *a2).
        struct Parameters
        {
            u32 size;   // +0x00 N: the cube is N x N x N RGB cells
        };

        // X360 0x82402C48 -- size the rw resource for the colour cube.
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpResult,
                                                                     const Parameters* lpParameters);

        // The serialised header this class IS. Both constants are the ones
        // GetResourceDescriptor itself spends (`3 * N^3 + 16`), so neither is invented:
        //   KU_HEADER_BYTES    the `+ 16` the descriptor adds for this header
        //   KU_BYTES_PER_CELL  the `3 *` it multiplies the cell count by
        static const u32 KU_HEADER_BYTES   = 16u;
        static const u32 KU_BYTES_PER_CELL = 3u;

        // ⭐ THE MEMBER AND ACCESSOR NAMES ARE THE DWARF'S (corrected 2026-08-16, step-10 fix
        // round). references/DecFIGS/dwarfdump/SDKs/RenderEngineClub/MAIN/components/include/postfx/
        // rwgpfxcolourcube.h declares this class as exactly two data members and their accessors:
        //     uint32_t   m_size;             // rwgpfxcolourcube.h:41
        //     uint8_t *  m_pixels;           // rwgpfxcolourcube.h:42
        //     uint32_t   GetSize() const;    // :27
        //     uint8_t *  GetPixels() const;  // :28
        // An earlier cut invented muEdgeLength / muCellsOffset / GetEdgeLength() / GetCells() for a
        // class whose real names rung 2 supplies. The OFFSETS were right either way -- m_size@+0x00,
        // m_pixels@+0x04 is the `lwz r8, 4(r8)` BrnPostFx::BeginTintBlend @0x823F83F8 reads -- so
        // this is a naming correction, not a layout one. (Fill(uint8_t*) and GetParameters are in
        // the DWARF too and are deliberately NOT declared here: nothing on the X360 tint path calls
        // them, and AGENTS.md gates every DWARF declaration on attestation + need.)
        u32 GetSize() const { return m_size; }

        // The packed RGB cell volume. On the console this is a POINTER read straight out of
        // the header -- BrnPostFx::BeginTintBlend @0x823F83F8 does `lwz r8, 4(r8)` on the
        // ColourCube and stores the result as TintBlendParameters::src[i].
        //
        // ⚠ WHY m_pixels KEEPS THE CONSOLE'S FOUR-BYTE WIDTH, AND WHY THIS RECOMPUTES RATHER THAN
        // READS IT. The DWARF types it `uint8_t*`, which is FOUR bytes on the 32-bit console and
        // eight here -- but the ColourCube is used IN PLACE inside its serialised resource: the
        // whole object is `3*N^3 + 16` contiguous bytes with this header at the front, because
        // ColourCube::GetResourceDescriptor @0x82402C48 asks for exactly that in slot0 and nothing
        // else. Widening the member would move every cell in the blob and would need every shipped
        // cube re-converted (the same narrow-slot problem envdata recorded for the Keyframe's +0x80
        // import). So the slot keeps its on-disk width and the address it names -- resourceBase + 16
        // -- is recomputed from the same constant the descriptor spends. MEASURED on the shipped
        // data: every retail cube's +0x04 word is 0x00000010 (ENVIRONMENTSETTINGS/COLOURCUBES/
        // PARADISE_INGAME_JUNK.BUNDLE and all four resources of POSTFX/COLOURCUBEDICTIONARY.BIN,
        // all 98,320 bytes = 3*32^3 + 16).
        //
        // Raw byte-offset access into a SERIALISED BLOB, which AGENTS.md permits and expects
        // ("Exception -- external serialised / platform data"): the 16-byte header + packed
        // cell volume is a file layout, not a C++ object layout.
        const u8* GetPixels() const
        {
            return reinterpret_cast<const u8*>(this) + KU_HEADER_BYTES;
        }

        u32 m_size;        // +0x00 N (read by RwColourCubeResourceType::GetSerialisedResourceDescriptor)
        u32 m_pixels;      // +0x04 DWARF `uint8_t * m_pixels`, held at the CONSOLE's four-byte width:
                           //       on the serialised blob it is the RELATIVE OFFSET 16 that the
                           //       console's fix-up relocates in place. Reached through GetPixels().
        u32 m_reserved08;  // +0x08 zero in every shipped cube
        u32 m_reserved0C;  // +0x0C zero in every shipped cube
    };

    // The object IS the serialised header, and GetPixels() depends on it being exactly that.
    static_assert(sizeof(ColourCube) == ColourCube::KU_HEADER_BYTES,
                  "rw::graphics::postfx::ColourCube must stay the serialised 16-byte cube header");
}

#endif // RW_GPFX_COLOUR_CUBE_H
