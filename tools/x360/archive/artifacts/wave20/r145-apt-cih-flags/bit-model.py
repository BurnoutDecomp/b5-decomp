# Portable arithmetic model for ARTIST packed fields; no game code executes.
fields = [0x80000000, 0x60000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000, 0x00800000, 0x007fff80]
for i,a in enumerate(fields):
    for b in fields[i+1:]: assert not (a & b), (hex(a),hex(b))
for old in [0,0xffffffff,0xaaaaaaaa,0x55555555,0x0800007f,0xdeadbeef]:
    for field, shift, values in [(0x60000000,29,range(8)),(0x08000000,27,range(4)),(0x007fff80,7,[0,1,0x7fff,0x8000,0xffff]),(0xfffc0000,18,[0,1,0x1fff,0x2000,0x3fff,0xffff])]:
        for value in values:
            new = ((old & ~field) | ((value << shift) & field)) & 0xffffffff
            assert (new & ~field) == (old & ~field)
            assert ((new & field) >> shift) == (value & (field >> shift))
    for raw in [0,1,0x7fff,0x8000,0xffff]:
        for step in [-1,1]:
            new = (old & ~0x007fff80) | (((raw+step)&0xffff)<<7)
            assert (new & ~0x007fff80)==(old & ~0x007fff80)
            assert ((new>>7)&0xffff)==((raw+step)&0xffff)
print("packed fields disjoint; inserts preserve neighbors; signed boundaries wrap")
