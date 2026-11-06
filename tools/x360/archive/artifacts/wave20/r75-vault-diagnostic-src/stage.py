from pathlib import Path


source = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.cpp")
body = source.read_text(encoding="utf-8")
anchor = '#include "GameShared/GameClasses/Core/CgsAssert.h"'
assert body.count(anchor) == 1
body = body.replace(anchor, '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n' + anchor)
anchor = '    const AssetID* lpSerialisedIds = mDependencies->GetIds();\n'
assert body.count(anchor) == 1
body = body.replace(anchor, anchor + '''    char lacProbe[220];
    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R75_VAULT_PRE id=%08X%08X image=%08X bytes=%u dep=%u source=%08X dest=%08X\\n",
        static_cast<u32>(lAssetId >> 32), static_cast<u32>(lAssetId),
        reinterpret_cast<u32>(lpData), luSize, mNumDependencies,
        reinterpret_cast<u32>(lpSerialisedIds), reinterpret_cast<u32>(mDepIDs));
    OutputDebugStringA(lacProbe);
''')
anchor = '    // Bind the vault\'s own serialised image as dependency block [0].'
assert body.count(anchor) == 1
body = body.replace(anchor, '''    CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
        "R75_VAULT_POST id=%08X%08X dep=%u\\n",
        static_cast<u32>(lAssetId >> 32), static_cast<u32>(lAssetId), mNumDependencies);
    OutputDebugStringA(lacProbe);

''' + anchor)
with Path(__file__).with_name(source.name).open("x", encoding="utf-8") as output:
    output.write(body)
