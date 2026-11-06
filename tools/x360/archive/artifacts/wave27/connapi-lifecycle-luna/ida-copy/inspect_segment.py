import ida_segment, idc
for ea in (0x8324E919, 0x8324E918, 0x82B14608, 0x82F7B77C):
    s = ida_segment.getseg(ea)
    if s is None:
        print('%08X NOSEG' % ea)
    else:
        print('%08X SEG %s %08X-%08X type=%d perm=%X name=%s' % (ea, ida_segment.get_segm_name(s), s.start_ea, s.end_ea, s.type, s.perm, idc.get_name(ea)))
