import hashlib, json, os, pathlib, subprocess, sys

BASE = pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts')
OUT = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PREFIX = 'src/SDKs/Xam/'
PARENT = '015c30af9d873e85481dba4b2e6176d3b317e73c'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=WORK).decode().strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def edit(name, changes):
    path = WORK / (PREFIX+name)
    contents = path.read_bytes()
    eol = b'\r\n' if b'\r\n' in contents else b'\n'
    for before,after in changes:
        a,b = [x.encode().replace(b'\n',eol) for x in (before,after)]
        assert contents.count(a) == 1, (name,before)
        contents = contents.replace(a,b)
    path.write_bytes(contents)

assert git('rev-parse','HEAD') == PARENT and not git('status','--porcelain=v1')
assert git('branch','--show-current') == 'codex/experimental-x360-r23-thread-layout'
edit('CConformanceList.cpp', [(
"""// CConformanceList::GetConformingScopeAndValue @ 0x82980400""",
"""// CConformanceList::ReleaseConformanceInfo @ 0x829803A0
s32 CConformanceList::ReleaseConformanceInfo(u32 luIndex)
{
    if (luIndex >= KU_MAX_ENTRIES)
    {
        return KI_E_INVALIDARG;
    }

    if (luIndex == KU_MAX_ENTRIES - 1)
    {
        return KI_S_OK;
    }

    Entry& lEntry = maEntries[luIndex];
    const u32 luRequired = KU_FLAG_ESTABLISHED | KU_FLAG_ACQUIRED;
    if ((lEntry.muFlags & luRequired) != luRequired)
    {
        return KI_E_FAIL;
    }

    lEntry.muFlags &= ~KU_FLAG_ACQUIRED;
    return KI_S_OK;
}

// ---------------------------------------------------------------------------
// CConformanceList::GetConformingScopeAndValue @ 0x82980400""")])
edit('CSchemaData.h', [(
"""// LAYOUT (from LookupConstantFromTable's asm -- only the two fields it reads are
// pinned; the rest of the object is opaque padding sized to reach them):
//     +0x18  muConstantCount  -- u16: highest valid constant index, inclusive
//                                (lhz r10,0x18(r3); the guard rejects index>count)
//     +0x40  mpConstantTable  -- const u32*: base of the constant value table
//                                (lwz r10,0x40(r3); indexed by `4 * index`)""",
"""// Layout from the native constant lookup, entry lookup and payload binding:
//     +0x18  muConstantCount  -- highest valid constant index, inclusive
//     +0x28  muEntryCount     -- number of schema entries
//     +0x2A  muEntryStride    -- source table stride in bytes
//     +0x30  mpEntryTable     -- schema entry table
//     +0x34  mpPayloadBase    -- payload span
//     +0x40  mpConstantTable  -- constant value table"""),(
"""    // Look a schema field up by id and fill `lpEntry` (a CSchemaAccess::SchemaEntry:
    // two u16 sizes + two u32 offsets). Returns a negative status when the id is not
    // found. Body lives in the CSchemaData/CSchemaAccess marshalling TU; declared
    // here so CSchemaAccess::BindToSchema @ 0x8297F380 links against the real member.
    s32 GetSchemaEntry(u32 luSchemaId, void* lpEntry);""",
"""    enum { KU_SCHEMA_ENTRY_SIZE = 0x18 };

    // @ 0x8297F228 -- copy the complete native entry, or return E_FAIL.
    s32 GetSchemaEntry(u32 luSchemaId, void* lpEntry);"""),(
"""    u8        mPad1[0x34 - 0x1A];   // +0x1A .. +0x33 (opaque)
    u8*       mpPayloadBase;""",
"""    u8        mPad1[0x28 - 0x1A];   // +0x1A .. +0x27 (opaque)
    u16       muEntryCount;         // +0x28
    u16       muEntryStride;        // +0x2A
    u8        mPadEntry[0x30 - 0x2C];
    const u8* mpEntryTable;         // +0x30
    u8*       mpPayloadBase;""")])
edit('CSchemaData.cpp', [(
"""#include "SDKs/Xam/CSchemaData.h"
""",
"""#include "SDKs/Xam/CSchemaData.h"

#include <cstring>
"""),(
"""// CSchemaData::LookupConstantFromTable @ 0x8297F280""",
"""// CSchemaData::GetSchemaEntry @ 0x8297F228
s32 CSchemaData::GetSchemaEntry(u32 luSchemaId, void* lpEntry)
{
    if (luSchemaId >= muEntryCount)
    {
        return KI_E_FAIL;
    }

    memcpy(lpEntry, mpEntryTable + luSchemaId * muEntryStride, KU_SCHEMA_ENTRY_SIZE);
    return 0;
}

// ---------------------------------------------------------------------------
// CSchemaData::LookupConstantFromTable @ 0x8297F280""")])
edit('CSchemaAccess.h', [(
"""    // The 0x0C-byte record CSchemaData::GetSchemaEntry fills for a schema id.""",
"""    // The 0x18-byte record copied by CSchemaData::GetSchemaEntry."""),(
"""        u32 muOffsetSecond; // +0x08  (lwz var_38)  offset for the second path
    };
""",
"""        u32 muOffsetSecond; // +0x08  (lwz var_38)  offset for the second path
        u8 maOtherFields[CSchemaData::KU_SCHEMA_ENTRY_SIZE - 0x0C];
    };

    typedef char SchemaEntrySizeCheck[sizeof(SchemaEntry) == CSchemaData::KU_SCHEMA_ENTRY_SIZE ? 1 : -1];
""")])
changed = [PREFIX+x for x in ('CConformanceList.cpp','CSchemaData.h','CSchemaData.cpp','CSchemaAccess.h')]
assert set(git('diff','--name-only').splitlines())==set(changed)
git('diff','--check','--',*changed)
sources = [PREFIX+x for x in ('CConformanceList.cpp','CSchemaData.cpp','CSchemaAccess.cpp','CMarshaller.cpp')]
reference = json.loads((BASE/'wave17/network-platform-terra/connapi-control-receipt.json').read_text())
args = [sys.executable,'-X','utf8','-B','tools/build_x360.py','--sdk',reference['sdk'],
        '--out',str(OUT/'native1'),'--jobs','1']
args += [x for x in reference['invocation'] if x.startswith('--compiler-arg=')]
assert len([x for x in args if x.startswith('--compiler-arg=/I')])==9
for src in sources: args += ['--source',src]
temp = OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path = OUT/'native1/compile-results.json'
report = json.loads(report_path.read_text())
assert report['requested'] == report['passed'] == 4 and not report['failed']
rows = {x['source']:x for x in report['results']}
assert set(rows)==set(sources) and all(r['machine']==498 for r in rows.values())
for file,symbol in [('CSchemaData.cpp','?GetSchemaEntry@CSchemaData@@QAAHIPAX@Z'),
                    ('CConformanceList.cpp','?ReleaseConformanceInfo@CConformanceList@@QAAHI@Z')]:
    asm = pathlib.Path(rows[PREFIX+file]['assembly']).read_text(errors='replace')
    assert symbol in asm
    if file=='CSchemaData.cpp':
        body = asm[asm.index('Begin code for function: '+symbol):asm.index('End code for function: '+symbol)]
        for opcode, immediate in [('lhz','40('),('lhz','42('),('lwz','48('),('li','24')]:
            assert any(opcode in x and immediate in x for x in body.splitlines()), (opcode,immediate,body)
message = OUT/'COMMIT_MESSAGE.txt'
message.write_text('Recover native Xam schema entries and slot release\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*changed)
assert set(git('diff','--cached','--name-only').splitlines()) == set(changed)
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt = {'parent':PARENT,'commit':git('rev-parse','HEAD'),
           'paths':{p:sha(WORK/p) for p in changed},'report':str(report_path),
           'report_sha256':sha(report_path),
           'artist':{'GetSchemaEntry':'0x8297F228..0x8297F27F',
                     'BindToSchema':'0x8297F380..0x8297F3F7',
                     'ReleaseConformanceInfo':'0x829803A0..0x829803FB'},
           'facts':['Native BindToSchema passes a 24-byte stack record, with its first 12 bytes used for the two sizes and offsets.',
                    'Schema lookup reads count at +0x28, stride at +0x2A, and table at +0x30. Copies 24 bytes.',
                    'Release rejects index >= 64, treats slot 63 as success without mutation, requires both flags for ordinary slots, and clears only acquired.'],
           'outputs':{p:{kind:sha(pathlib.Path(r[kind])) for kind in ('assembly','object','log')} for p,r in rows.items()}}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
