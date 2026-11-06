from pathlib import Path
import sys


here = Path(__file__).resolve().parent
root = Path(sys.argv[1]) / "src"
prefix = ('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
          'extern bool gbTraceAptRuntime;\n')


def write(relative, old, new):
    path = root / relative
    body = path.read_text(encoding="utf-8")
    assert body.count(old) == 1, old
    with (here / path.name).open("x", encoding="utf-8") as output:
        output.write(prefix + body.replace(old, new))


anchor = "        AptAssetString hHandle = gAptFuncs.pfnAllocateString(&params);"
write(
    "SDKs/EATech/include/Apt/AptCIHText.cpp", anchor,
    """        static unsigned int suTitleFonts = 0;
        const bool lbProbeFont = gbTraceAptRuntime && suTitleFonts++ < 24;
        if (lbProbeFont)
        {
            char lacProbe[190];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R106_APT_FONT font=%08X format=%08X itemFont=%d authored=%d parent=%08X\\n",
                reinterpret_cast<u32>(params.szFontName), reinterpret_cast<u32>(pFmt),
                pItem->mFontID, pCharacter ? pCharacter->mnDefaultGlyphIndex : -1,
                reinterpret_cast<u32>(pParent));
            OutputDebugStringA(lacProbe);
        }
""" + anchor + """
        if (lbProbeFont) OutputDebugStringA("R106_APT_FONT_LAYOUT_END\\n");""",
)

anchor = """        const CgsResource::SafeResourceHandle<CgsResource::Font>& lFontHandle =
            lpFonts->FindFont(lpParams->szFontName);"""
write(
    "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.cpp", anchor,
    """        static unsigned int suTitleFinds = 0;
        const bool lbProbeFont = gbTraceAptRuntime && suTitleFinds++ < 24;
        if (lbProbeFont)
        {
            char lacProbe[152];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R106_FONT_FIND name=%.72s\\n",
                lpParams->szFontName ? lpParams->szFontName : "<NULL>");
            OutputDebugStringA(lacProbe);
        }
""" + anchor + """
        if (lbProbeFont)
            OutputDebugStringA(lFontHandle.IsNull() ? "R106_FONT_FIND_EMPTY\\n" : "R106_FONT_FIND_OK\\n");""",
)
