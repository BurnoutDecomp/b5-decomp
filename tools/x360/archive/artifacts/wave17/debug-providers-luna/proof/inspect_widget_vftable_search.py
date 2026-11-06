import ida_bytes
import ida_nalt
import ida_segment
import idautils
import idc

def emit(value): print(value, flush=True)

emit('input='+ida_nalt.get_input_file_path())
for ea in range(0x820DC2F0,0x820DC500,4):
    if ida_bytes.is_loaded(ea):
        val=ida_bytes.get_dword(ea)
        emit('0x%08X: 0x%08X name=%s' % (ea,val,idc.get_name(val)))
        raw=ida_bytes.get_bytes(ea,4)
        if raw and all(32<=b<127 for b in raw): emit('  ascii='+raw.decode('ascii'))

emit('rdata pairs of shared no-op target 0x8284CB38:')
noop=0x8284CB38
for seg_start in idautils.Segments():
    if idc.get_segm_name(seg_start) != '.rdata': continue
    seg_end=idc.get_segm_attr(seg_start,idc.SEGATTR_END)
    for ea in range(seg_start,seg_end-7,4):
        if ida_bytes.get_dword(ea)==noop and ida_bytes.get_dword(ea+4)==noop:
            emit('pair at 0x%08X' % ea)
            emit('  prev=%s name=%s refs=%s' % (hex(ida_bytes.get_dword(ea-4)),idc.get_name(ida_bytes.get_dword(ea-4)),','.join('0x%08X'%x.frm for x in idautils.XrefsTo(ea,0))))
            emit('  refs+4='+','.join('0x%08X'%x.frm for x in idautils.XrefsTo(ea+4,0)))

emit('strings containing Widget:')
for s in idautils.Strings():
    value=str(s)
    if 'Widget' in value or 'widget' in value:
        emit('  0x%08X %s' % (s.ea,value))
