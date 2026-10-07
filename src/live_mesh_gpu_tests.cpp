#include "live_mesh.h"
#include <iostream>
int main(){using namespace LiveMesh;int fails=0;auto check=[&](bool ok,const char* name){if(!ok){std::cerr<<name<<"\n";++fails;}};
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 1;
 auto table=*reinterpret_cast<void***>(device);nextCreate=reinterpret_cast<CreateFn>(table[3]);
 ExternalMesh::Bytes bytes(24);float values[]{1,2,3,4,5,6};std::memcpy(bytes.data(),values,24);slots={{"fixture",bytes,bytes}};revision=1;enabled=true;
 D3D11_BUFFER_DESC desc{};desc.ByteWidth=24;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=bytes.data();ID3D11Buffer* buffer=nullptr;
 check(SUCCEEDED(Create(device,&desc,&init,&buffer))&&captures.size()==1,"exact default buffer captured");
 float changed[]{7,8,9,10,11,12};std::memcpy(slots[0].current.data(),changed,24);++revision;dirty=true;Upload(context);
 D3D11_BUFFER_DESC staging=desc;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Buffer* readback=nullptr;device->CreateBuffer(&staging,nullptr,&readback);context->CopyResource(readback,buffer);D3D11_MAPPED_SUBRESOURCE mapped{};
 check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"GPU readback map");if(mapped.pData){check(std::memcmp(mapped.pData,changed,24)==0,"live bytes applied on immediate context");context->Unmap(readback,0);}
 ID3D11Buffer* immutable=nullptr;desc.Usage=D3D11_USAGE_IMMUTABLE;check(SUCCEEDED(Create(device,&desc,&init,&immutable))&&captures.size()==1,"immutable buffer left unchanged and not captured");check(matches==2&&unsupported==1&&lastUsage==int(D3D11_USAGE_IMMUTABLE),"unsupported exact matches diagnosed");check(uploads==1&&renderCalls==1,"actual upload counted");
 ID3D11Buffer* unrelated=nullptr;desc.Usage=D3D11_USAGE_DEFAULT;init.pSysMem=changed;check(SUCCEEDED(Create(device,&desc,&init,&unrelated))&&captures.size()==1,"unrelated same-size buffer not captured");
 ID3D11Buffer* delayed=nullptr;check(SUCCEEDED(Create(device,&desc,nullptr,&delayed))&&captures.size()==1,"empty creation counted without false capture");
 auto contextTable=*reinterpret_cast<void***>(context);auto nextUpdate=reinterpret_cast<UpdateFn>(contextTable[48]);
 UpdateWith(nextUpdate,context,delayed,0,nullptr,bytes.data(),0,0);check(captures.size()==2&&fills==1,"deferred full initialization captured");
 auto before=uploads;Upload(context);context->CopyResource(readback,delayed);mapped={};
 check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"deferred buffer readback map");if(mapped.pData){check(std::memcmp(mapped.pData,changed,24)==0,"deferred capture receives current live revision");context->Unmap(readback,0);}check(uploads>before,"deferred capture actually uploaded");
 // Separate relays forward to their own implementation without sharing next pointers.
 relays[2].create=nextCreate;ID3D11Buffer* otherRelay=nullptr;check(SUCCEEDED(EntryPoints<2>::Create(device,&desc,&init,&otherRelay)),"per-adapter next relay forwards");otherRelay->Release();delayed->Release();
 // Resource created outside our CreateBuffer hook: discover it from an actual SRV.
 desc.BindFlags=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_SHADER_RESOURCE;init.pSysMem=bytes.data();ID3D11Buffer* bound=nullptr;device->CreateBuffer(&desc,&init,&bound);
 D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};viewDesc.Format=DXGI_FORMAT_R32_FLOAT;viewDesc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;viewDesc.Buffer.NumElements=6;ID3D11ShaderResourceView* view=nullptr;
 check(SUCCEEDED(device->CreateShaderResourceView(bound,&viewDesc,&view)),"bound SRV fixture");auto prior=captures.size();ConsiderViews(1,&view);check(candidates.size()==1&&captures.size()==prior,"binding only schedules content verification");
 for(int attempt=0;attempt<100&&pendingReads;++attempt){Upload(context);context->Flush();Sleep(1);}check(captures.size()==prior+1&&readbacks==1,"bound resource captured after full GPU content verification");
 Upload(context);context->CopyResource(readback,bound);mapped={};check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"bound resource readback");if(mapped.pData){check(std::memcmp(mapped.pData,changed,24)==0,"bound capture receives live update");context->Unmap(readback,0);}
 Consider(unrelated);for(int attempt=0;attempt<100&&pendingReads;++attempt){Upload(context);context->Flush();Sleep(1);}check(captures.size()==prior+1,"same-size unrelated GPU content rejected");
 auto oldReads=readbacks;Consider(unrelated);check(!pendingReads&&readbacks==oldReads,"failed candidate is not re-read every frame");view->Release();bound->Release();
 // Discover a buffer on the deferred binding path, then read/update after execution.
 ID3D11DeviceContext* deferred=nullptr;check(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"deferred context fixture");auto deferredTable=*reinterpret_cast<void***>(deferred);relays[3].vertices=reinterpret_cast<BindVerticesFn>(deferredTable[18]);relays[3].execute=reinterpret_cast<ExecuteFn>(contextTable[58]);
 ID3D11Buffer* deferredBound=nullptr;device->CreateBuffer(&desc,&init,&deferredBound);auto beforeCapture=captures.size();UINT stride=12,offset=0;
 EntryPoints<3>::Vertices(deferred,0,1,&deferredBound,&stride,&offset);check(pendingReads,"deferred binding schedules candidate");auto beforeRead=readbacks;Upload(deferred);check(readbacks==beforeRead,"no staging readback on deferred context");
 ID3D11CommandList* list=nullptr;check(SUCCEEDED(deferred->FinishCommandList(FALSE,&list)),"finish deferred command list");EntryPoints<3>::Execute(context,list,FALSE);list->Release();
 for(int attempt=0;attempt<100&&pendingReads;++attempt){Upload(context);context->Flush();Sleep(1);}check(captures.size()==beforeCapture+1&&executions==1,"deferred candidate matched after command list execution");Upload(context);
 context->CopyResource(readback,deferredBound);mapped={};check(SUCCEEDED(context->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"deferred live readback");if(mapped.pData){check(std::memcmp(mapped.pData,changed,24)==0,"deferred binding gets current live data");context->Unmap(readback,0);}deferredBound->Release();deferred->Release();
 Shutdown();check(captures.empty()&&slots.empty()&&!enabled,"shutdown releases captured refs");
 unrelated->Release();immutable->Release();readback->Release();buffer->Release();context->Release();device->Release();std::cout<<"Live mesh GPU readback tests complete\n";return fails?1:0;
}
