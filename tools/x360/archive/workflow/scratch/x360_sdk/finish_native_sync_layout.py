from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]/'b5-decomp'

def edit(name, transform):
    p=ROOT/name
    p.write_bytes(transform(p.read_text(encoding='utf-8')).replace('\n','\r\n').encode('utf-8'))

edit('src/SDKs/EATech/rwcore/filesys/device.cpp', lambda s:s.replace(
    'mCompletion.Wait(&mCompletionLock, lTimeoutAbsolute);',
    'mCompletion.Wait(&mCompletionLock, &lTimeoutAbsolute);'))

def device(s):
    a=s.index('            // offsets are NOT reproduced here:')
    b=s.index('            class Device',a)
    s=s[:a]+'''            // Native EAThread objects preserve these offsets.
'''+s[b:]
    needle='                DeviceDriver*         mpDriver;'
    assert needle in s
    a=s.index('            };',s.index(needle))
    s=s[:a]+'''
                void CheckLayout()
                {
                    CGS_STATIC_ASSERT((offsetof(Device, mLock) == 0x18), "ARTIST device lock");
                    CGS_STATIC_ASSERT((offsetof(Device, mWork) == 0x48), "ARTIST device work condition");
                    CGS_STATIC_ASSERT((offsetof(Device, mThread) == 0xA8), "ARTIST device worker");
                    CGS_STATIC_ASSERT((offsetof(Device, mCompletionLock) == 0xB0), "ARTIST device completion lock");
                    CGS_STATIC_ASSERT((offsetof(Device, mCompletion) == 0xE0), "ARTIST device completion condition");
                    CGS_STATIC_ASSERT((offsetof(Device, mpDriver) == 0x140), "ARTIST device driver");
                    CGS_STATIC_ASSERT((offsetof(Device, mu64ReadBudget) == 0x148), "ARTIST device read budget");
                }
'''+s[a:]
    return s

edit('src/SDKs/EATech/rwcore/filesys/device.h',device)

edit('src/SDKs/Realmc/RealmcMessageQueue.h',lambda s:s.replace(
    '} // namespace RealmcCore',
    'CGS_STATIC_ASSERT((sizeof(MessageQueue) == 0xB8), "ARTIST message queue size");\n\n} // namespace RealmcCore'))
