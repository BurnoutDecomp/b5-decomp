import ctypes as c
import hashlib
import json
import random
from pathlib import Path

root = Path(__file__).resolve().parent
dll = c.CDLL(str(root / 'crypto-host-check.dll'))
byte = c.c_uint8
u32 = c.c_uint32
i32 = c.c_int32
pointer = c.c_void_p

class Sha1(c.Structure):
    _fields_ = [('count', u32), ('partial', u32), ('H', u32 * 5), ('block', byte * 64)]

class Arc4(c.Structure):
    _fields_ = [('table', byte * 256), ('walk', byte), ('swap', byte)]

for name, args in {
    'CryptSha1Init': [c.POINTER(Sha1)],
    'CryptSha1Update': [c.POINTER(Sha1), pointer, u32],
    'CryptSha1Final': [c.POINTER(Sha1), pointer, u32],
    'CryptArc4Init': [c.POINTER(Arc4), pointer, i32, i32],
    'CryptArc4Apply': [c.POINTER(Arc4), pointer, i32],
    'CryptArc4Advance': [c.POINTER(Arc4), i32],
}.items():
    fn = getattr(dll, name)
    fn.argtypes = args
    fn.restype = None

rng = random.Random(360)
sha_cases = 0
for size in (0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 127, 128, 129, 4097):
    message = rng.randbytes(size)
    expected = hashlib.sha1(message).digest()
    for stride in (1, 17, 64, 211, max(size, 1)):
        for requested in (0, 7, 20, 32):
            state = Sha1()
            dll.CryptSha1Init(c.byref(state))
            for start in range(0, size, stride):
                chunk = message[start:start+stride]
                dll.CryptSha1Update(c.byref(state), chunk, len(chunk))
            assert state.count == size // 64 * 64 and state.partial == size % 64
            out = (byte * 32)(*[0xA5] * 32)
            dll.CryptSha1Final(c.byref(state), out, requested)
            copy = min(requested, 20)
            assert bytes(out) == expected[:copy] + b'\xA5' * (32-copy)
            assert state.count == size
            assert bytes(state.block[56:]) == (size * 8).to_bytes(8, 'big')
            assert state.partial == (size % 64 if size % 64 < 56 else 0)
            sha_cases += 1

state = Arc4()
dll.CryptArc4Init(c.byref(state), b'Key', 3, 1)
vector = c.create_string_buffer(b'Plaintext', 9)
dll.CryptArc4Apply(c.byref(state), vector, len(vector))
assert bytes(vector).hex() == 'bbf316e8d940af0ad3'

arc_cases = 0
for key_size in (0, 1, 3, 16, 256, 257):
    key = rng.randbytes(key_size)
    for iterations in (-3, 0, 1, 2, 5):
        table = list(range(256))
        swap = 0
        if key_size:
            for _ in range(max(1, iterations)):
                for walk in range(256):
                    swap = (swap + table[walk] + key[walk % key_size]) & 255
                    table[walk], table[swap] = table[swap], table[walk]
        start = Arc4()
        dll.CryptArc4Init(c.byref(start), key if key else None, key_size, iterations)
        assert list(start.table) == table and start.walk == start.swap == 0
        for length in (0, 1, 255, 256, 257, 1025):
            src = rng.randbytes(length)
            state = Arc4.from_buffer_copy(start)
            advance = Arc4.from_buffer_copy(start)
            out = c.create_string_buffer(src, length)
            if length:
                cut = length // 2
                dll.CryptArc4Apply(c.byref(state), out, cut)
                dll.CryptArc4Apply(c.byref(state), c.byref(out, cut), length-cut)
            else:
                dll.CryptArc4Apply(c.byref(state), None, -1)
            dll.CryptArc4Advance(c.byref(advance), length)
            oracle = table.copy()
            walk = swap = 0
            encoded = bytearray()
            for value in src:
                walk = (walk + 1) & 255
                swap = (swap + oracle[walk]) & 255
                oracle[walk], oracle[swap] = oracle[swap], oracle[walk]
                encoded.append(value ^ oracle[(oracle[walk] + oracle[swap]) & 255])
            assert bytes(out) == encoded
            assert bytes(advance) == bytes(state) == bytes(oracle + [walk, swap])
            arc_cases += 1

result = {'sha1_cases': sha_cases, 'arc4_cases': arc_cases, 'arc4_known_vector': True,
          'scope': 'Host semantic check of exact sources with width-only type shim.'}
(root/'host-check.json').write_text(json.dumps(result, indent=2)+'\n')
print(result)
