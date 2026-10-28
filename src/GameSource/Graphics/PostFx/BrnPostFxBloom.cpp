#include "GameSource/Graphics/PostFx/BrnPostFxBloom.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"                 // saDepthStencilStates[1] (Render)
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"                   // saRasterizerStates[2]   (Render)
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"                        // saBlendStates[0]        (PrepareDownSampleBuffer)
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // rw::graphics::postfx::RenderTarget
namespace
{
    // Generated X360 shader packages recovered from the ARTIST executable.
    const char gacBloomDSVertexProgram[] =
        "\x10\x2A\x11\x01\x00\x00\x00\xF0\x00\x00\x00\x6C\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x00\x00\x00\x00\xB0\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x88\x00\x00\x00\x1C\x00\x00\x00\x7B\xFF\xFE\x03\x00\x00\x00\x00\x02\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\x74\x00\x00\x00\x44\x00\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x54\x00\x00\x00\x00\x00\x00\x00\x64\x00\x02\x00\x01"
        "\x00\x01\x00\x00\x00\x00\x00\x54\x00\x00\x00\x00\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x30\x5F\x30\x31\x00\x00\x01\x00\x03"
        "\x00\x01\x00\x04\x00\x01\x00\x00\x00\x00\x00\x00\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x32\x5F\x30\x33\x00\x76\x73\x5F\x33"
        "\x5F\x30\x00\x32\x2E\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB\xAB\x00\x00\x00\x00\x00\x00\x00\x6C\x00\x11\x00\x01\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x20\x42\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x02\x00\x00\x02\x90\x00\x00\x00\x03\x00\x00\x50\x05"
        "\x00\x00\xF0\x50\x00\x01\xF1\x51\x00\x00\x10\x06\x00\x00\x10\x07\x10\x01\x10\x03\x00\x00\x12\x00\xC2\x00\x00\x00\x00\x00\x10\x04"
        "\x00\x00\x12\x00\xC4\x00\x00\x00\x10\x09\x30\x05\x00\x00\x22\x00\x00\x00\x00\x00\x05\xF8\x10\x00\x00\x00\x0A\x88\x00\x00\x00\x00"
        "\xC8\x0F\x80\x3E\x00\x00\x00\x00\xE2\x01\x01\x00\x05\xF8\x00\x00\x00\x00\x0F\xC8\x00\x00\x00\x00\xC8\x0F\x80\x00\x00\xA0\x00\x00"
        "\xA0\x00\x00\x00\xC8\x0F\x80\x01\x00\xA0\x00\x00\xA0\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

    const char gacBloomDSPixelProgram[] =
        "\x10\x2A\x11\x00\x00\x00\x01\x14\x00\x00\x00\xF0\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x00\x00\x00\x00\xEC\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\xC4\x00\x00\x00\x1C\x00\x00\x00\xB6\xFF\xFF\x03\x00\x00\x00\x00\x03\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\xAF\x00\x00\x00\x58\x00\x03\x00\x00\x00\x01\x00\x00\x00\x00\x00\x68\x00\x00\x00\x00\x00\x00\x00\x78\x00\x02\x00\x00"
        "\x00\x01\x00\x00\x00\x00\x00\x8C\x00\x00\x00\x00\x00\x00\x00\x9C\x00\x02\x00\x01\x00\x01\x00\x00\x00\x00\x00\x8C\x00\x00\x00\x00"
        "\x53\x61\x6D\x70\x6C\x65\x72\x53\x6F\x75\x72\x63\x65\x00\xAB\xAB\x00\x04\x00\x0C\x00\x01\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00"
        "\x6B\x44\x6F\x74\x57\x69\x74\x68\x57\x68\x69\x74\x65\x4C\x65\x76\x65\x6C\x00\xAB\x00\x01\x00\x03\x00\x01\x00\x03\x00\x01\x00\x00"
        "\x00\x00\x00\x00\x6B\x54\x68\x72\x65\x73\x68\x6F\x6C\x64\x41\x6E\x64\x53\x63\x61\x6C\x65\x00\x70\x73\x5F\x33\x5F\x30\x00\x32\x2E"
        "\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB\xAB\xAB\x00\x00\x00\x00\x00\x00\x00\xF0\x10\x00\x05\x00\x00\x00\x00\x04\x00\x00\x00\x00"
        "\x00\x00\x20\x42\x00\x03\x00\x03\x00\x00\x00\x01\x00\x00\xF0\x50\x00\x00\xF1\x51\x00\x00\x00\x00\x60\x02\xC4\x00\x12\x00\x02\x55"
        "\x00\x00\x60\x08\x50\x0E\x12\x00\x22\x00\x00\x00\xB8\x08\x20\x01\x1F\x1F\xFE\x88\x00\x00\x40\x00\x10\x08\x50\x21\x1F\x1F\xFE\x88"
        "\x00\x00\x40\x00\xB8\x08\x10\x21\x1F\x1F\xFE\x88\x00\x00\x40\x00\x10\x08\x40\x01\x1F\x1F\xFE\x88\x00\x00\x40\x00\xC8\x01\x00\x00"
        "\x00\xBE\xBE\x00\xB0\x04\x00\x00\xC8\x02\x00\x00\x00\xBE\xBE\x00\xB0\x01\x00\x00\xC8\x04\x00\x00\x00\xBE\xBE\x00\xB0\x05\x00\x00"
        "\xC8\x08\x00\x00\x00\xBE\xBE\x00\xB0\x02\x00\x00\xC8\x0F\x00\x03\x02\x00\x6C\x00\xA0\x00\x01\x00\xC9\x07\x00\x00\x00\xB1\xC0\x00"
        "\xE1\x03\x01\x00\xC9\x07\x00\x01\x00\xC6\xC0\x00\xE1\x03\x05\x00\xC9\x07\x00\x03\x00\x6C\xC0\x00\xE1\x03\x04\x00\xC9\x07\x00\x02"
        "\x00\x1B\xC0\x00\xE1\x03\x02\x00\xC8\x07\x00\x02\x00\xB4\xB4\x00\xE0\x03\x02\x00\xC8\x07\x00\x01\x00\xC0\xB4\x00\xE0\x02\x01\x00"
        "\xC8\x07\x00\x00\x00\xC0\xB4\x00\xE0\x01\x00\x00\xC8\x8F\xC0\x00\x00\xB4\xB1\x00\xA1\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00";

    const char gacBloomBlurVertexProgram[] =
        "\x10\x2A\x11\x01\x00\x00\x00\xF8\x00\x00\x00\x78\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x00\x00\x00\x00\xB0\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x88\x00\x00\x00\x1C\x00\x00\x00\x7B\xFF\xFE\x03\x00\x00\x00\x00\x02\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\x74\x00\x00\x00\x44\x00\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x54\x00\x00\x00\x00\x00\x00\x00\x64\x00\x02\x00\x01"
        "\x00\x01\x00\x00\x00\x00\x00\x54\x00\x00\x00\x00\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x30\x5F\x30\x31\x00\x00\x01\x00\x03"
        "\x00\x01\x00\x04\x00\x01\x00\x00\x00\x00\x00\x00\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x32\x5F\x30\x33\x00\x76\x73\x5F\x33"
        "\x5F\x30\x00\x32\x2E\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB\xAB\x00\x00\x00\x00\x00\x00\x00\x78\x00\x21\x00\x01\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x30\x63\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x02\x90\x00\x00\x00\x03\x00\x00\x50\x05"
        "\x00\x00\xF0\x50\x00\x01\xF1\x51\x00\x02\xF2\x52\x00\x00\x10\x07\x00\x00\x10\x08\x00\x00\x10\x06\x10\x01\x10\x03\x00\x00\x12\x00"
        "\xC2\x00\x00\x00\x00\x00\x10\x04\x00\x00\x12\x00\xC4\x00\x00\x00\x10\x09\x40\x05\x00\x00\x22\x00\x00\x00\x00\x00\x05\xF8\x10\x00"
        "\x00\x00\x0A\x88\x00\x00\x00\x00\xC8\x0F\x80\x3E\x00\x00\x00\x00\xE2\x01\x01\x00\x05\xF8\x00\x00\x00\x00\x0F\xC8\x00\x00\x00\x00"
        "\xC8\x0F\x80\x02\x00\xA0\xA0\x00\xE2\x00\x00\x00\xC8\x0F\x80\x00\x00\xA0\x00\x00\xA0\x00\x00\x00\xC8\x0F\x80\x01\x00\xA0\x00\x00"
        "\xA0\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

    const char gacBloomBlurPixelProgram[] =
        "\x10\x2A\x11\x00\x00\x00\x00\xE0\x00\x00\x00\xC4\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x8C\x00\x00\x00\xB4\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x64\x00\x00\x00\x1C\x00\x00\x00\x57\xFF\xFF\x03\x00\x00\x00\x00\x01\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\x50\x00\x00\x00\x30\x00\x03\x00\x00\x00\x01\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x53\x61\x6D\x70\x6C\x65\x72\x53"
        "\x6F\x75\x72\x63\x65\x00\xAB\xAB\x00\x04\x00\x0C\x00\x01\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x70\x73\x5F\x33\x5F\x30\x00\x32"
        "\x2E\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB\xAB\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14"
        "\x01\xFC\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x84\x10\x00\x03\x00"
        "\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x30\x63\x00\x07\x00\x07\x00\x00\x00\x01\x00\x00\xF0\x50\x00\x00\xF1\x51\x00\x00\xF2\x52"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x3E\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x60\x02\xC4\x00\x12\x00\x02\x55\x00\x00\x20\x08\x00\x00\x22\x00\x00\x00\x00\x00\xB8\x08\x20\x21\x1F\x1F\xFE\x88"
        "\x00\x00\x40\x00\x10\x08\x10\x21\x1F\x1F\xFE\x88\x00\x00\x40\x00\x10\x08\x30\x01\x1F\x1F\xFE\x88\x00\x00\x40\x00\xB8\x08\x00\x01"
        "\x1F\x1F\xFE\x88\x00\x00\x40\x00\xC8\x07\x00\x00\x00\xC0\xC0\x00\xE0\x03\x00\x00\xC8\x07\x00\x00\x00\xC0\xC0\x00\xE0\x00\x01\x00"
        "\xC8\x07\x00\x00\x00\xC0\xC0\x00\xE0\x00\x02\x00\xC8\x8F\xC0\x00\x00\xC0\x6C\x00\xA1\x00\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00";

    const char gacBloomBlurOldVertexProgram[] =
        "\x10\x2A\x11\x01\x00\x00\x01\x1C\x00\x00\x00\x78\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x00\x00\x00\x00\xD4\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\xAC\x00\x00\x00\x1C\x00\x00\x00\x9F\xFF\xFE\x03\x00\x00\x00\x00\x03\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\x98\x00\x00\x00\x58\x00\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00\x68\x00\x00\x00\x00\x00\x00\x00\x78\x00\x02\x00\x01"
        "\x00\x01\x00\x00\x00\x00\x00\x68\x00\x00\x00\x00\x00\x00\x00\x88\x00\x02\x00\x02\x00\x01\x00\x00\x00\x00\x00\x68\x00\x00\x00\x00"
        "\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x30\x5F\x30\x31\x00\x00\x01\x00\x03\x00\x01\x00\x04\x00\x01\x00\x00\x00\x00\x00\x00"
        "\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x32\x5F\x30\x33\x00\x6B\x55\x76\x4F\x66\x66\x73\x65\x74\x5F\x30\x34\x5F\x30\x35\x00"
        "\x76\x73\x5F\x33\x5F\x30\x00\x32\x2E\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB\xAB\x00\x00\x00\x00\x00\x00\x00\x78\x00\x21\x00\x01"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x30\x63\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x02\x90\x00\x00\x00\x03"
        "\x00\x00\x50\x05\x00\x00\xF0\x50\x00\x01\xF1\x51\x00\x02\xF2\x52\x00\x00\x10\x06\x00\x00\x10\x07\x00\x00\x10\x08\x10\x01\x10\x03"
        "\x00\x00\x12\x00\xC2\x00\x00\x00\x00\x00\x10\x04\x00\x00\x12\x00\xC4\x00\x00\x00\x10\x09\x40\x05\x00\x00\x22\x00\x00\x00\x00\x00"
        "\x05\xF8\x10\x00\x00\x00\x0A\x88\x00\x00\x00\x00\xC8\x0F\x80\x3E\x00\x00\x00\x00\xE2\x01\x01\x00\x05\xF8\x00\x00\x00\x00\x0F\xC8"
        "\x00\x00\x00\x00\xC8\x0F\x80\x00\x00\xA0\x00\x00\xA0\x00\x00\x00\xC8\x0F\x80\x01\x00\xA0\x00\x00\xA0\x00\x01\x00\xC8\x0F\x80\x02"
        "\x00\xA0\x00\x00\xA0\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

    const char gacBloomBlurOldPixelProgram[] =
        "\x10\x2A\x11\x00\x00\x00\x01\x0C\x00\x00\x00\x9C\x00\x00\x00\x00\x00\x00\x00\x24\x00\x00\x00\x00\x00\x00\x00\xE0\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\xB8\x00\x00\x00\x1C\x00\x00\x00\xAC\xFF\xFF\x03\x00\x00\x00\x00\x03\x00\x00\x00\x1C\x00\x00\x00\x08"
        "\x00\x00\x00\xA5\x00\x00\x00\x58\x00\x03\x00\x00\x00\x01\x00\x00\x00\x00\x00\x68\x00\x00\x00\x00\x00\x00\x00\x78\x00\x02\x00\x00"
        "\x00\x01\x00\x00\x00\x00\x00\x88\x00\x00\x00\x00\x00\x00\x00\x98\x00\x02\x00\x01\x00\x01\x00\x00\x00\x00\x00\x88\x00\x00\x00\x00"
        "\x53\x61\x6D\x70\x6C\x65\x72\x53\x6F\x75\x72\x63\x65\x00\xAB\xAB\x00\x04\x00\x0C\x00\x01\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00"
        "\x6B\x54\x61\x70\x57\x65\x69\x67\x68\x74\x73\x30\x5F\x33\x00\xAB\x00\x01\x00\x03\x00\x01\x00\x04\x00\x01\x00\x00\x00\x00\x00\x00"
        "\x6B\x54\x61\x70\x57\x65\x69\x67\x68\x74\x73\x34\x00\x70\x73\x5F\x33\x5F\x30\x00\x32\x2E\x30\x2E\x36\x35\x33\x34\x2E\x31\x00\xAB"
        "\x00\x00\x00\x00\x00\x00\x00\x9C\x10\x00\x05\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x30\x63\x00\x07\x00\x07\x00\x00\x00\x01"
        "\x00\x00\xF0\x50\x00\x00\xF1\x51\x00\x00\xF2\x52\x00\x00\x00\x00\x60\x02\xC4\x00\x12\x00\x09\x55\x00\x00\x40\x08\x00\x00\x22\x00"
        "\x00\x00\x00\x00\x10\x08\x30\x01\x1F\x1F\xFE\x88\x00\x00\x40\x00\xB8\x08\x40\x01\x1F\x1F\xFE\x88\x00\x00\x40\x00\x10\x08\x50\x21"
        "\x1F\x1F\xFE\x88\x00\x00\x40\x00\xB8\x08\x10\x21\x1F\x1F\xFE\x88\x00\x00\x40\x00\x10\x08\x00\x41\x1F\x1F\xFE\x88\x00\x00\x40\x00"
        "\xC8\x07\x00\x00\x00\xB4\x6C\x00\xA1\x00\x01\x00\xC8\x07\x00\x00\x00\xB4\x1B\xC0\xAB\x01\x00\x00\xC8\x07\x00\x00\x00\xC0\xC6\xB4"
        "\xAB\x05\x00\x00\xC8\x07\x00\x00\x00\xB4\xB1\xB4\xAB\x04\x00\x00\xC8\x8F\xC0\x00\x00\xC0\x6C\xB4\xAB\x03\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00";

    static_assert(sizeof(gacBloomDSVertexProgram) - 1 == 348,
                  "Bloom downsample vertex shader size mismatch");
    static_assert(sizeof(gacBloomDSPixelProgram) - 1 == 516,
                  "Bloom downsample pixel shader size mismatch");
    static_assert(sizeof(gacBloomBlurVertexProgram) - 1 == 368,
                  "Bloom blur vertex shader size mismatch");
    static_assert(sizeof(gacBloomBlurPixelProgram) - 1 == 420,
                  "Bloom blur pixel shader size mismatch");
    static_assert(sizeof(gacBloomBlurOldVertexProgram) - 1 == 404,
                  "Bloom legacy blur vertex shader size mismatch");
    static_assert(sizeof(gacBloomBlurOldPixelProgram) - 1 == 424,
                  "Bloom legacy blur pixel shader size mismatch");
}

namespace renderengine
{

    extern const u32 guPostFxBloomDSVertexProgramPCSize;
    extern const u32 guPostFxBloomDSPixelProgramPCSize;
    extern const u32 guPostFxBloomBlurVertexProgramPCSize;
    extern const u32 guPostFxBloomBlurPixelProgramPCSize;
    extern const u32 guPostFxBloomBlurOldVertexProgramPCSize;
    extern const u32 guPostFxBloomBlurOldPixelProgramPCSize;
}

// ==================================================================================================
// THE BLOOM CHAIN'S DRAW SURFACE -- what the three private passes below need and where it comes from.
//
// SOURCE OF TRUTH: the X360 ARTIST assembly. PrepareDownSampleBuffer @0x82401AE8,
// Generate1PassBlurredBloomBuffer @0x82401F50, Generate2PassBlurredBloomBuffer @0x82402308, all three
// reached only from BrnPostFxBloom::Render @0x82402B40. Every declaration shape, every parameter name
// and every local name below is the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/BrnPostFxBloom.{h,cpp}).
// ==================================================================================================

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open one 16-byte
// shader-constant row and return the write cursor. The shared decl-only surface every committed
// immediate-mode TU already uses (CgsIm2dColTex.cpp:91, CgsIm2dUntex.cpp:82, CgsIm3d.cpp:91,
// BrnIm3d.cpp:118); DEFINED in the mounted pc/gcm/renderengine/ImmediateMode.cpp:543. There is
// no header for this seam, which is why each consumer declares it.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The Xbox 360 immediate-vertex ring. Declared exactly as CgsIm2dUntex.cpp:102-104,
// BrnSkidVertex.cpp:103-104 and BrnPostFxShader.cpp:135-137 declare it; DEFINED in the mounted
// pc/gcm/renderengine/XenonD3D9Shims.cpp:2396 / :2478 (landed 7af516a6), which also owns the Xenos
// primitive-type translation this file relies on -- see KU_BLOOM_PRIMITIVE_TYPE.
struct D3DDevice;
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                         u32 luVertexCount, u32 luStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

// ==================================================================================================
// THE THREE CACHED RENDER STATES THE BLOOM CHAIN PUSHES. ALL THREE ARE IDENTIFIED; NONE IS NAMEABLE
// FROM THIS TU YET. Declared here as the externs the calls need, with NO placeholder value invented,
// because what is missing is a HOST PUBLISHER, not a value. This is the identical situation -- and
// deliberately the identical spelling -- that BrnPostFx.cpp:120-123 / :669-672 already documents for
// the first two of them.
//
//   gpPostFxDepthStencilState == X360 dword_83010910 == CgsDepthStencilStateFactory::saDepthStencilStates[1]
//   == E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF (CgsDepthStencilStateFactory.cpp:26-33).
//   BrnPostFxBloom::Render inlines the compare/apply/cache for it at 0x82402B50-0x82402BAC, against
//   the same shadow slot (dword_83010A28) and through the same applier (sub_827E8150 ==
//   Xbox2SetDepthStencilStateLowLevelShadowed) as BrnPostFx::Render @0x8240A504. Same object, same
//   setter, same host name -- NOT a second global.
//
//   gpPostFxRasterizerState == X360 dword_83010A40 == CgsRasterizerStateFactory's
//   gapRasterizerStates[2] == E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE
//   (CgsRasterizerStateFactory.cpp:14-19). Inlined at 0x82402BB0-0x82402BEC, same argument.
//
//   gpPostFxBloomBlendState == X360 dword_83010F70 == CgsBlendStateFactory::saBlendStates[0] ==
//   E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA (CgsBlendStateFactory.h:113/:150 --
//   the slot names are the FireAssert strings CgsBlendStateFactory::Construct @0x827EB2D8 embeds).
//   PrepareDownSampleBuffer inlines the blend third of the triple at 0x82401B1C-0x82401B60: the gate
//   is byte_83010907 (mbBlendStateLocked), the compare/cache slot is dword_83010964
//   (StateBlockShadow::m_pBlendState) and the applier is shadow::Device::Xbox2SetStateLowLevelShadowed
//   -- i.e. shadow::Device::SetState(const BlendMaterialState*) @0x82276A68, verbatim. It is the ONLY
//   one of the three the bloom chain pushes, and only in the down-sample pass; the two blur passes
//   inherit it.
//
// THE FIRST TWO ARE NOW NAMEABLE (gate-flip wave, 2026-08-15): the depth-stencil and rasterizer
// factories publish their tables as the DWARF's private statics behind a static GetState(slot)
// (CgsDepthStencilStateFactory.h / CgsRasterizerStateFactory.h), and BrnPostFx::Render reads the
// same two slots the same way -- so no `gpPostFx*` global exists on the host any more than it did on
// the console (there, too, these are two table slots, not globals). Render below reads the tables
// through the two enumerators the shipped assert strings pin.
//
// The third is nameable too (verify fix-forward, 2026-08-15): CgsBlendStateFactory::GetState is
// static like its two siblings, so PrepareDownSampleBuffer reads saBlendStates[0] through the
// enumerator its FireAssert string pins -- no `gpPostFxBloomBlendState` global exists on the host,
// exactly as no dword_83010F70-named global did on the console.
namespace renderengine { class DepthStencilState; class RasterizerState; }

renderengine::ProgramBufferData* BrnPostFxBloom::CreateProgram(
    const void* lpMicrocode, u32 luSize, bool lbPixelProgram)
{
    {
        // The measurement gate is OFF, so Construct never reached here at all (its own early-out
        // fires first); kept as the belt-and-braces refusal to enter the console route.
        static bool sbReported = false;
        return nullptr;
    }

    // clang-format off
    // ---- the console route (X360 CreateProgram body), unreachable on the PC backend -- see above ---
    renderengine::ProgramBufferParameters lParameters = {};
    lParameters.muFunction =
        static_cast<u32>(reinterpret_cast<usize>(lpMicrocode));
    lParameters.muShaderType = lbPixelProgram ? 1u : 0u;
    lParameters.muReserved8 = luSize;

    rw::BaseResourceDescriptors<5> lDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);
    rw::Resource lResource = mpAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
    return renderengine::ProgramBuffer::Initialize(
        reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource),
        &lParameters);
    // clang-format on
}

void BrnPostFxBloom::Construct(rw::IResourceAllocator* lpAllocator)
{
    mpAllocator = lpAllocator;
    mbUseNewBloom = false;
    mbUseHardwareGaussianInBloom = false;

    renderengine::VertexDescriptor::Parameters lVertexParameters;
    lVertexParameters.maElements[0].mu16Stream = 0;
    lVertexParameters.maElements[0].miOffset = 0x002A23B9;
    lVertexParameters.maElements[0].mu8UsageIndex = 1;
    lVertexParameters.maElements[1].mu16Stream = 0;
    lVertexParameters.maElements[1].miOffset = 0x002C23A5;
    lVertexParameters.maElements[1].mu8UsageIndex = 6;

    rw::BaseResourceDescriptors<5> lVertexDescriptor;
    renderengine::VertexDescriptor::GetResourceDescriptor(
        &lVertexDescriptor, &lVertexParameters);
    mBloomVertexDescriptorResource = mpAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lVertexDescriptor), nullptr);
    mpBloomVertexDescriptor = renderengine::VertexDescriptor::Initialize(
        &mBloomVertexDescriptorResource, &lVertexParameters);

    // ⚠ WHAT EACH CALL PASSES, AND WHY IT IS NOT THE CONSOLE'S ARGUMENT. The X360 passes the Xenos
    // microcode package and its microcode size (the `gacBloom*` array and `sizeof - 1` this used to
    // spell, carried in the trailing comment on each call). A PC build cannot bind Xenos microcode at
    // all, and ProgramBufferPC_Adopt refuses it on its first check, so each call is handed the
    // generated leaf's PC image WITH THAT IMAGE'S OWN PUBLISHED SIZE -- never a console size, which
    // the adopt would bounds-check the image against and refuse. Same convention as
    // BrnPostFxShader::Construct's slot-0 call.
                                                 // console operand: gacBloomDSVertexProgram,
                                                 // X360 0x8203E6F8, 348 B
    CGS_ASSERT(mpBloomDSVertexProgram, "NULL != mpBloomDSVertexProgram");

                                                 // console operand: gacBloomDSPixelProgram,
                                                 // X360 0x8203E858, 516 B
    CGS_ASSERT(mpBloomDSPixelProgram, "NULL != mpBloomDSPixelProgram");

    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomDSVertexProgram, reinterpret_cast<const u8*>("kUvOffset_00_01"),
        &mBloomDSVertexVariableHandleUvOffset_00_01);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomDSVertexProgram, reinterpret_cast<const u8*>("kUvOffset_02_03"),
        &mBloomDSVertexVariableHandleUvOffset_02_03);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomDSPixelProgram, reinterpret_cast<const u8*>("kDotWithWhiteLevel"),
        &mBloomDSPixelVariableHandleDot);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomDSPixelProgram, reinterpret_cast<const u8*>("kThresholdAndScale"),
        &mBloomDSPixelVariableHandleThresholdScale);

                                                 // console operand: gacBloomBlurVertexProgram,
                                                 // X360 0x8203EA60, 368 B
    CGS_ASSERT(mpBloomBlurVertexProgram, "NULL != mpBloomBlurVertexProgram");

                                                 // console operand: gacBloomBlurPixelProgram,
                                                 // X360 0x8203EBD0, 420 B
    CGS_ASSERT(mpBloomBlurPixelProgram, "NULL != mpBloomBlurPixelProgram");

    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurVertexProgram, reinterpret_cast<const u8*>("kUvOffset_00_01"),
        &mBloomBlurVertexVariableHandleUvOffset_00_01);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurVertexProgram, reinterpret_cast<const u8*>("kUvOffset_02_03"),
        &mBloomBlurVertexVariableHandleUvOffset_02_03);

                                                 // console operand: gacBloomBlurOldVertexProgram,
                                                 // X360 0x8203ED78, 404 B
    CGS_ASSERT(mpBloomBlurOldVertexProgram, "NULL != mpBloomBlurOldVertexProgram");

                                                 // console operand: gacBloomBlurOldPixelProgram,
                                                 // X360 0x8203EF10, 424 B
    CGS_ASSERT(mpBloomBlurOldPixelProgram, "NULL != mpBloomBlurOldPixelProgram");

    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurOldVertexProgram, reinterpret_cast<const u8*>("kUvOffset_00_01"),
        &mBloomBlurOldVertexVariableHandleUvOffset_00_01);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurOldVertexProgram, reinterpret_cast<const u8*>("kUvOffset_02_03"),
        &mBloomBlurOldVertexVariableHandleUvOffset_02_03);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurOldVertexProgram, reinterpret_cast<const u8*>("kUvOffset_04_05"),
        &mBloomBlurOldVertexVariableHandleUvOffset_04_05);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurOldPixelProgram, reinterpret_cast<const u8*>("kTapWeights0_3"),
        &mBloomBlurOldPixelVariableHandleTapWeights_0_to_3);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurOldPixelProgram, reinterpret_cast<const u8*>("kTapWeights4"),
        &mBloomBlurOldPixelVariableHandleTapWeights_4);
}

// X360: INLINED into BrnPostFx::Destruct @0x82408214-0x82408260 (its assert cites this file,
// line 265). Restored here as the call the original source made -- see this edit's note for the
// instruction-by-instruction map.
void BrnPostFxBloom::Destruct()
{
    CGS_ASSERT(mpAllocator != nullptr, "mpAllocator");

    renderengine::VertexDescriptor::Release(mpBloomVertexDescriptor);
    mpAllocator->DoFree(mBloomVertexDescriptorResource);
}

void BrnPostFxBloom::Render(
    rw::graphics::postfx::RenderTarget* lpBloomRenderTarget,
    rw::graphics::postfx::RenderTarget* lpIntermediateRenderTarget,
    rw::graphics::postfx::RenderTarget* lpSourceRenderTarget,
    f32 lfThreshold, f32 lfWhiteLevel)
{
    mfThreshold = lfThreshold;
    mfWhiteLevel = lfWhiteLevel;

    // asm 0x82402B50-0x82402BAC and 0x82402BB0-0x82402BEC: the depth/stencil and rasteriser thirds of
    // the render-state triple, both open-coded by the console (lock gate -> compare against the shadow
    // block's own slot -> apply through the low-level setter with lbWasUnset == (cached == 0) ->
    // cache). That sequence IS shadow::Device::SetState(const DepthStencilState*) @0x82276AD0 and
    // SetState(const RasterizerState*) @0x82276B38, and the two objects are dword_83010910 /
    // dword_83010A40 -- the SAME pair BrnPostFx::Render pushes @0x8240A504/@0x8240A514 and already
    // reaches through these two names (BrnPostFx.cpp:716-717).
    //
    // ⚠ THIS REPLACES shadow::Device::FlushDepthStencilState() / FlushRasterizerState(), WHICH WERE
    // INVENTED. Neither name exists anywhere in the X360 export set (a scan of all 30,095 exports for
    // "Flush" in a shadow::Device name returns only FlushVertexProgramState @0x827E7A10), neither
    // appears anywhere in the DecFIGS DWARF (`grep -rn "FlushDepthStencilState\|FlushRasterizerState"
    // references/DecFIGS/dwarfdump/` is empty), and this file was their only caller. They were a
    // second host spelling of a setter this tree already owns -- the split-brain the shadow block's
    // own history warns about -- so they are gone from shadowingdevice.h too (edit 03).
    // The two slots by name: saDepthStencilStates[1] / saRasterizerStates[2], the enumerators the
    // factories' shipped assert strings pin (see the header banner above).
    shadow::Device::SetState(
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(
        CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));

    if (mbUseNewBloom)
    {
        PrepareDownSampleBuffer(lpIntermediateRenderTarget, lpSourceRenderTarget);
        Generate1PassBlurredBloomBuffer(lpBloomRenderTarget,
                                        lpIntermediateRenderTarget);
    }
    else
    {
        PrepareDownSampleBuffer(lpBloomRenderTarget, lpSourceRenderTarget);
        Generate2PassBlurredBloomBuffer(lpBloomRenderTarget,
                                        lpIntermediateRenderTarget);
    }
}

// ==================================================================================================
// X360 0x82401AE8 -- BrnPostFxBloom::PrepareDownSampleBuffer(RenderTarget* lpDestRt,
//                                                            RenderTarget* lpSourceRt)
//
// DWARF: BrnPostFxBloom.h:116 (declaration), BrnPostFxBloom.cpp:285 (definition, and the parameter
// names). Only caller: BrnPostFxBloom::Render @0x82402B40, on both arms
// (`bl` @0x82402C08 with lpDestRt == the intermediate target, `bl` @0x82402C28 with lpDestRt == the
// bloom target).
//
// SIGNATURE: two pointers, no floats. Hex-Rays prints eight parameters including a `double a8`; the
// prologue is `__savegprlr_26` + `__savefpr_27` with r3/r4/r5 taken at 0x82401B04-0x82401B14 and NO
// incoming FPR read anywhere in the body -- f27..f31 are all defined before use (f31 from
// flt_82001C98 @0x82401BB4, the rest computed). The phantom parameters are the PPC ABI's skipped
// slots; DWARF gives two.
//
// WHAT IT DOES: bind the down-sample program pair, upload the four 2x2 box-tap offsets and the two
// threshold/white-level constants, bind the source target's colour TextureState at unit 0, and draw
// one full-screen quad into lpDestRt. This is the bright-pass: it down-samples the scene, subtracts
// the threshold and rescales, and the result is what the blur passes then smear.
//
// ORDER IS THE ASM'S AND IT IS LOAD-BEARING: Begin comes FIRST (before any state), the blend state is
// pushed before the programs, and the vertex-descriptor bind sits between the texture bind and the
// flush (`bl sub_8227D158` @0x82401C94, the descriptor compare/store 0x82401C98-0x82401CB0, `bl
// FlushVertexProgramState` @0x82401CB4) -- the flush is what makes the descriptor and the staged
// constants visible to the draw, so nothing may move across it.
// ==================================================================================================
void BrnPostFxBloom::PrepareDownSampleBuffer(rw::graphics::postfx::RenderTarget* lpDestRt,
                                             rw::graphics::postfx::RenderTarget* lpSourceRt)
{
    {
        static bool sbReportedNoPrograms = false;
        ReportOnce(sbReportedNoPrograms, KPC_NO_PROGRAMS_DOWNSAMPLE);
        return;
    }

    // asm 0x82401B0C-0x82401B18: Begin(lpDestRt, 0). Slice/face 0 -- the 2D path.
    lpDestRt->Begin(0);

    // asm 0x82401B1C-0x82401B60: the BLEND third of the render-state triple, open-coded (lock gate on
    // byte_83010907, compare against dword_83010964, apply through Xbox2SetStateLowLevelShadowed with
    // lbWasUnset == (cached == 0), then cache). That sequence IS shadow::Device::SetState(const
    // BlendMaterialState*) @0x82276A68, so this is inlining reversal, not a paraphrase. The two blur
    // passes do NOT push it -- they inherit whatever this pass left.
    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));

    // asm 0x82401B64-0x82401B84: the program pair, the vertex one through the same compare/apply/
    // cache shape (dword_8301095C), i.e. shadow::Device::SetVertexProgram @0x82276BA0 inlined.
    shadow::Device::SetVertexProgram(mpBloomDSVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomDSPixelProgram);

    // asm 0x82401B88-0x82401BC4: the DWARF's `lFullInvWidth` / `lFullInvHeight`
    // (BrnPostFxBloom.cpp:305/:306), taken from the SOURCE target's dimensions -- `lwz r11, 4(r29)` /
    // `lwz r10, 8(r29)` with r29 == lpSourceRt, through the console's std/lfd/fcfid/frsp
    // integer-to-float dance (which is just a cast).
    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpSourceRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpSourceRt->muHeight);

    // asm 0x82401BD0-0x82401BF4 and 0x82401BF8-0x82401C20: the four box taps, packed two per
    // constant as the DWARF's `lOffset_00_01` / `lOffset_02_03` (BrnPostFxBloom.cpp:371/:372).
    // Taps: (-du,-dv) (+du,-dv) (-du,+dv) (+du,+dv) -- a 2x2 bilinear box around the destination
    // texel. Signs read straight off the fneg pair f28 = -f30 / f27 = -f29 @0x82401BC8/0x82401BCC and
    // the four stfs per block.
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_00_01,
                       -lfFullInvWidth, -lfFullInvHeight,  lfFullInvWidth, -lfFullInvHeight);
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_02_03,
                       -lfFullInvWidth,  lfFullInvHeight,  lfFullInvWidth,  lfFullInvHeight);

    // asm 0x82401C20-0x82401C58: kDotWithWhiteLevel == (k/white, k/white, k/white, 0), k being the
    // four-tap average scale (see KF_DOWNSAMPLE_TAP_SCALE). The pixel program dots the summed taps
    // with this, so the divide by the white level and the tap average happen in one constant.
    // `lfs f13, 0x74(r30)` is mfWhiteLevel -- the member Render stored on entry, not a parameter.
    const f32 lfDotWithWhiteLevel = KF_DOWNSAMPLE_TAP_SCALE / mfWhiteLevel;
    PushShaderConstant(mBloomDSPixelVariableHandleDot,
                       lfDotWithWhiteLevel, lfDotWithWhiteLevel, lfDotWithWhiteLevel, KF_ZERO);

    // asm 0x82401C5C-0x82401C8C: kThresholdAndScale == (threshold, 0.25/(1 - threshold), 0, 0).
    // `lfs f29, 0x70(r30)` is mfThreshold; `fsubs f13, f31, f29` is 1 - threshold; `fdivs f28, f0,
    // f13` with f0 == flt_82003F40 == 0.25. The shader subtracts the threshold and rescales what is
    // left back up, so the scale is the reciprocal of the surviving range (times the quarter that
    // pairs with the four taps).
    const f32 lfThresholdScale = KF_THRESHOLD_SCALE_NUMER / (KF_ONE - mfThreshold);
    PushShaderConstant(mBloomDSPixelVariableHandleThresholdScale,
                       mfThreshold, lfThresholdScale, KF_ZERO, KF_ZERO);

    // asm 0x82401C90-0x82401C94: bind the source target's colour TextureState (texture + its own
    // sampler block) at unit 0. `lwz r3, 0x2C(r29)` is maColourTargets[0].mpTextureState on the X360
    // 4-byte-pointer image (+0x20 array base, +0x0C into the first Target); reached BY NAME here, so
    // no guest offset survives to the host.
    shadow::Device::SetState(lpSourceRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);

    // asm 0x82401C98-0x82401CB0: the vertex-descriptor bind, open-coded (compare off_83010958, set
    // the byte_83010A34 dirty flag, cache) == shadow::Device::SetVertexDescriptor.
    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);

    // asm 0x82401CB4.
    shadow::Device::FlushVertexProgramState();

    // asm 0x82401CB8-0x82401F20: the quad, half-texel-offset by the SOURCE target's dimensions.
    DrawFullScreenQuad(lpSourceRt);

    // asm 0x82401F24-0x82401F30: Resolve(lpDestRt, true, true) -- resolve both the depth/stencil and
    // the colour surface out of EDRAM so the next pass can sample the result.
    lpDestRt->Resolve(true, true);
}

// ==================================================================================================
// X360 0x82401F50 -- BrnPostFxBloom::Generate1PassBlurredBloomBuffer(RenderTarget* lpBloomRt,
//                                                                    RenderTarget* lpWorkRt)
//
// DWARF: BrnPostFxBloom.h:122 (declaration), BrnPostFxBloom.cpp:310 (definition + parameter names).
// Only caller: BrnPostFxBloom::Render @0x82402C18, on the mbUseNewBloom arm only -- and this build
// sets mbUseNewBloom to FALSE in Construct, so nothing on the shipping path reaches it. That matters
// for the finding below.
//
// Parameter roles, from the caller's registers (`mr r4, r28` == the bloom target, `mr r5, r27` == the
// intermediate/work target @0x82402C0C-0x82402C14): lpBloomRt is WRITTEN, lpWorkRt is SAMPLED. Inside
// the body r29 == lpBloomRt (Begin, Resolve) and r31 == lpWorkRt (dimensions, texture state).
// Signature is two pointers; the Hex-Rays `double a8` is a skipped PPC parameter slot, and no
// incoming FPR is read (f27..f31 are all defined before use).
//
// WHAT IT DOES: one blur pass straight from the work buffer into the bloom buffer, using the "new"
// blur program pair and a four-tap bilinear box. Structurally identical to PrepareDownSampleBuffer
// minus the blend-state push and the two pixel constants.
//
// ⚠ FINDING -- THE CONSOLE BINDS THE *DOWN-SAMPLE* PROGRAM'S CONSTANT HANDLES HERE, AND IT IS
// REPRODUCED AS WRITTEN. `addi r3, r30, 0x28` @0x82401FBC and `addi r3, r30, 0x2C` @0x82402004 are
// mBloomDSVertexVariableHandleUvOffset_00_01 / _02_03 -- the handles Construct filled from
// mpBloomDSVertexProgram (`addi r5, r31, 0x28` @0x82401808 / `addi r5, r31, 0x2C` @0x8240181C, with
// `lwz r3, 0x20(r31)` == the DS vertex program) -- while the program bound two instructions earlier is
// mpBloomBlurVertexProgram (+0x3C). The blur program's OWN handles, mBloomBlurVertexVariableHandle-
// UvOffset_00_01 / _02_03 at +0x44 / +0x48 (`addi r5, r31, 0x44` @0x82401960 / `+0x48` @0x82401970),
// are filled by Construct and then never read by anything. This is a copy-paste in the original
// source, not a decode error: the member offsets are pinned by Construct's own out-parameter
// addresses, and this pass is dead on the shipping build so it was never caught. It is harmless in
// practice only because both packages declare kUvOffset_00_01/_02_03 identically (see their CTABs in
// the arrays at the top of this file). Substituting the "right" handles here would be a silent
// behaviour change, so it is flagged and left alone.
// ==================================================================================================
void BrnPostFxBloom::Generate1PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                                     rw::graphics::postfx::RenderTarget* lpWorkRt)
{
    {
        static bool sbReportedNoPrograms = false;
        ReportOnce(sbReportedNoPrograms, KPC_NO_PROGRAMS_BLUR_1PASS);
        return;
    }

    // asm 0x82401F74-0x82401F80.
    lpBloomRt->Begin(0);

    // asm 0x82401F84-0x82401FAC. No blend push on this path -- it inherits the one
    // PrepareDownSampleBuffer left.
    shadow::Device::SetVertexProgram(mpBloomBlurVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomBlurPixelProgram);

    // asm 0x82401FB0-0x82401FEC: from the SAMPLED target's dimensions (`lwz r11, 4(r31)` /
    // `lwz r10, 8(r31)`, r31 == lpWorkRt).
    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpWorkRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpWorkRt->muHeight);

    // asm 0x82401FF8-0x8240201C and 0x82402020-0x82402038. The same 2x2 box the down-sample uses,
    // and -- see the banner -- the same two HANDLES it uses.
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_00_01,
                       -lfFullInvWidth, -lfFullInvHeight,  lfFullInvWidth, -lfFullInvHeight);
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_02_03,
                       -lfFullInvWidth,  lfFullInvHeight,  lfFullInvWidth,  lfFullInvHeight);

    // asm 0x8240203C-0x82402040 / 0x82402044-0x8240205C / 0x82402060.
    shadow::Device::SetState(lpWorkRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    // asm 0x82402064-0x824022D8: the quad, half-texel-offset by the SAMPLED target's dimensions.
    DrawFullScreenQuad(lpWorkRt);

    // asm 0x824022DC-0x824022E8.
    lpBloomRt->Resolve(true, true);
}

// ==================================================================================================
// X360 0x82402308 -- BrnPostFxBloom::Generate2PassBlurredBloomBuffer(RenderTarget* lpBloomRt,
//                                                                    RenderTarget* lpWorkRt)
//
// DWARF: BrnPostFxBloom.h:128 (declaration), BrnPostFxBloom.cpp:446 (definition + parameter names).
// Only caller: BrnPostFxBloom::Render @0x82402C38, on the !mbUseNewBloom arm -- which is the arm this
// build takes (Construct clears mbUseNewBloom), so THIS is the bloom blur the shipping game runs.
//
// Parameter roles, from the caller (`mr r4, r28` == the bloom target, `mr r5, r27` == the
// intermediate/work target @0x82402C2C-0x82402C34) and from the two assert strings the body embeds,
// which name them: r27 == lpBloomRt, r17 == lpWorkRt.
//
// WHAT IT DOES: a separable five-tap Gaussian, ping-ponged.
//   pass 1 (horizontal): sample lpBloomRt  -> draw into lpWorkRt , resolve lpWorkRt
//   pass 2 (vertical):   sample lpWorkRt   -> draw into lpBloomRt, resolve lpBloomRt
// So the bloom target is both the input and the final output, and the work target is scratch. The two
// asserts exist because a separable blur is only separable if both buffers are the same size.
//
// The PIXEL constants (the five tap weights) are uploaded ONCE, before the first Begin; only the
// VERTEX constants (the offsets) change between passes -- the console re-uploads three per pass and
// nothing else. Likewise the program pair and the vertex descriptor are bound once, at the top.
//
// SIGNATURE: two pointers. Hex-Rays prints this one with NO parameters at all (`int
// BrnPostFxBloom::Generate2PassBlurredBloomBuffer()`), which is the same PPC artefact from the other
// direction; the prologue takes r3/r4/r5 at 0x82402324-0x82402330 and DWARF gives two pointers.
// ==================================================================================================
void BrnPostFxBloom::Generate2PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                                     rw::graphics::postfx::RenderTarget* lpWorkRt)
{
    // asm 0x82402334-0x82402368 / 0x8240236C-0x8240239C. Strings and line numbers are the console's
    // own (`li r5, 0x227` == 551, `li r5, 0x228` == 552, both against
    // "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Graphics/PostFx/BrnPostFxBloom.cpp").
    // The console reaches the dimensions through GetWidth()/GetHeight() accessors the compiler
    // inlined to `lwz 4(rt)` / `lwz 8(rt)`; this tree's RenderTarget has no such accessors, so the
    // members are read by name.
    CGS_ASSERT(lpWorkRt->muWidth == lpBloomRt->muWidth,
               "lpWorkRt->GetWidth() == lpBloomRt->GetWidth()");      // BrnPostFxBloom.cpp:551
    CGS_ASSERT(lpWorkRt->muHeight == lpBloomRt->muHeight,
               "lpWorkRt->GetHeight() == lpBloomRt->GetHeight()");    // BrnPostFxBloom.cpp:552

    {
        static bool sbReportedNoPrograms = false;
        ReportOnce(sbReportedNoPrograms, KPC_NO_PROGRAMS_BLUR_2PASS);
        return;
    }

    // asm 0x824023A0-0x824023EC. DWARF locals BrnPostFxBloom.cpp:553/:554. Taken from lpWorkRt
    // (`lwz r11, 0(r28)` with r28 == lpWorkRt+4, `lwz r10, 0(r30)` with r30 == lpWorkRt+8); the two
    // asserts above are what make it irrelevant which of the pair is measured.
    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpWorkRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpWorkRt->muHeight);

    // asm 0x824023CC-0x824023FC / 0x82402400-0x82402404 / 0x82402408-0x82402420. Bound ONCE for both
    // passes. No blend push here either -- the chain inherits PrepareDownSampleBuffer's.
    shadow::Device::SetVertexProgram(mpBloomBlurOldVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomBlurOldPixelProgram);
    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);

    // asm 0x82402424-0x82402470 and 0x82402474-0x824024A4: the five tap weights, four in one constant
    // and the fifth alone. Pixel-program constants, so they are uploaded once and survive both passes.
    PushShaderConstant(mBloomBlurOldPixelVariableHandleTapWeights_0_to_3,
                       KAF_BLUR_OLD_TAP_WEIGHT[0], KAF_BLUR_OLD_TAP_WEIGHT[1],
                       KAF_BLUR_OLD_TAP_WEIGHT[2], KAF_BLUR_OLD_TAP_WEIGHT[3]);
    PushShaderConstant(mBloomBlurOldPixelVariableHandleTapWeights_4,
                       KAF_BLUR_OLD_TAP_WEIGHT[4], KF_ZERO, KF_ZERO, KF_ZERO);

    // ---- pass 1: HORIZONTAL. Sample lpBloomRt, write lpWorkRt. -----------------------------------
    // asm 0x824024A8.
    lpWorkRt->Begin(0);

    // asm 0x824024AC-0x82402524 / 0x82402528-0x82402550 / 0x82402554-0x8240256C. The DWARF's
    // `lOffset_00_01` / `lOffset_02_03` / `lOffset_04_05` (BrnPostFxBloom.cpp:618/:619/:620), each a
    // PAIR of taps packed (u0, v0, u1, v1). On the horizontal pass every v is zero and the u's carry
    // the offsets scaled into texel space by 1/width; the sixth slot is unused (five taps).
    // The five multiplies are `fmuls f25/f24/f23/f22/f21, f31, <literal>` @0x824024C4-0x824024F4 with
    // f31 == lfFullInvWidth.
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_00_01,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[0], KF_ZERO,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[1], KF_ZERO);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_02_03,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[2], KF_ZERO,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[3], KF_ZERO);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_04_05,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[4], KF_ZERO,
                       KF_ZERO, KF_ZERO);

    // asm 0x82402570-0x82402574 / 0x82402578: sample the BLOOM target.
    shadow::Device::SetState(lpBloomRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::FlushVertexProgramState();

    // asm 0x8240257C-0x82402800: the quad, half-texel-offset by the target being SAMPLED.
    DrawFullScreenQuad(lpBloomRt);

    // asm 0x82402804-0x82402810.
    lpWorkRt->Resolve(true, true);

    // ---- pass 2: VERTICAL. Sample lpWorkRt, write lpBloomRt. -------------------------------------
    // asm 0x82402814-0x8240281C.
    lpBloomRt->Begin(0);

    // asm 0x82402820-0x82402868 / 0x8240286C-0x82402890 / 0x82402894-0x824028AC. The SAME five
    // offsets, now in the v lane and scaled by 1/height (`fmuls f29/f28/f27/f26/f30, f30, <same
    // literals>` @0x82402828-0x8240283C, f30 == lfFullInvHeight). Every u is zero. This lane swap IS
    // the separability of the filter; getting it wrong smears the bloom diagonally.
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_00_01,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[0],
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[1]);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_02_03,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[2],
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[3]);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_04_05,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[4],
                       KF_ZERO, KF_ZERO);

    // asm 0x824028B0-0x824028B4 / 0x824028B8: sample the WORK target this time. The pixel constants,
    // the programs and the vertex descriptor are NOT re-bound -- the console re-binds neither.
    shadow::Device::SetState(lpWorkRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::FlushVertexProgramState();

    // asm 0x824028BC-0x82402B14: the quad, half-texel-offset by lpWorkRt (`lwz r11, var_150` /
    // `lwz r11, var_14C` @0x824028BC/0x824028E8 -- the two stack slots holding &lpWorkRt->muWidth and
    // &lpWorkRt->muHeight, saved at 0x8240233C / 0x8240237C).
    DrawFullScreenQuad(lpWorkRt);

    // asm 0x82402B18-0x82402B24.
    lpBloomRt->Resolve(true, true);
}
