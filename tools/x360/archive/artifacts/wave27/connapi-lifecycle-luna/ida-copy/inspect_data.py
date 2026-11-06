import ida_bytes, idc
for ea in (0x8324E919,):
    print('ADDR %08X' % ea)
    print('STR %r' % (idc.get_strlit_contents(ea, -1, idc.STRTYPE_C),))
    print('BYTES %s' % (' '.join('%02X' % b for b in bytearray(ida_bytes.get_bytes(ea, 48) or b'')),))
