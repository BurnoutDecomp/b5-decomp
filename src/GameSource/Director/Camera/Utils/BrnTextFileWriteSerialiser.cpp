// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp
//
// Compilation home for the BrnDirector::Camera::TextFileWriteSerialiser slice this TU
// owns:
//   - TextFileWriteSerialiser::FormatName(char* dest, const char* src) @0x821F6128
//
// Called by every Camera serialise template instance
//   (BrnDirector::Camera::TextFileWriteSerialiser::Serialise<...>) to build the
// per-field label written to the tunings text file.
//
// Store-for-store from the asm at 0x821F6128:
//   v4   = 4 * (*this)               ; *this == muRecursionDepth (lwz r10,0(r3); slwi ,2)
//   v7   = strlen(src)               ; the trailing strlen scan (lbz/addi/cmplwi/bne)
//   v6   = 0                         ; running output index (r11)
//   if (v4 != 0):                    ; cmplwi r10,0 ; beq skip
//       for k in [0, v4):  dest[k] = '_'   ; mtctr r10 ; stb '_' loop
//       v6 = v4
//   v10  = v7 + v4                   ; add r8,r8,r10  (total length)
//   while (v6 < v10):                ; cmplw r11,r8 ; bge skip ; blt loop
//       ch = src[v6 - v4]            ; subf r10,r10,r11 ; add r9,r10,r5 ; lbz
//       dest[v6] = (ch == ' ') ? '_' : ch    ; cmplwi 0x20 ; stbx
//       ++v6
//   dest[v6] = 0                     ; stbx r6(=0), r11, r4  (NUL terminate)
//
// (r3/this is read for the depth but never written; the X360 returns it unchanged, which
// is a leftover-register artifact -- the DWARF signature returns void.)
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

void TextFileWriteSerialiser::FormatName(char* lpcDest, const char* lpcSrc)
{
    const u32 luIndent = 4u * muRecursionDepth;   // v4: 4 underscores per nesting level

    // strlen(src) -- the X360 walks src to its NUL, then subtracts.
    u32 luSrcLen = 0;
    {
        const char* lpcScan = lpcSrc;
        while (*lpcScan++)
            ;
        luSrcLen = static_cast<u32>(lpcScan - lpcSrc - 1);   // v7
    }

    u32 luOut = 0;   // v6: running output index

    if (luIndent != 0)
    {
        for (u32 luK = 0; luK < luIndent; ++luK)
            lpcDest[luK] = '_';
        luOut = luIndent;
    }

    const u32 luTotal = luSrcLen + luIndent;   // v10
    while (luOut < luTotal)
    {
        const char lcCh = lpcSrc[luOut - luIndent];
        lpcDest[luOut] = (lcCh == ' ') ? '_' : lcCh;
        ++luOut;
    }

    lpcDest[luOut] = '\0';
}

// ----------------------------------------------------------------------------
// TextFileWriteSerialiser::Serialise(const char* name, f32& value) @0x82208878
//
// Writes one named float field to the text file. Store-for-store from the asm:
//   if (mpFile /* *(this+4) */) {
//       FormatName(&lacBuffer, name);                 ; bl FormatName(this, buf, name)
//       fprintf(mpFile, "%s : %f\n", lacBuffer,       ; the f32 is promoted to double and
//               (double)value);                       ;   passed as the varargs %f arg (stfd/ld)
//   }
// The label buffer is the fixed KI_CHARBUFFERLENGTH (64) stack buffer the sibling
// Serialise<...> template instances also use (DWARF `char lacBuffer[64]`); the X360 sized
// its stack slot at 72 bytes with alignment padding.
//
// (this/r3 is returned unchanged when mpFile is null -- a leftover-register artifact; the
// DWARF signature returns void.)
// ----------------------------------------------------------------------------
void TextFileWriteSerialiser::Serialise(const char* lpcName, f32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[64];                 // KI_CHARBUFFERLENGTH label buffer
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %f\n", lacBuffer, lrValue);
    }
}

} // namespace Camera
} // namespace BrnDirector
