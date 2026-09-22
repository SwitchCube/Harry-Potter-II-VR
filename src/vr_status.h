#pragma once
struct HandStatusRow {std::wstring name,label;int count=0,maximum=0,page=0;bool maxShown=false;};
static std::vector<HandStatusRow> handRows;
static std::map<std::wstring,std::pair<int,int>> previousStatus;
static VrPanel statusPanel;static VrMenu statusOverlay("left-status");
static ULONGLONG statusAutoUntil=0,statusLastRead=0;static int statusAutoPage=0,statusOffset=0;static std::wstring statusFocus;
static const wchar_t* statusLabel(const std::wstring& n,int& page){
 struct Label {const wchar_t* name;const wchar_t* text;int page;};
 static const Label labels[]={
 {L"StatusItemHealth",tr(L"Leben",L"Health"),0},{L"StatusItemJellybeans",tr(L"Bohnen",L"Beans"),0},{L"StatusItemWiggenWell",tr(L"Heiltr\u00e4nke",L"Wiggenweld potions"),0},
 {L"StatusItemFlobberMucus",tr(L"Flubberwurmschleim",L"Flobberworm mucus"),0},{L"StatusItemWiggenBark",tr(L"Wiggenbaumrinde",L"Wiggentree bark"),0},{L"StatusItemBicorn",tr(L"Zweihorn-Horn",L"Bicorn horn"),0},{L"StatusItemBoomslang",tr(L"Baumschlangenhaut",L"Boomslang skin"),0},{L"StatusItemBitOGoyle",tr(L"Goyles Haar",L"Goyle's hair"),0},
 {L"StatusItemBronzeCards",tr(L"Bronzekarten",L"Bronze cards"),1},{L"StatusItemSilverCards",tr(L"Silberkarten",L"Silver cards"),1},{L"StatusItemGoldCards",tr(L"Goldkarten",L"Gold cards"),1},{L"StatusItemWizardCards",tr(L"Zaubererkarten",L"Wizard cards"),1},{L"StatusItemStars",tr(L"Herausforderungssterne",L"Challenge stars"),1},
 {L"StatusItemLock1",tr(L"Silberschl\u00fcssel 1",L"Silver key 1"),1},{L"StatusItemLock2",tr(L"Silberschl\u00fcssel 2",L"Silver key 2"),1},{L"StatusItemLock3",tr(L"Silberschl\u00fcssel 3",L"Silver key 3"),1},{L"StatusItemLock4",tr(L"Silberschl\u00fcssel 4",L"Silver key 4"),1},
 {L"StatusItemGryffindorPts",L"Gryffindor",2},{L"StatusItemRavenclawPts",L"Ravenclaw",2},{L"StatusItemHufflePuffPts",L"Hufflepuff",2},{L"StatusItemSlytherinPts",L"Slytherin",2},
 {L"StatusItemNimbus",L"Nimbus 2001",0},{L"StatusItemQArmor",tr(L"Quidditch-R\u00fcstung",L"Quidditch armour"),0}};
 for(const auto& l:labels)if(!_wcsicmp(n.c_str(),l.name)){page=l.page;return l.text;}return nullptr;
}
static bool hideStatusItem(void* self,void* frame){
 if(startup.mode<3||startup.mode==6||!vrPlayer||menuVisible||levelLoadDepth||objectName(mem<void*>(frame,4))!=L"DrawItem")return false;
 auto name=objectName(mem<void*>(self,0x24));int page=0;return name!=L"StatusItemHealth"&&statusLabel(name,page)!=nullptr;
}
static void updateStatus(){
 if(startup.mode<3||startup.mode==6||!vrPlayer||levelLoadDepth||GetTickCount64()-statusLastRead<100)return;statusLastRead=GetTickCount64();
 void* manager=objectField(vrPlayer,L"managerStatus");if(!manager)return;
 bool initial=previousStatus.empty();std::vector<HandStatusRow> rows;
 void* group=objectField(manager,L"sgList");unsigned groups=0,items=0;
 for(;group&&groups++<32;group=objectField(group,L"sgNext"))for(void* item=objectField(group,L"siList");item&&items++<128;item=objectField(item,L"siNext")){
  HandStatusRow row;row.name=objectName(mem<void*>(item,0x24));const auto label=statusLabel(row.name,row.page);if(!label)continue;row.label=label;
  row.count=mem<int>(item,field(item,L"nCount",4,L"IntProperty"));row.maximum=mem<int>(item,field(item,L"nMaxCount",4,L"IntProperty"));row.maxShown=boolField(item,L"bDisplayMaxCount");int potential=mem<int>(item,field(item,L"nCurrCountPotential",4,L"IntProperty"));
  auto value=std::make_pair(row.count,potential);auto old=previousStatus.find(row.name);bool changed=!initial&&(old==previousStatus.end()?row.count>0:old->second!=value);previousStatus[row.name]=value;
  if(changed){statusAutoUntil=GetTickCount64()+4000;statusAutoPage=row.page;statusFocus=row.name;if(row.name==L"StatusItemHealth")healthAutoUntil=statusAutoUntil;
   log("{\"event\":\"hand_status_changed\",\"item\":\"%ls\",\"count\":%d,\"potential\":%d,\"page\":%d}",row.name.c_str(),row.count,potential,row.page);}
  if(row.name!=L"StatusItemHealth")rows.push_back(row);
 }
 need(groups<=32&&items<=128,"Status list cycle or unsupported size");handRows.swap(rows);
}
static void updateStatusOverlay(){
 bool held=statusInspect;bool show=(held||GetTickCount64()<statusAutoUntil)&&vrSettings.hud&&!menuVisible&&!cinemaActive&&!headBlocked;
 if(startup.mode==3)show=show&&runtime.compositor->CanRenderScene()&&vrInput.leftGrip.bActive&&vrInput.leftGrip.pose.bPoseIsValid&&vrInput.leftGrip.pose.bDeviceIsConnected;
 else show=startup.mode==5&&profileFlag(L"replay-status.flag")&&!menuVisible;
 if(!show){statusOverlay.hide();return;}
 int page=held?statusPage:statusAutoPage;
 std::vector<const HandStatusRow*> rows;for(const auto& r:handRows)if(r.page==page)rows.push_back(&r);
 if(!held){statusOffset=0;for(unsigned i=0;i<rows.size();++i)if(rows[i]->name==statusFocus)statusOffset=(i/5)*5;}
 if(statusOffset>=int(rows.size()))statusOffset=0;
 if(held){static bool latched=false;float y=vrInput.move.y;if(std::fabs(y)<.3f)latched=false;if(std::fabs(y)>.7f&&!latched){statusOffset=y<0?statusOffset+5:statusOffset-5;if(statusOffset<0)statusOffset=std::max(0,((int(rows.size())-1)/5)*5);if(statusOffset>=int(rows.size()))statusOffset=0;latched=true;}}
 std::wstring content=std::to_wstring(page)+L":"+std::to_wstring(statusOffset);for(auto r:rows)content+=r->name+std::to_wstring(r->count)+L"/"+std::to_wstring(r->maximum);
 static std::wstring oldContent;
 if(!statusPanel.dc||content!=oldContent){
  oldContent=content;statusPanel.open(720,480);statusPanel.clear();const wchar_t* titles[]={tr(L"Leben & Vorr\u00e4te",L"Health & supplies"),tr(L"Karten & Sammelobjekte",L"Cards & collectibles"),tr(L"Hauspunkte",L"House points")};statusPanel.text(30,12,660,titles[page],34,RGB(236,198,116));
  int y=95;for(unsigned i=statusOffset;i<rows.size()&&i<unsigned(statusOffset+5);++i){auto r=rows[i];std::wstring count=std::to_wstring(r->count);if(r->maxShown&&r->maximum>0)count+=L" / "+std::to_wstring(r->maximum);statusPanel.text(30,y,480,r->label.c_str(),30);statusPanel.text(520,y,170,count.c_str(),34);y+=58;}
  if(rows.empty())statusPanel.text(30,100,650,L"Noch keine Eintr\u00e4ge",28);
  statusPanel.text(30,405,660,tr(L"Stick: links/rechts = Seite | oben/unten = Liste",L"Stick: left/right = page | up/down = list"),23);statusPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);
  log("{\"event\":\"hand_status_texture\",\"page\":%d,\"items\":%u,\"width\":720,\"height\":480,\"gpu_verified\":true}",page,unsigned(rows.size()));
 }
 if(startup.mode==3){auto pose=hpvr::healthPlane(copyMatrix(vrInput.leftGrip.pose.mDeviceToAbsoluteTracking),copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking));pose.m[1][3]-=.20f;statusOverlay.width=.32f;statusOverlay.submit(runtime,statusPanel.texture.textures[0],720,480,false,false,&pose);}
}
static void closeUi(){closePresentation();menuHint.close();menuHintPanel.close();bindingPanel.close();statusOverlay.close();statusPanel.close();handRows.clear();previousStatus.clear();statusAutoUntil=healthAutoUntil=0;bindingDismissedPage=nullptr;bindingPanelActive=bindingPanelConsumed=menuPointerConsumed=false;}
