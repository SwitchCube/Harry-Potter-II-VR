"""Generate native addresses only from pinned original exports and reviewed instructions."""
import argparse,json,hashlib
from analyze_original import verify_originals,follow_jumps
from inspect_pe import PE
from project_paths import inside,write_text

SYMBOLS=[
 ('scriptedtick','Engine.dll','?Tick@UScriptedTexture@@UAEXM@Z'),
 ('viewportmessage','WinDrv.dll','?ViewportWndProc@UWindowsViewport@@QAEJIIJ@Z'),
 ('viewportwindow','WinDrv.dll','?GetWindow@UWindowsViewport@@UAEPAXXZ'),
 ('audioplay','ALAudio.dll','?PlaySound@UALAudioSubsystem@@UAEHPAVAActor@@HPAVUSound@@VFVector@@MMMHMM@Z'),
 ('audiostop','ALAudio.dll','?StopSoundSource@UALAudioSubsystem@@QAEXH@Z'),
 ('audioupdate','ALAudio.dll','?Update@UALAudioSubsystem@@UAEXPAUFSceneNode@@@Z'),
 ('soundduration','Engine.dll','?GetDuration@USound@@UAEMXZ'),
 ('childframe','Render.dll','?CreateChildFrame@URender@@UAEPAUFSceneNode@@PAU2@PAVFSpanBuffer@@PAVULevel@@HHMABVFPlane@@ABVFCoords@@PAUFScreenBounds@@@Z'),
 ('applyanim','Engine.dll','?ApplyAnim@USkeletalMesh@@ABEXPAVAActor@@PAUCFSkelHeader@1@_N@Z'),
 ('farmove','Engine.dll','?FarMoveActor@ULevel@@UAEHPAVAActor@@VFVector@@HH@Z'),
 ('indexedobject','Core.dll','?GetIndexedObject@UObject@@SAPAV1@H@Z'),
 ('destroyactor','Engine.dll','?DestroyActor@ULevel@@UAEHPAVAActor@@H@Z'),
 ('viewportexec','WinDrv.dll','?Exec@UWindowsViewport@@UAEHPBGAAVFOutputDevice@@@Z'),
 ('exec','Engine.dll','?Exec@UGameEngine@@UAEHPBGAAVFOutputDevice@@@Z'),
 ('unlock','D3DDrv.dll','?Unlock@UD3DRenderDevice@@UAEXH@Z'),
 ('loadmap','Engine.dll','?LoadMap@UGameEngine@@UAEPAVULevel@@ABVFURL@@PAVUPendingLevel@@PBV?$TMap@VFString@@V1@@@AAVFString@@@Z'),
 ('preprocess','Engine.dll','?PreProcess@UInput@@UAEHW4EInputKey@@W4EInputAction@@M@Z'),
 ('readinput','Engine.dll','?ReadInput@UInput@@UAEXMAAVFOutputDevice@@@Z'),
 ('inputevent','Engine.dll','?InputEvent@UGameEngine@@UAEHPAVUViewport@@W4EInputKey@@W4EInputAction@@M@Z'),
 ('keyname','Engine.dll','?GetKeyName@UInput@@UBEPBGW4EInputKey@@@Z'),
 ('internal','Core.dll','?ProcessInternal@UObject@@QAEXAAUFFrame@@QAX@Z'),
 ('line','Engine.dll','?SingleLineCheck@ULevel@@UAEHAAUFCheckResult@@PAVAActor@@ABVFVector@@2KV4@E@Z'),
 ('tick','Engine.dll','?Tick@UGameEngine@@UAEXM@Z'),
 ('draw','Engine.dll','?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z'),
 ('destroy','Engine.dll','?Destroy@UGameEngine@@UAEXXZ'),
 ('size','Engine.dll','?ComputeRenderSize@FSceneNode@@QAEXXZ'),
 ('coords','Engine.dll','?ComputeRenderCoords@FSceneNode@@QAEXAAVFVector@@AAVFRotator@@@Z'),
 ('create','Render.dll','?CreateMasterFrame@URender@@UAEPAUFSceneNode@@PAVUViewport@@VFVector@@VFRotator@@PAUFScreenBounds@@@Z'),
 ('finish','Render.dll','?FinishMasterFrame@URender@@UAEXXZ'),
 ('world','Render.dll','?DrawWorld@URender@@UAEXPAUFSceneNode@@@Z'),
 ('occlude','Render.dll','?OccludeFrame@URender@@QAEXPAUFSceneNode@@@Z'),
 ('dispatch','Core.dll','?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z'),
 ('name','Core.dll','??0FName@@QAE@PBGW4EFindName@@@Z'),
 ('field','Core.dll','?FindObjectField@UObject@@QAEPAVUField@@VFName@@H@Z'),
 ('mesh','Render.dll','?DrawMesh@URender@@QAEXPAUFSceneNode@@PAUFDynamicSprite@@PAVAActor@@ABVFCoords@@K@Z'),
 ('lodmesh','Render.dll','?DrawLodMesh@URender@@QAEXPAUFSceneNode@@PAUFDynamicSprite@@PAVAActor@@ABVFCoords@@K@Z')]

def main():
 p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');p.add_argument('--refresh',action='store_true');args=p.parse_args()
 verify_originals();rows=[]
 for label,module,symbol in SYMBOLS:
  data=inside('system/'+module).read_bytes();pe=PE(data);ex=next(e for e in pe.exports() if e['name']==symbol)
  start,_=follow_jumps(pe,int(ex['rva'],16))
  rows.append(dict(label=label,module=module,symbol=symbol,rva=start,prefix=pe.take(pe.offset(start),20).hex(),sha256=hashlib.sha256(data).hexdigest()))
 data=inside('system/Core.dll').read_bytes();pe=PE(data)
 assert pe.take(pe.offset(0x43642),5).hex()=='e8b2dbfbff'
 start,chain=follow_jumps(pe,0x11f9);assert start==0x494c0
 rows.append(dict(label='property',module='Core.dll',symbol='internal cdecl property lookup called by execGetPropertyText',rva=start,prefix=pe.take(pe.offset(start),20).hex(),sha256=hashlib.sha256(data).hexdigest()))
 # Validate relocated instructions exactly, including absolute addresses in
 # SEH prologues when Windows cannot use a DLL's preferred base address.
 for row in rows:
  pe=PE(inside('system/'+row['module']).read_bytes());row['image_base']=pe.image_base;row['relocations']=[]
  directory,size=pe.directory(5);position=0
  while directory and position<size:
   offset=pe.offset(directory+position);page,block=pe.unpack('<II',offset)
   if block<8 or position+block>size:raise ValueError('Invalid relocation block')
   for i in range((block-8)//2):
    entry=pe.u16(offset+8+i*2);kind=entry>>12;address=page+(entry&4095)
    if row['rva']<=address<row['rva']+16 and kind:
     if kind!=3:raise ValueError('Unsupported target relocation')
     row['relocations'].append(address-row['rva'])
   position+=block
  if len(row['relocations'])>4:raise ValueError('Too many target relocations')
 checks=[('Engine.dll',0x126823,'8b86a4000000'),('Engine.dll',0x126829,'8b401c'),('Engine.dll',0x126855,'8b4e38'),('Engine.dll',0x12685b,'0faf4e34'),('Core.dll',0xb4e0,'8b410cc3'),('Core.dll',0x44363,'8b460c'),('Core.dll',0x4436b,'8b401c'),('Core.dll',0x44372,'8b4020'),('ALAudio.dll',14352,'81e2ff7f0000'),('ALAudio.dll',14360,'c1f80f'),('ALAudio.dll',14417,'c1e00f'),('ALAudio.dll',14428,'c745d800800000'),('ALAudio.dll',14463,'6800800000'),('ALAudio.dll',14555,'6800800000'),('ALAudio.dll',14952,'c7473000800000'),('Render.dll',0x20671,'ff158057b710'),('Engine.dll',0x12c9d8,'5757568bca'),('Engine.dll',0x12d838,'6a016a0056'),('Engine.dll',0xb38b7,'c21800'),('Engine.dll',0xb3b43,'c21800'),('Engine.dll',0x11a801,'8b8f94000000'),('Engine.dll',0x11a821,'ff9090000000'),('Core.dll',0xb460,'8b4118c3'),('Core.dll',0x116b0,'8b4118c3'),('Engine.dll',0xb0dd7,'c20800'),('WinDrv.dll',0x7ee7,'8d4fd4'),('Engine.dll',0xa215f,'8b57d88d4fd8'),('Engine.dll',0xa216c,'ff928c000000'),('Core.dll',0x710b2,'8b415c'),('Core.dll',0x33556,'8b565c'),('Engine.dll',0x8ae34,'8b5130d982c8030000'),('Engine.dll',0x8aea4,'895108'),('Engine.dll',0xaf63b,'8b80a00e0000'),('Engine.dll',0xaf264,'8a9408b00e0000'),('Core.dll',0x44793,'8b4604'),('Core.dll',0x44796,'8b4e10'),('Engine.dll',0x11adf0,'8b8f94000000'),('Core.dll',0x7022b,'8b4638'),('Core.dll',0x70234,'8b5648'),('Core.dll',0xb370,'8b4124c3'),
         ('Render.dll',0x1cdbd,'89462c'),('Engine.dll',0x9aa1e,'89475c'),('Render.dll',0x279ed,'8b432c8b485c')]
 for module,rva,value in checks:
  pe=PE(inside('system/'+module).read_bytes());assert pe.take(pe.offset(rva),len(bytes.fromhex(value))).hex()==value
 lines=['#pragma once','struct NativeTarget {const char* label;const wchar_t* module;unsigned long rva;unsigned char prefix[20];unsigned long imageBase;unsigned relocationCount;unsigned char relocations[4];};','static const NativeTarget kNativeTargets[]={']
 for r in rows:lines.append(' {"%s",L"%s",0x%x,{%s},0x%x,%u,{%s}},'%(r['label'],r['module'],r['rva'],','.join('0x'+r['prefix'][i:i+2] for i in range(0,40,2)),r['image_base'],len(r['relocations']),','.join(str(i) for i in r['relocations']) or '0'))
 lines+=['};']
 outputs={'src/native_targets.h':'\n'.join(lines)+'\n','config/native-targets.json':json.dumps(dict(targets=rows,checks=checks),indent=2)+'\n'}
 for path,value in outputs.items():
  if args.check:
   if inside(path).read_text(encoding='utf-8')!=value:raise ValueError('Stale native target '+path)
  else:write_text(path,value,exclusive=not args.refresh)
 print('Native targets checked' if args.check else 'Native targets generated')
if __name__=='__main__':main()
