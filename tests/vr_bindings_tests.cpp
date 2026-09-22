#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include "vr_bindings.h"
#include "vr_presentation.h"
static void require(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Fixture root required");VrBindings b;b.load(argv[1]);for(int i=0;i<b.Count;++i)require(b.source[i]==i,"Defaults");
 b.assign(VrBindings::Jump,VrBindings::Cast);require(b.source[0]==1&&b.source[1]==0,"Conflict swap");
 b.swapSticks=true;b.save();VrBindings c;c.load(argv[1]);require(c.source[0]==1&&c.source[1]==0&&c.swapSticks,"Persisted remap");
 c.assign(VrBindings::Status,VrBindings::Menu);require(c.source[9]==2&&c.source[2]==9,"Status/menu swap");
 bool used[c.Count]={};for(int i=0;i<c.Count;++i){require(!used[c.source[i]],"Duplicate mapping");used[c.source[i]]=true;}
 WritePrivateProfileStringW(L"Bindings",L"Action0",L"99",c.path.c_str());VrBindings d;d.load(argv[1]);for(int i=0;i<d.Count;++i)require(d.source[i]==i,"Malformed configuration fallback");d.defaults();d.save();
 VrPresentation p;p.load(argv[1]);require(!p.spatialScenes,"Initial cinematic default");p.setSpatial(true);VrPresentation q;q.load(argv[1]);require(q.spatialScenes,"3D cinematic option persisted");q.setSpatial(false);p.load(argv[1]);require(!p.spatialScenes,"Cinema screen option persisted");WritePrivateProfileStringW(L"Presentation",L"SpatialScenes",L"garbage",p.path.c_str());p.load(argv[1]);require(!p.spatialScenes,"Invalid option safe fallback");p.setSpatial(false);
 std::puts("VR bindings: conflict swaps, hand-status remap, persistence and invalid-config recovery passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
