import ida_bytes
import ida_funcs
import ida_nalt
import idautils
import idc

def emit(value): print(value, flush=True)

def small_fn(target):
    f=ida_funcs.get_func(target)
    if not f or f.end_ea-f.start_ea > 16: return False
    lines=[]
    for ea in idautils.Heads(f.start_ea, f.end_ea):
        lines.append(idc.generate_disasm_line(ea,0) or '')
    return len(lines)>0 and all(('blr' in x or 'li        r3, 0' in x or 'li        r3, 1' in x) for x in lines)

emit('input='+ida_nalt.get_input_file_path())
for start in idautils.Segments():
    seg=idc.get_segm_attr(start, idc.SEGATTR_START)
    end=idc.get_segm_attr(start, idc.SEGATTR_END)
    emit('segment 0x%08X..0x%08X name=%s' % (seg,end,idc.get_segm_name(start)))
update=idc.get_name_ea_simple('CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct')
render=idc.get_name_ea_simple('CgsDev::DebugUI::WidgetLineGraph::Render')
emit('slot0 no-op alias=0x%08X slot1 render=0x%08X' % (update,render))
for x in idautils.XrefsTo(update,0):
    if ida_funcs.get_func(x.frm): continue
    following=ida_bytes.get_dword(x.frm+4) if ida_bytes.is_loaded(x.frm+4) else 0
    emit('data ref 0x%08X seg=%s next=0x%08X nextname=%s smallfn=%s' % (x.frm,idc.get_segm_name(x.frm),following,idc.get_name(following),small_fn(following)))
