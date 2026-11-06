import hashlib,json,pathlib,re
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
proof=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\debug-providers-luna\proof')
changes={}

def write_new(rel,text):
    p=repo/rel
    if p.exists(): raise SystemExit('expected new path absent: '+rel)
    p.parent.mkdir(parents=True,exist_ok=True)
    data=text.replace('\n', '\r\n').encode('utf-8')
    p.write_bytes(data)
    changes[rel]={'pre_exists':False,'post_sha256':hashlib.sha256(data).hexdigest(),'post_bytes':len(data)}

def modify(rel,transform):
    p=repo/rel; old=p.read_bytes(); new=transform(old)
    if new==old: raise SystemExit('no change made: '+rel)
    p.write_bytes(new)
    changes[rel]={'pre_exists':True,'pre_sha256':hashlib.sha256(old).hexdigest(),'post_sha256':hashlib.sha256(new).hexdigest(),'pre_bytes':len(old),'post_bytes':len(new)}

linegraph='src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.h'
def edit_linegraph(data):
    eol=b'\r\n' if b'\r\n' in data else b'\n'
    old=b'#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h" // Internal::DebugInternal (Widget base)'
    new=b'#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h" // native debug widget base'
    if data.count(old)!=1: raise SystemExit('linegraph include marker mismatch')
    data=data.replace(old,new,1)
    old_comment=(b'// The Widget base mirrors the committed MenuItem / Window pattern: an empty (vptr-only)'+eol+
                 b'// CgsDev::Internal::DebugInternal-derived base that exposes the debug-UI palette/metrics/'+eol+
                 b'// renderer accessors (declaration-only; bodies live in their own TU).')
    new_comment=(b'// Widget base declaration and virtual slot order live in CgsWidget.h.')
    if data.count(old_comment)!=1: raise SystemExit('linegraph base comment mismatch')
    data=data.replace(old_comment,new_comment,1)
    old_metrics=b'        struct Metrics;'+eol
    if data.count(old_metrics)!=1: raise SystemExit('Metrics forward declaration mismatch')
    data=data.replace(old_metrics,b'',1)
    start=b'        // The debug-widget base (empty, vptr-only) -- mirrors MenuItem/Window: derives from'+eol
    end=b'        struct WidgetLineGraph : public Widget'+eol
    i=data.find(start)
    j=data.find(end,i)
    if i<0 or j<0: raise SystemExit('local Widget block markers missing')
    data=data[:i]+data[j:]
    old_render=b'            void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight);'
    new_render=b'            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight);'
    if data.count(old_render)!=1: raise SystemExit('linegraph Render declaration mismatch')
    data=data.replace(old_render,new_render,1)
    return data
modify(linegraph,edit_linegraph)

widget_h='''#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"

// CgsDev::DebugUI::Widget -- the empty debug-UI base. ARTIST installs the WidgetLineGraph
// vtable at 0x820DC32C from MenuItemVariableLineGraph::Render: slot 0 is the one-instruction
// no-op at 0x8284CB38; slot 1 is WidgetLineGraph::Render at 0x82824558. This confirms the
// Update/Render slot order and that Widget has no virtual destructor slot. The base Render
// body itself is not emitted as a separate ARTIST target because the used derived widget
// overrides it; DecFIGS records its default body as empty.
namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Widget : public Internal::DebugInternal
        {
            virtual void Update(f32 lfTimeStep);
            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY,
                                f32 lfWidth, f32 lfHeight);

        protected:
            const Palette&          GetPalette() const;
            const Metrics&          GetMetrics() const;
            Debug2DImmediateRender* Get2DRenderer() const;
        };
    }
}
'''
write_new('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h',widget_h)

widget_cpp='''#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

namespace CgsDev
{
    namespace DebugUI
    {
        // ARTIST off_820DC32C uses 0x8284CB38 in the Widget::Update slot; that target is a
        // single blr. Keep the base update as the native no-op.
        void Widget::Update(f32)
        {
        }

        // DecFIGS CgsWidget.cpp identifies the base Render implementation as empty. ARTIST's
        // used WidgetLineGraph vtable replaces this slot with its real Render target, so there
        // is no standalone base Render calltarget in the X360 export.
        void Widget::Render(Debug2DImmediateRender*, f32, f32, f32, f32)
        {
        }

        const Palette& Widget::GetPalette() const
        {
            return GetUI().GetPalette();
        }

        const Metrics& Widget::GetMetrics() const
        {
            return GetUI().GetMetrics();
        }

        Debug2DImmediateRender* Widget::Get2DRenderer() const
        {
            return GetUI().Get2DRenderer();
        }
    }
}
'''
write_new('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.cpp',widget_cpp)

render='src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp'
def insert_maybe(data):
    eol=b'\r\n' if b'\r\n' in data else b'\n'
    marker=b'namespace CgsDev'+eol+b'{'+eol
    body=(b'// X360 0x82824048. Preserve the native global helper boundary: pack (x,y,0,0), then delegate'+eol+
          b'// to the real member overload with screen checking disabled. The helper result is not consumed.'+eol+
          b'void MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,'+eol+
          b'                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool /*lbCheckScreen*/)' +eol+
          b'{'+eol+
          b'    const rw::math::vpu::Vector2 lv2Position = MakeDebugVector2(lfX, lfY, 0.0f, 0.0f);'+eol+
          b'    lpDisplay->DrawText(lpcText, lv2Position, lfScale, lColour, false);'+eol+
          b'}'+eol+eol)
    if data.count(marker)!=1: raise SystemExit('2D namespace insertion point mismatch')
    return data.replace(marker,body+marker,1)
modify(render,insert_maybe)

out=proof/'SCOPE_EDIT.json'
out.write_text(json.dumps({'changed_files':changes},indent=2)+'\n',encoding='utf-8')
print('changed_source_paths=%d' % len(changes))
for rel in changes: print(rel)
print('manifest='+str(out))
