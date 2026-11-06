"""Prepare an ignored native-record candidate; apply only after the frozen sweep ends."""
import argparse
import hashlib
import json
from pathlib import Path

BASE = Path(__file__).resolve().parents[2]
ROOT = BASE / 'b5-decomp'
OVERLAY = ROOT / 'build/x360/native-apt-record-candidate'
APT = 'src/SDKs/EATech/include/Apt/'
changes = {}


def load(name):
    return (ROOT / name).read_text(encoding='utf-8')


def change(name, body):
    old = (ROOT / name).read_bytes()
    changes[name] = {'old_sha256': hashlib.sha256(old).hexdigest(),
                     'body': body.replace('\r\n', '\n').replace('\n', '\r\n')}


def replace(body, old, new, count=1):
    assert body.count(old) == count, (old[:100], body.count(old), count)
    return body.replace(old, new)


name = APT + 'AptPseudoData.h'
change(name, '''#pragma once

// ARTIST AptPseudoData_t constructor @0x82AD9910. The source is an in-place
// 32-bit place command; the snapshot borrows its matrix, colour and handler data.
#include <cstddef>
#include "types.hpp"

struct AptPlaceObjectBody_t
{
    u32 muxFlags;             // +0x00
    s32 mi32Depth;            // +0x04 display depth
    s32 mi32CharacterId;      // +0x08
    u8 maMatrix[24];          // +0x0C six floats
    u8 maColorTransform[8];   // +0x24
    f32 mfRatio;              // +0x2C
    const char* mpName;       // +0x30
    s32 miClipDepth;          // +0x34: LWZ, then truncated to the snapshot halfword
    void* mpClipActions;      // +0x38
};
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, muxFlags) == 0x00), "ARTIST place flags");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, maMatrix) == 0x0C), "ARTIST place matrix");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, maColorTransform) == 0x24), "ARTIST place colour");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mfRatio) == 0x2C), "ARTIST place ratio");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mpName) == 0x30), "ARTIST place name");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, miClipDepth) == 0x34), "ARTIST place clip depth");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mpClipActions) == 0x38), "ARTIST place handlers");
CGS_STATIC_ASSERT((sizeof(AptPlaceObjectBody_t) == 0x3C), "ARTIST place body");

struct AptPlaceObjectInfo_t
{
    u32 muTag;                // tag 3; body follows at command+4

    const AptPlaceObjectBody_t* GetBody() const
    {
        return reinterpret_cast<const AptPlaceObjectBody_t*>(this + 1);
    }
    AptPlaceObjectBody_t* GetBody()
    {
        return const_cast<AptPlaceObjectBody_t*>(
            static_cast<const AptPlaceObjectInfo_t*>(this)->GetBody());
    }
};
CGS_STATIC_ASSERT((sizeof(AptPlaceObjectInfo_t) == 4), "ARTIST command tag");

struct AptPseudoData_t
{
    void* mpData;             // +0x00
    void* mpMatrix;           // +0x04 borrowed under flag 4
    void* mpColorTransform;   // +0x08 borrowed under flag 8
    void* mpClipActions;      // +0x0C borrowed under flag 0x80
    f32 mfRatio;              // +0x10 captured under flag 0x10
    u32 muxFlags;             // +0x14
    s16 mi16CharacterId;      // +0x18
    s16 mi16Depth;            // +0x1A captured clip depth

    AptPseudoData_t(const AptPlaceObjectInfo_t* lpSource,
                    s16 li16CharacterId, void* lpData);
};
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpData) == 0x00), "ARTIST snapshot data");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpMatrix) == 0x04), "ARTIST snapshot matrix");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpColorTransform) == 0x08), "ARTIST snapshot colour");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpClipActions) == 0x0C), "ARTIST snapshot handlers");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mfRatio) == 0x10), "ARTIST snapshot ratio");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, muxFlags) == 0x14), "ARTIST snapshot flags");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mi16CharacterId) == 0x18), "ARTIST snapshot character");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mi16Depth) == 0x1A), "ARTIST snapshot clip depth");
CGS_STATIC_ASSERT((sizeof(AptPseudoData_t) == 0x1C), "ARTIST snapshot size");
''')

name = APT + 'AptPseudoData.cpp'
body = load(name)
body = body[body.index('\nAptPseudoData_t::AptPseudoData_t') + 1:]
start = body.index('    // The body is align8(')
end = body.index('    const AptPlaceObjectBody_t*', start)
body = body[:start] + '    // ARTIST addi r11,r4,4: the body immediately follows the tag.\n' + body[end:]
start = body.index('    // X360 ctor @')
end = body.index('    mi16Depth', start)
body = body[:start] + '    // ARTIST LWZ command+0x38, then STH snapshot+0x1A.\n' + body[end:]
body = body.replace('   // native-8 clip depth', '')
body = body.replace('    // XB1 14082667E..688: mov r8,[rdx+48h]; mov[rcx+18h],r8.\n', '')
change(name, '#include "SDKs/EATech/include/Apt/AptPseudoData.h"\n\n'
             '// ARTIST @0x82AD9910: conditional captures from the resident place command.\n' + body)

name = APT + 'AptCharacter.h'
body = load(name)
body = body.replace('// BASE LAYOUT (console offsets; reconstructed with NAMED members so x64 widths\n// are correct -- the engine is 32-bit-pointer + hard-coded-offset code):',
                    '// ARTIST base layout: four 32-bit fields, no virtual table.')
body = body.replace('//                       back-link); pointer-typed so the x64 width is correct.',
                    '//                       back-link).')
body = body.replace('//                       operate on the whole word. (Console lwarx/stwcx. atomic ->\n//                       host _InterlockedExchangeAdd on the whole word.)',
                    '//                       operate on the whole word.')
start = body.index('// The x64 natural alignment')
body = body[:start] + '''// ARTIST Fixup/SetupCharacter access these slots in the resident character record.
CGS_STATIC_ASSERT((offsetof(AptCharacter, mnType) == 0x00), "ARTIST character type");
CGS_STATIC_ASSERT((offsetof(AptCharacter, mpFixupLink) == 0x04), "ARTIST character backlink");
CGS_STATIC_ASSERT((offsetof(AptCharacter, mnRefAndFlags) == 0x08), "ARTIST character references");
CGS_STATIC_ASSERT((offsetof(AptCharacter, mpAnimationFile) == 0x0C), "ARTIST character file");
CGS_STATIC_ASSERT((sizeof(AptCharacter) == 0x10), "ARTIST character header");
'''
change(name, body)

name = APT + 'AptCharacterAnimation.h'
body = load(name)
start = body.index('    // @0x7E3738 -- index')
methods = body[start:body.index('// Lock the serialized 64-bit def-base offsets')]
start = methods.index('    // \u26a0\ufe0f nBase is the LOAD BASE')
end = methods.index('    void* Unresolve', start)
methods = methods[:start] + '    // nBase is the 32-bit load address.\n' + methods[end:]
methods = methods.replace('    // the load base (the aptDataOffset memory address). SINGLE native-64-bit path (GUIAPT64\n    // "1:7:8" only); the console dual-path/transcode is gone. Faithful body in the .cpp.',
                          '    // the load base (the aptDataOffset memory address). The remaining PC\n    // record accesses in this body still require native recovery.')
prefix = '''#pragma once

// ARTIST's in-place 32-bit movie definition follows the 16-byte AptCharacter.
// The final name/id list serves both library exports and class-init indicators.
#include <cstddef>
#include "types.hpp"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"

struct AptFile;

struct AptImportEntry
{
    const char* mpImportFileName;   // +0x00
    const char* mpClassName;        // +0x04
    int32_t mnId;                  // +0x08
    AptFile* mpFile;               // +0x0C
};

struct AptInitEntry
{
    void* mpInitObject;            // +0x00 name
    int32_t mnIndicator;           // +0x04 character id, negated when consumed
};

struct AptCharacterAnimation
{
    int32_t mnFrameCount;          // +0x00
    void* mpFrames;                // +0x04
    void* mpUnk10;                 // +0x08 label-hash slot
    int32_t mnCharacterCount;      // +0x0C
    AptCharacter** mpCharacterTable; // +0x10
    int32_t mnScreenW;             // +0x14
    int32_t mnScreenH;             // +0x18
    int32_t mnMillisecondsPerFrame; // +0x1C
    int32_t mnImportCount;         // +0x20
    AptImportEntry* mpImportTable; // +0x24
    int32_t mnInitListCount;       // +0x28
    AptInitEntry* mpInitList;      // +0x2C
    int32_t mnParsedValueCount;    // +0x30

'''
gates = '''// Native offsets used by FindExport, GetIDFromImportFile, Resolve and Fixup.
CGS_STATIC_ASSERT((sizeof(AptImportEntry) == 0x10), "ARTIST import stride");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mpClassName) == 0x04), "ARTIST import class");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mnId) == 0x08), "ARTIST import id");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mpFile) == 0x0C), "ARTIST import file");
CGS_STATIC_ASSERT((sizeof(AptInitEntry) == 0x08), "ARTIST export/init stride");
CGS_STATIC_ASSERT((offsetof(AptInitEntry, mnIndicator) == 0x04), "ARTIST export/init id");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpFrames) == 0x04), "ARTIST movie frames");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnCharacterCount) == 0x0C), "ARTIST movie characters");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpCharacterTable) == 0x10), "ARTIST movie character table");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnMillisecondsPerFrame) == 0x1C), "ARTIST movie frame period");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnImportCount) == 0x20), "ARTIST movie imports");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpImportTable) == 0x24), "ARTIST movie import table");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnInitListCount) == 0x28), "ARTIST movie exports/init count");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpInitList) == 0x2C), "ARTIST movie exports/init list");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnParsedValueCount) == 0x30), "ARTIST movie resolve counter");
CGS_STATIC_ASSERT((sizeof(AptCharacterAnimation) == 0x34), "ARTIST movie definition");
'''
change(name, prefix + methods + gates)

name = APT + 'AptFile.h'
body = load(name)
body = body[:body.index('// ---------------------------------------------------------------------------\n// The loaded .apt movie root view')]
body = body.replace('#include <cstddef>   // offsetof (AptMovieData native-8 offset static_asserts)', '#include <cstddef>')
body = body.replace('struct AptImportEntry; // AptMovieData::mpImportTable element (defined in AptCharacterAnimation.h)\n', '')
start = body.index('// Console layout is 28 bytes')
end = body.index('//\n// EA SDK identifiers', start)
body = body[:start] + '// ARTIST layout is 28 bytes: seven 32-bit fields.\n' + body[end:]
body += '''// Loaded movie fields use the canonical AptCharacterAnimation immediately after
// the AptCharacter header. AptGetMovieCharacterAnimation reaches that definition;
// no separate widened movie/export view is required.
CGS_STATIC_ASSERT((offsetof(AptFile, mpData) == 0x14), "ARTIST file movie root");
CGS_STATIC_ASSERT((sizeof(AptFile) == 0x1C), "ARTIST file size");
'''
change(name, body)

for leaf in ['AptFile.cpp', 'AptCharacterAnimation.cpp', 'AptDisplayList.cpp', 'AptLoader.cpp']:
    name = APT + leaf
    body = load(name)
    if 'AptCharacterAnimationInst.h"' not in body:
        body = '#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h" // native embedded movie accessor\n' + body
    body = body.replace('static_cast<const AptMovieData*>(mpData)',
                        'AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(mpData))')
    body = body.replace('static_cast<const AptMovieData*>(import.mpFile->mpData)',
                        'AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(import.mpFile->mpData))')
    body = body.replace('static_cast<const AptMovieData*>(file.pData->mpData)',
                        'AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(file.pData->mpData))')
    body = body.replace('static_cast<const AptMovieData*>(pFile->mpData)',
                        'AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(pFile->mpData))')
    body = body.replace('reinterpret_cast<const AptMovieData*>(pChar->mpFixupLink)',
                        'AptGetMovieCharacterAnimation(pChar->mpFixupLink)')
    body = body.replace('const AptMovieData*', 'const AptCharacterAnimation*')
    body = body.replace('mnExportCount', 'mnInitListCount').replace('mpExportTable', 'mpInitList')
    body = body.replace('pMovie->mpInitList[iExport].mpName',
                        'static_cast<const char*>(pMovie->mpInitList[iExport].mpInitObject)')
    body = body.replace('pMovie->mpInitList[iExport].mnCharacterId', 'pMovie->mpInitList[iExport].mnIndicator')
    body = body.replace('const AptExportEntry& lrEntry', 'const AptInitEntry& lrEntry')
    body = body.replace('lrEntry.mnCharacterId', 'lrEntry.mnIndicator')
    body = body.replace('lrEntry.mpName', 'static_cast<const char*>(lrEntry.mpInitObject)')
    # Remove stale comments describing the now-deleted parallel view.
    if leaf == 'AptFile.cpp':
        body = '#include "SDKs/EATech/include/Apt/Apt.h" // native user-function table\n' + body
        start = body.index('// The loaded-block free hook the Apt runtime installs')
        end = body.index('AptFile::~AptFile()', start)
        body = body[:start] + body[end:]
        body = replace(body, 'AptFile_FreeLoadedBlock(mpDataBlock, this);',
                             'gAptFuncs.pfnFreeAnimation(mpDataBlock);')
        start = body.index('// ---------------------------------------------------------------------------\n// The loaded-data teardown')
        end = body.index('AptFile::~AptFile()', start)
        body = body[:start] + '// ARTIST @0x82AF7FE0: unregister, unresolve, free, then release the name.\n' + body[end:]
        body = body.replace('   // [aptlife] opt-in witness\n\n', '')
        start = body.index('        // Inverse of the load-time Fixup')
        end = body.index('        AptCharacterAnimation* pAnimation', start)
        body = body[:start] + '        // The native embedded definition follows the 16-byte character header.\n' + body[end:]
        body = body.replace(' // KU_AptEmbeddedMovieOff (native-8 character-header size)', ' // native embedded definition offset')
        start = body.index('// AptFile::mpData points at the loaded + resolved')
        end = body.index('// ---------------------------------------------------------------------------\n// FindExport', start)
        body = body[:start] + body[end:]
    if leaf == 'AptLoader.cpp':
        start = body.index('\n//\n// The key is AptFile::mpDataBlock')
        end = body.index('// ---------------------------------------------------------------------------\n// ~AptLoader', start)
        body = body[:start] + '\n' + body[end:]
        body = ''.join(line for line in body.splitlines(keepends=True)
                       if not ('#include' in line and any(key in line for key in
                               ['<cstdio>', '<cstdlib>', 'Development/Log/CgsLog.h'])))
    body = body.replace(' // AptMovieData (the .apt root + import table)', ' // loaded movie records')
    body = body.replace(' // AptMovieData (AllImportsAvailable)', ' // loaded AptFile')
    body = body.replace('// handle (the by-value AptFilePtr argument\'s teardown). AptMovieData is the homed\n// native-8 view of the loaded .apt movie root (AptFile.h).',
                        '// handle (the by-value AptFilePtr argument\'s teardown).')
    assert 'AptMovieData' not in body, leaf
    change(name, body)

name = 'src/SDKs/EATech/Apt/AptUpdate.cpp'
body = load(name)
body = body.replace('#include "SDKs/EATech/include/Apt/AptFile.h"                      // AptMovieData (mnMillisecondsPerFrame view)',
                    '#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"        // native authored frame period')
body = replace(body, 'reinterpret_cast<const AptMovieData*>(\n        pRootInst->mpRenderItem->mpCharacter)->mnMillisecondsPerFrame',
                     'AptGetMovieCharacterAnimation(\n        pRootInst->mpRenderItem->mpCharacter)->mnMillisecondsPerFrame')
body = replace(body, 'std::snprintf(lacTag, sizeof(lacTag), "%06d", gAptInputRecorderTag);',
                     'std::sprintf(lacTag, "%06d", gAptInputRecorderTag);')
body = body.replace('        if (gAptFuncs.pfnDebugSetScreenGrabPending)   // dword_8324E834 == gAptFuncs+0x1C\n            gAptFuncs.pfnDebugSetScreenGrabPending(lacTag);',
                    '        gAptFuncs.pfnDebugSetScreenGrabPending(lacTag);')
body = body.replace('        if (gAptFuncs.pfnDebugAddSavedInput)          // dword_8324E830 == gAptFuncs+0x18\n            gAptFuncs.pfnDebugAddSavedInput(reinterpret_cast<AptSavedInputRecord*>(laRecord), 8);',
                    '        gAptFuncs.pfnDebugAddSavedInput(reinterpret_cast<AptSavedInputRecord*>(laRecord), 8);')
body = body.replace('#include <cstdio>   // snprintf', '#include <cstdio>   // sprintf')
change(name, body)

name = 'src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.cpp'
body = load(name)
body = replace(body, 'extern void (*gpAptFreeAnimationHook)(void* pDataBlock);                                        // off_1059C66C\n', '')
body = replace(body, '        gpAptFreeAnimationHook              = &AptCallbackFile::FreeAnimation;\n', '')
change(name, body)

parser = argparse.ArgumentParser()
parser.add_argument('--apply', action='store_true')
args = parser.parse_args()
if args.apply:
    current = json.loads((ROOT / 'build/x360/full-native-network-apt/compile-results.json').read_text())
    assert current['state'] == 'complete', 'Do not mutate frozen production sources during the full sweep'

manifest = []
for name, row in changes.items():
    raw = row['body'].encode('utf-8')
    destination = (ROOT if args.apply else OVERLAY) / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(raw)
    manifest.append({'source': name, 'old_sha256': row['old_sha256'], 'new_sha256': hashlib.sha256(raw).hexdigest()})
(BASE / 'scratch/x360_sdk/native-apt-record-candidate.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(('Applied' if args.apply else 'Prepared ignored candidate'), len(manifest), 'paths')
