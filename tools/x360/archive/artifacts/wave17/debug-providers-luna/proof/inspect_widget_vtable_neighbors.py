import ida_bytes
import ida_funcs
import ida_nalt
import idautils
import idc

def emit(value): print(value, flush=True)

emit('input='+ida_nalt.get_input_file_path())
for start in [0x820DC304,0x820DC32C,0x820DC37C,0x820DC3D4,0x820DC418,0x820DC448,0x820DC4C8]:
    emit('=== address 0x%08X name=%s ===' % (start,idc.get_name(start)))
    for x in idautils.XrefsTo(start,0):
        emit('  xref from=0x%08X type=%d fn=%s' % (x.frm,x.type,idc.get_func_name(x.frm)))
    for i in range(12):
        a=start+i*4
        if not ida_bytes.is_loaded(a): break
        val=ida_bytes.get_dword(a)
        emit('  +%02X 0x%08X %s func=%s' % (i*4,val,idc.get_name(val),idc.get_func_name(val)))
