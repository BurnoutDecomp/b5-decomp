from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
p=ROOT/'build/x360/native-atomic-probes.cpp'
p.write_text('''#include "SDKs/EATech/eathread/eathread_atomic.h"
#include <new>
template <class T> T Exercise(EA::Thread::AtomicInt<T>* p, unsigned op, T value, T condition)
{
    switch (op)
    {
    case 0: return p->SetValue(value);
    case 1: return p->SetValueConditional(value, condition);
    case 2: return p->Increment();
    case 3: return p->Decrement();
    case 4: return p->Add(value);
    case 5: return *p = value;
    case 6: return (*p)++;
    case 7: return (*p)--;
    case 8: new (p) EA::Thread::AtomicInt<T>(value); return p->GetValue();
    case 9: return p->GetValue();
    case 10: return p->GetValueRaw();
    case 11: return *p += value;
    case 12: return *p -= value;
    default: new (p) EA::Thread::AtomicInt<T>(); return p->GetValueRaw();
    }
}
extern "C" s32 AtomicS32(EA::Thread::AtomicInt<s32>* p,unsigned op,s32 value,s32 condition)
{ return Exercise(p,op,value,condition); }
extern "C" u32 AtomicU32(EA::Thread::AtomicInt<u32>* p,unsigned op,u32 value,u32 condition)
{ return Exercise(p,op,value,condition); }
extern "C" s64 AtomicS64(EA::Thread::AtomicInt<s64>* p,unsigned op,s64 value,s64 condition)
{ return Exercise(p,op,value,condition); }
extern "C" u64 AtomicU64(EA::Thread::AtomicInt<u64>* p,unsigned op,u64 value,u64 condition)
{ return Exercise(p,op,value,condition); }
''',encoding='utf-8')
q=ROOT/'src/GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.h'
s=q.read_text()
a=s.index('    };',s.index('        s32                 miNumResultsAtEnd;'))
s=s[:a]+'''
        void CheckLayout()
        {
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, mpcResultBuffer) == 8), "ARTIST result buffer");
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, miResultSize) == 0x10), "ARTIST result stride");
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, mpcDataBuffer) == 0x18), "ARTIST data buffer");
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, mbStreaming) == 0x20), "ARTIST streaming flag");
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, miNextResult) == 0x24), "ARTIST result cursor");
            CGS_STATIC_ASSERT((offsetof(DataStreamResultReader, miNumResultsAtEnd) == 0x28), "ARTIST result count");
        }
'''+s[a:]
s+='\nCGS_STATIC_ASSERT((sizeof(CgsMemory::DataStreamResultReader) == 0x30), "ARTIST data stream reader size");\n'
q.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
