"""One-time restoration of the ARTIST EAString pool/refcount path."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]/'b5-decomp'


def save(path, body):
    path.write_bytes(body.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))


p = ROOT/'src/SDKs/EATech/include/Apt/AptString/EAString.cpp'
s = p.read_text(encoding='utf-8')
a = s.index('// TWO PC-PORT NOTES')
b = s.index('// EA SDK identifiers',a)
s = s[:a] + '// ARTIST uses the first 32-bit header word for interrupt-masked refcount\n// updates and the native DOGMA pool for every allocated string buffer.\n//\n' + s[b:]
s = s.replace('#include <cstdlib>   // malloc/free (the pre-AptInit static-init fallback)\n','')
s = s.replace('#include <xtl.h>  // _InterlockedIncrement16/_InterlockedDecrement16 (the refcount atomic)',
              '#include <xtl.h>   // native interrupt-masked 32-bit InterlockedExchangeAdd')
a = s.index('void EAStringC::IncreaseInternalRefCount()')
b = s.index('// ---------------------------------------------------------------------------\n// Buffer allocate',a)
s = s[:a] + '''void EAStringC::IncreaseInternalRefCount()
{
    if (m_pData != reinterpret_cast<DebugDataC*>(&s_EmptyInternalData))
        _InterlockedExchangeAdd(
            reinterpret_cast<volatile long*>(&m_pData->m_uRefCount), 0x10000);
}

void EAStringC::DecreaseInternalRefCount(StringDataC* const pData)
{
    if (pData == reinterpret_cast<StringDataC*>(&s_EmptyInternalData))
        return;

    const uint32_t uPrevious = static_cast<uint32_t>(_InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(&pData->m_uRefCount), -0x10000));
    if (((uPrevious - 0x10000u) & 0xFFFF0000u) == 0)
        gpNonGCPoolManager->Deallocate(pData, static_cast<size_t>(pData->m_uMaxSize) + 9);
}

''' + s[b:]
assert s.count('    DebugDataC* result;')==1
s = s.replace('    DebugDataC* result;', '    DebugDataC* const result = static_cast<DebugDataC*>(gpNonGCPoolManager->Allocate(nBlock));')
s = s.replace('void EAStringC::DeallocateBuffer(const void* const pBuffer, const uint32_t uSize)\n{\n}',
              'void EAStringC::DeallocateBuffer(const void* const pBuffer, const uint32_t uSize)\n{\n    gpNonGCPoolManager->Deallocate(const_cast<void*>(pBuffer), uSize);\n}')
a=s.index('// The grow/copy-on-write workhorse')
b=s.index('void EAStringC::ChangeBuffer',a)
s=s[:a]+'// ARTIST @0x82AE34A0: in-place update or 12.5-percent-slack copy-on-write.\n'+s[b:]
a=s.index('void EAStringC::ChangeBuffer')
b=s.index('// ---------------------------------------------------------------------------\n// Out-of-line constructors',a)
old=s[a:b]
assert old.count('        IncreaseInternalRefCount();')==1
s=s[:a]+old.replace('        IncreaseInternalRefCount();\n','')+s[b:]
save(p,s)

p=ROOT/'src/SDKs/EATech/include/Apt/AptString/EAString.h'
s=p.read_text(encoding='utf-8')
s=s.replace('#include <cstdarg>   // va_list', '#include <cstdarg>   // va_list\n#include <cstddef>   // native record offsets')
s += '''
// ARTIST string header and StaticStringHelperT record used by the interned names.
CGS_STATIC_ASSERT((sizeof(EAStringC) == 4), "ARTIST EAStringC pointer");
CGS_STATIC_ASSERT((offsetof(EAStringC::StaticStringHelperT, sd.m_uRefCount) == 0), "ARTIST string references");
CGS_STATIC_ASSERT((offsetof(EAStringC::StaticStringHelperT, sd.m_uSize) == 2), "ARTIST string length");
CGS_STATIC_ASSERT((offsetof(EAStringC::StaticStringHelperT, sd.m_uMaxSize) == 4), "ARTIST string capacity");
CGS_STATIC_ASSERT((offsetof(EAStringC::StaticStringHelperT, sd.m_uHash) == 6), "ARTIST string hash");
CGS_STATIC_ASSERT((offsetof(EAStringC::StaticStringHelperT, pbuf) == 8), "ARTIST string bytes");
CGS_STATIC_ASSERT((sizeof(EAStringC::StaticStringHelperT) == 0x108), "ARTIST static string stride");
'''
save(p,s)

p=ROOT/'src/SDKs/EATech/Apt/AptStringPool.cpp'
s=p.read_text(encoding='utf-8')
a=s.index('// ---------------------------------------------------------------------------\n// saConstant --')
b=s.index('// The shared fixed-size pool',a)
names=re.findall(r'EAStringC\("([^"\\]+)"\)',s[a:b])
assert len(names)==88
records='\n'.join('    {{1, %d, 1, 0}, "%s"},' % (len(name),name) for name in names)
s=s[:a]+'''// ARTIST CRT initializer82C71F10 builds these writable 0x108-byte records.
// No allocator is needed before StringPool::Initialize publishes their addresses.
static EAStringC::StaticStringHelperT sStringPoolData[StringPool::KU_CONSTANT_COUNT] = {
'''+records+'''
};
EAStringC StringPool::saConstant[StringPool::KU_CONSTANT_COUNT];

'''+s[b:]
s=s.replace('// The shared fixed-size pool the bucket array is carved from (off_8324D808).\nextern DOGMA_PoolManager* gpAptPseudoDataPool;\n\n','')
s=s.replace('while (_InterlockedExchange(&gStringPoolTableLock, 1) != 0) {}',
            'while (_InterlockedCompareExchange(&gStringPoolTableLock, 1, 0) != 0) {}')
a=s.index('// ---------------------------------------------------------------------------\n// StringPool::Initialize')
b=s.index('// ---------------------------------------------------------------------------\n// FindOrCreate',a)
s=s[:a]+'''// ARTIST @0x82AE3630: publish the static names, then allocate the bucket table.
void StringPool::Initialize(int nBucketCount)
{
    StringPoolTableLock_Acquire();
    for (int i = 0; i < StringPool::KU_CONSTANT_COUNT; ++i)
        saConstant[i] = sStringPoolData[i];

    gpAptStringPoolBuckets = gpNonGCPoolManager->Allocate(sizeof(void*) * static_cast<unsigned>(nBucketCount));
    std::memset(gpAptStringPoolBuckets, 0, sizeof(void*) * static_cast<unsigned>(nBucketCount));
    gnAptStringPoolCount = static_cast<unsigned int>(nBucketCount);
    StringPoolTableLock_Release();
}

'''+s[b:]
s=s.replace('gpAptPseudoDataPool->Deallocate(gpAptStringPoolBuckets, sizeof(void*) * nCount);',
            'gpNonGCPoolManager->Deallocate(gpAptStringPoolBuckets, sizeof(void*) * nCount);')
assert s.count('        saConstant[i] = EAStringC();')==1
s=s.replace('        saConstant[i] = EAStringC();', '        saConstant[i].ClearConstantValue();')
s=s.replace('// Reset the interned AS-name table to empty (X360 writes the empty-string\n    // sentinel into each slot; the object-model equivalent is an empty EAStringC).',
            '// Reset the static names without releasing their non-pool-owned records.')
assert 'gpAptPseudoDataPool' not in s
save(p,s)
print('Restored three native string foundation paths')
