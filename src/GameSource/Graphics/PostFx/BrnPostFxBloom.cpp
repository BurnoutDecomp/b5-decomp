#include "GameSource/Graphics/PostFx/BrnPostFxBloom.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <cstddef>

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

    CGS_STATIC_ASSERT((sizeof(gacBloomDSVertexProgram) - 1 == 348),
                  "Bloom downsample vertex shader size mismatch");
    CGS_STATIC_ASSERT((sizeof(gacBloomDSPixelProgram) - 1 == 516),
                  "Bloom downsample pixel shader size mismatch");
    CGS_STATIC_ASSERT((sizeof(gacBloomBlurVertexProgram) - 1 == 368),
                  "Bloom blur vertex shader size mismatch");
    CGS_STATIC_ASSERT((sizeof(gacBloomBlurPixelProgram) - 1 == 420),
                  "Bloom blur pixel shader size mismatch");
    CGS_STATIC_ASSERT((sizeof(gacBloomBlurOldVertexProgram) - 1 == 404),
                  "Bloom legacy blur vertex shader size mismatch");
    CGS_STATIC_ASSERT((sizeof(gacBloomBlurOldPixelProgram) - 1 == 424),
                  "Bloom legacy blur pixel shader size mismatch");
}

namespace
{
    // Exact ARTIST scalar constants, 820475FC and 82047600..620.
    const f32 KF_ONE = 1.0f;
    const f32 KF_ZERO = 0.0f;
    const f32 KF_DOWNSAMPLE_TAP_SCALE = 0.33333298563957214f;
    const f32 KF_THRESHOLD_SCALE_NUMER = 0.25f;
    const u32 KU_SAMPLER_SOURCE = 0;
    const f32 KAF_BLUR_OLD_TAP_WEIGHT[5] = {
        0.15075992047786713f, 0.2732667028903961f, 0.29776808619499207f,
        0.224263995885849f, 0.0539412759244442f };
    const f32 KAF_BLUR_OLD_TAP_OFFSET[5] = {
        -3.3577959537506104f, -1.4663771390914917f, 0.4897145628929138f,
        2.4317173957824707f, 4.0f };

    __forceinline void PushShaderConstant(const renderengine::ProgramVariableHandle& lrHandle,
                                         f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(lrHandle, lIterator);
        lIterator.m_dest->mV.x = lfX;
        lIterator.m_dest->mV.y = lfY;
        lIterator.m_dest->mV.z = lfZ;
        lIterator.m_dest->mV.w = lfW;
        ++lIterator.m_dest;
    }

    // ARTIST82401CB8..1F20: four FLOAT3/FLOAT2 vertices, native strip type6.
    __forceinline void DrawFullScreenQuad(const rw::graphics::postfx::RenderTarget* lpSourceRt)
    {
        const rw::math::vpu::VectorIntrinsic lInverse = {
            KF_ONE / static_cast<f32>(lpSourceRt->muWidth),
            KF_ONE / static_cast<f32>(lpSourceRt->muHeight), KF_ZERO, KF_ZERO };
        const rw::math::vpu::VectorIntrinsic lHalf = { 0.5f, 0.5f, 0.5f, 0.5f };
        const rw::math::vpu::VectorIntrinsic lOffset = __vmulfp(lInverse, lHalf);
        D3DDevice* const lpDevice = gpD3DDevice;
        typedef rw::graphics::postfx::PfxHelper::Vertex Vertex;
        CGS_STATIC_ASSERT((sizeof(Vertex) == 20), "ARTIST bloom vertex stride");
        Vertex* lpVertices = static_cast<Vertex*>(
            D3DDevice_BeginVertices(lpDevice, D3DPT_TRIANGLESTRIP, 4, sizeof(Vertex)));
        const f32 lafPosition[4][2] = { {-1.0f,-1.0f}, {1.0f,-1.0f}, {-1.0f,1.0f}, {1.0f,1.0f} };
        const rw::math::vpu::VectorIntrinsic laUv[4] = {
            {0.0f,1.0f,0.0f,0.0f}, {1.0f,1.0f,0.0f,0.0f},
            {0.0f,0.0f,0.0f,0.0f}, {1.0f,0.0f,0.0f,0.0f} };
        for (u32 luVertex = 0; luVertex < 4; ++luVertex)
        {
            const rw::math::vpu::VectorIntrinsic lUv = __vaddfp(lOffset, laUv[luVertex]);
            lpVertices[luVertex].maPosition[0] = lafPosition[luVertex][0];
            lpVertices[luVertex].maPosition[1] = lafPosition[luVertex][1];
            lpVertices[luVertex].maPosition[2] = KF_ZERO;
            lpVertices[luVertex].maUv[0] = lUv.x;
            lpVertices[luVertex].maUv[1] = lUv.y;
        }
        D3DDevice_EndVertices(lpDevice);
    }
}

void BrnPostFxBloom::Construct(rw::IResourceAllocator* lpAllocator)
{
    CGS_STATIC_ASSERT((sizeof(BrnPostFxBloom) == 0x78), "ARTIST bloom size");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mpBloomVertexDescriptor) == 0x18), "ARTIST bloom descriptor");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mbUseNewBloom) == 0x1c), "ARTIST bloom mode");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mpBloomDSVertexProgram) == 0x20), "ARTIST bloom DS programs");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mpBloomBlurVertexProgram) == 0x3c), "ARTIST bloom blur programs");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mpBloomBlurOldVertexProgram) == 0x50), "ARTIST bloom old programs");
    CGS_STATIC_ASSERT((offsetof(BrnPostFxBloom, mfThreshold) == 0x70), "ARTIST bloom threshold");
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
    rw::ResourceDescriptor lVertexDescriptor;
    renderengine::VertexDescriptor::GetResourceDescriptor(&lVertexDescriptor, &lVertexParameters);
    mBloomVertexDescriptorResource = mpAllocator->DoAllocate(lVertexDescriptor, 0);
    mpBloomVertexDescriptor = renderengine::VertexDescriptor::Initialize(
        &mBloomVertexDescriptorResource, &lVertexParameters);
    mpBloomDSVertexProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        0, gacBloomDSVertexProgram, sizeof(gacBloomDSVertexProgram) - 1, mpAllocator);
    CGS_ASSERT(mpBloomDSVertexProgram, "NULL != mpBloomDSVertexProgram");
    mpBloomDSPixelProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        1, gacBloomDSPixelProgram, sizeof(gacBloomDSPixelProgram) - 1, mpAllocator);
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
    mpBloomBlurVertexProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        0, gacBloomBlurVertexProgram, sizeof(gacBloomBlurVertexProgram) - 1, mpAllocator);
    CGS_ASSERT(mpBloomBlurVertexProgram, "NULL != mpBloomBlurVertexProgram");
    mpBloomBlurPixelProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        1, gacBloomBlurPixelProgram, sizeof(gacBloomBlurPixelProgram) - 1, mpAllocator);
    CGS_ASSERT(mpBloomBlurPixelProgram, "NULL != mpBloomBlurPixelProgram");
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurVertexProgram, reinterpret_cast<const u8*>("kUvOffset_00_01"),
        &mBloomBlurVertexVariableHandleUvOffset_00_01);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpBloomBlurVertexProgram, reinterpret_cast<const u8*>("kUvOffset_02_03"),
        &mBloomBlurVertexVariableHandleUvOffset_02_03);
    mpBloomBlurOldVertexProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        0, gacBloomBlurOldVertexProgram, sizeof(gacBloomBlurOldVertexProgram) - 1, mpAllocator);
    CGS_ASSERT(mpBloomBlurOldVertexProgram, "NULL != mpBloomBlurOldVertexProgram");
    mpBloomBlurOldPixelProgram = rw::graphics::postfx::PfxHelper::CreateProgram(
        1, gacBloomBlurOldPixelProgram, sizeof(gacBloomBlurOldPixelProgram) - 1, mpAllocator);
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

void BrnPostFxBloom::Destruct()
{
    CGS_ASSERT(mpAllocator != 0, "mpAllocator");

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

void BrnPostFxBloom::PrepareDownSampleBuffer(rw::graphics::postfx::RenderTarget* lpDestRt,
                                             rw::graphics::postfx::RenderTarget* lpSourceRt)
{

    lpDestRt->Begin(0);

    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));

    shadow::Device::SetVertexProgram(mpBloomDSVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomDSPixelProgram);

    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpSourceRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpSourceRt->muHeight);

    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_00_01,
                       -lfFullInvWidth, -lfFullInvHeight,  lfFullInvWidth, -lfFullInvHeight);
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_02_03,
                       -lfFullInvWidth,  lfFullInvHeight,  lfFullInvWidth,  lfFullInvHeight);

    const f32 lfDotWithWhiteLevel = KF_DOWNSAMPLE_TAP_SCALE / mfWhiteLevel;
    PushShaderConstant(mBloomDSPixelVariableHandleDot,
                       lfDotWithWhiteLevel, lfDotWithWhiteLevel, lfDotWithWhiteLevel, KF_ZERO);

    const f32 lfThresholdScale = KF_THRESHOLD_SCALE_NUMER / (KF_ONE - mfThreshold);
    PushShaderConstant(mBloomDSPixelVariableHandleThresholdScale,
                       mfThreshold, lfThresholdScale, KF_ZERO, KF_ZERO);

    shadow::Device::SetState(lpSourceRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);

    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);

    shadow::Device::FlushVertexProgramState();

    DrawFullScreenQuad(lpSourceRt);

    lpDestRt->Resolve(true, true);
}

void BrnPostFxBloom::Generate1PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                                     rw::graphics::postfx::RenderTarget* lpWorkRt)
{

    lpBloomRt->Begin(0);

    shadow::Device::SetVertexProgram(mpBloomBlurVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomBlurPixelProgram);

    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpWorkRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpWorkRt->muHeight);

    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_00_01,
                       -lfFullInvWidth, -lfFullInvHeight,  lfFullInvWidth, -lfFullInvHeight);
    PushShaderConstant(mBloomDSVertexVariableHandleUvOffset_02_03,
                       -lfFullInvWidth,  lfFullInvHeight,  lfFullInvWidth,  lfFullInvHeight);

    shadow::Device::SetState(lpWorkRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    DrawFullScreenQuad(lpWorkRt);

    lpBloomRt->Resolve(true, true);
}

void BrnPostFxBloom::Generate2PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                                     rw::graphics::postfx::RenderTarget* lpWorkRt)
{
    CGS_ASSERT(lpWorkRt->muWidth == lpBloomRt->muWidth,
               "lpWorkRt->GetWidth() == lpBloomRt->GetWidth()");      // BrnPostFxBloom.cpp:551
    CGS_ASSERT(lpWorkRt->muHeight == lpBloomRt->muHeight,
               "lpWorkRt->GetHeight() == lpBloomRt->GetHeight()");    // BrnPostFxBloom.cpp:552

    const f32 lfFullInvWidth  = KF_ONE / static_cast<f32>(lpWorkRt->muWidth);
    const f32 lfFullInvHeight = KF_ONE / static_cast<f32>(lpWorkRt->muHeight);

    shadow::Device::SetVertexProgram(mpBloomBlurOldVertexProgram);
    shadow::Device::SetPixelProgram(mpBloomBlurOldPixelProgram);
    shadow::Device::SetVertexDescriptor(mpBloomVertexDescriptor);

    PushShaderConstant(mBloomBlurOldPixelVariableHandleTapWeights_0_to_3,
                       KAF_BLUR_OLD_TAP_WEIGHT[0], KAF_BLUR_OLD_TAP_WEIGHT[1],
                       KAF_BLUR_OLD_TAP_WEIGHT[2], KAF_BLUR_OLD_TAP_WEIGHT[3]);
    PushShaderConstant(mBloomBlurOldPixelVariableHandleTapWeights_4,
                       KAF_BLUR_OLD_TAP_WEIGHT[4], KF_ZERO, KF_ZERO, KF_ZERO);

    lpWorkRt->Begin(0);

    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_00_01,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[0], KF_ZERO,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[1], KF_ZERO);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_02_03,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[2], KF_ZERO,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[3], KF_ZERO);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_04_05,
                       lfFullInvWidth * KAF_BLUR_OLD_TAP_OFFSET[4], KF_ZERO,
                       KF_ZERO, KF_ZERO);

    shadow::Device::SetState(lpBloomRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::FlushVertexProgramState();

    DrawFullScreenQuad(lpBloomRt);

    lpWorkRt->Resolve(true, true);

    lpBloomRt->Begin(0);

    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_00_01,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[0],
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[1]);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_02_03,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[2],
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[3]);
    PushShaderConstant(mBloomBlurOldVertexVariableHandleUvOffset_04_05,
                       KF_ZERO, lfFullInvHeight * KAF_BLUR_OLD_TAP_OFFSET[4],
                       KF_ZERO, KF_ZERO);

    shadow::Device::SetState(lpWorkRt->maColourTargets[0].mpTextureState, KU_SAMPLER_SOURCE);
    shadow::Device::FlushVertexProgramState();

    DrawFullScreenQuad(lpWorkRt);

    lpBloomRt->Resolve(true, true);
}
