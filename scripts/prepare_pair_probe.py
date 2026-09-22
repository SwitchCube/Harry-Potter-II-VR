"""Version-specific call targets for one bounded two-view render experiment."""
import argparse
import json
from prepare_observer import describe
from analyze_original import verify_originals
from inspect_pe import PE
from project_paths import inside, write_text

FUNCTIONS = [
    ('CreateMaster','Render.dll','?CreateMasterFrame@URender@@UAEPAUFSceneNode@@PAVUViewport@@VFVector@@VFRotator@@PAUFScreenBounds@@@Z'),
    ('FinishMaster','Render.dll','?FinishMasterFrame@URender@@UAEXXZ'),
    ('ProcessEvent','Core.dll','?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z'),
    ('D3DUnlock','D3DDrv.dll','?Unlock@UD3DRenderDevice@@UAEXH@Z'),
]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--refresh',action='store_true')
    args=parser.parse_args()
    verify_originals()
    rows=[]
    for label,module,symbol in FUNCTIONS:
        _,row=describe(module,symbol);row['label']=label;rows.append(row)
    checks=[('Engine.dll',0xa8edb,'ff5074'),('Engine.dll',0xa8ede,'8b4f5c'),
            ('Render.dll',0x27a80,'ff5210'),('Render.dll',0x1fcdb,'8b91a8000000'),
            ('Render.dll',0x1fffa,'8b86a8000000'),('D3DDrv.dll',0x38d5,'8b86a4090000'),
            ('Core.dll',0x9d50,'8b0185c0'),('Core.dll',0x9d5b,'83c00cc3'),
            ('Core.dll',0x44965,'668b567a'),('Core.dll',0x449cf,'668b467c'),
            ('Engine.dll',0x806b4,'c20c00'),('Render.dll',0x30ba0,'ff5210'),
            ('Render.dll',0x30b81,'c745e800000000')]
    for module,rva,hex_bytes in checks:
        pe=PE(inside('system/'+module).read_bytes())
        if pe.take(pe.offset(rva),len(bytes.fromhex(hex_bytes))) != bytes.fromhex(hex_bytes):
            raise ValueError('Unreviewed lifecycle instruction: '+module)
    record=dict(schema_version=1,targets=rows,checks=checks,
                mode='One extra master frame and draw, same tick, original D3D7 backbuffer readback',
                engine_draw_return_rva=0xa8ede,overlay_return_rva=0x27a83,
                renderer_depth_offset=0xa8,viewport_renderdevice_offset=0x5c,device7_offset=0x9a4,
                actor_dispatch_return_rva=0x806b4,update_return_rva=0x30ba3,
                function_parameter_bytes_offset=0x7a,function_return_offset_offset=0x7c,
                frame_bytes=0x16c,headset_test=False)
    lines=['// Generated from reviewed, fingerprinted original functions.','#pragma once',
           'static const Target kPairTargets[] = {']
    for row in rows:
        blob='{'+','.join('0x'+row['thunk_bytes'][i:i+2] for i in range(0,10,2))+'}'
        lines.append(' {"%s", L"%s", 0x%x, 0x%x, %s},' % (row['label'],row['module'],row['export_rva'],row['implementation_rva'],blob))
    lines+=['};','static const DWORD kDrawReturnRva=0xa8ede;',
            'static const DWORD kOverlayReturnRva=0x27a83;',
            'static const DWORD kActorDispatchReturnRva=0x806b4;',
            'static const DWORD kUpdateReturnRva=0x30ba3;',
            'static const DWORD kRendererDepthOffset=0xa8;',
            'static const DWORD kViewportDeviceOffset=0x5c;',
            'static const DWORD kD3D7Offset=0x9a4;']
    outputs={'config/pair-targets.json':json.dumps(record,indent=2)+'\n','src/pair_target.h':'\n'.join(lines)+'\n'}
    for name,text in outputs.items():
        if args.check:
            if inside(name).read_text(encoding='utf-8')!=text: raise ValueError('Stale pair target: '+name)
        else: write_text(name,text,exclusive=not args.refresh)
    print('Pair targets checked.' if args.check else 'Pair targets generated; runtime validation pending.')

if __name__=='__main__': main()
