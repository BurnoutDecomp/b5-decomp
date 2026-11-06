from pathlib import Path
p=Path('vendor/dirtysdk/include/protoname.h');s=p.read_text();s=s.replace('    s32  sema;\n    s32  thread;\n    u32  timeout;','''#if defined(_XBOX)
    void* pDns;           // ARTIST HostentT +0x50: XNDNS*
    void* pPlatformData;  // ARTIST HostentT +0x54: event or DirtyLSP query
    u32   timeout;        // +0x58
#else
    s32  sema;
    s32  thread;
    u32  timeout;
#endif''');p.write_text(s)
