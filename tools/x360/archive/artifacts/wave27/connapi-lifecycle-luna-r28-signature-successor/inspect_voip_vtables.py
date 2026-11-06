import ida_bytes, ida_segment, idc
for base in (0x82FA76D4,0x82FA76FC):
    print('VTABLE %08X %s' % (base,idc.get_name(base)))
    for i in range(16):
        ea=base+4*i
        value=ida_bytes.get_dword(ea)
        print('%08X +%02X %08X %s' % (ea,4*i,value,idc.get_name(value)))
