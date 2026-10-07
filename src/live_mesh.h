#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "external_mesh.h"
#include <BetterEndfield/ThirdPartyModule.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <optional>
#ifndef INTERACTION_NO_OVERLAY
#include <d3d11.h>
#include <Windows.h>
#endif
namespace LiveMesh {
#ifndef INTERACTION_NO_OVERLAY
using ExecuteFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11CommandList*,BOOL);
using BindViewsFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*);
using BindVerticesFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer* const*,const UINT*,const UINT*);
using UpdateFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
using CopyFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
using CreateFn=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const D3D11_BUFFER_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer**);
using DrawFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
using InstancedFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
using DispatchFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);
inline CreateFn nextCreate=nullptr;inline DrawFn nextDraw=nullptr;inline InstancedFn nextInstanced=nullptr;inline DispatchFn nextDispatch=nullptr;
struct Slot {std::string source;ExternalMesh::Bytes initial,current;};
struct Capture {ID3D11Buffer* buffer;ID3D11Device* device;size_t slot;uint64_t revision;};
struct Candidate {ID3D11Buffer* buffer;ID3D11Buffer* staging{};ID3D11Device* device{};D3D11_BUFFER_DESC desc;};
inline std::vector<Candidate> candidates;inline std::vector<ID3D11Buffer*> examined;inline size_t examinedBytes=0;inline std::atomic_bool pendingReads=false;inline uint64_t readbacks=0,sizeCandidates=0;inline std::atomic_uint64_t binds=0,executions=0;
inline std::mutex mutex;inline std::condition_variable cv;inline std::vector<Slot> slots;inline std::vector<Capture> captures;
inline std::atomic_bool enabled=false,dirty=false;inline std::string detail="即時網格未啟用";
inline std::vector<uint64_t> handles;inline const BE_HookChainApiV1* hooks=nullptr;
inline thread_local bool internalUpload=false;
inline uint64_t creates=0,fills=0,matches=0,unsupported=0,uploads=0;inline std::atomic_uint64_t renderCalls=0;inline size_t adapters=0;inline int lastUsage=-1;
inline uint64_t revision=0,epoch=0;inline std::optional<EfmiBridge::Scan> request;inline bool quit=false;inline std::thread worker;
inline void ReleaseCandidates(){for(auto& c:candidates){if(c.staging)c.staging->Release();c.buffer->Release();}candidates.clear();for(auto* b:examined)b->Release();examined.clear();examinedBytes=0;pendingReads=false;}
inline void ReleaseCaptures(){for(auto& c:captures)c.buffer->Release();captures.clear();}
inline void ObserveLocked(ID3D11Buffer* buffer,const D3D11_BUFFER_DESC& desc,const void* data){
 if(!data||!enabled||std::any_of(captures.begin(),captures.end(),[&](auto& c){return c.buffer==buffer;}))return;
 size_t found=slots.size();for(size_t i=0;i<slots.size();++i)if(desc.ByteWidth==slots[i].initial.size()&&std::memcmp(data,slots[i].initial.data(),desc.ByteWidth)==0){if(found!=slots.size()){detail="Buffer 內容重複，無法唯一辨識；請使用重載模式";return;}found=i;}
 if(found==slots.size())return;++matches;lastUsage=int(desc.Usage);if(desc.Usage!=D3D11_USAGE_DEFAULT||desc.CPUAccessFlags){++unsupported;detail="資料已匹配，但 Buffer 不支援直接更新";return;}if(captures.size()>=64){detail="GPU 捕獲已達上限；請重新寫入橋接";return;}
 ID3D11Device* device=nullptr;buffer->GetDevice(&device);if(!device)return;buffer->AddRef();captures.push_back({buffer,device,found,0});device->Release();dirty=true;detail="已接通 GPU Buffer；即時更新已啟用（僅網格）";
}
inline void Observe(ID3D11Buffer* b,const D3D11_BUFFER_DESC& d,const void* data){if(!enabled||!data)return;std::lock_guard lock(mutex);ObserveLocked(b,d,data);}
inline void Consider(ID3D11Resource* resource){
 if(internalUpload||!enabled||!resource)return;D3D11_RESOURCE_DIMENSION type;resource->GetType(&type);if(type!=D3D11_RESOURCE_DIMENSION_BUFFER)return;
 auto b=static_cast<ID3D11Buffer*>(resource);D3D11_BUFFER_DESC d{};b->GetDesc(&d);std::lock_guard lock(mutex);
 if(!enabled||examined.size()>=64||std::find(examined.begin(),examined.end(),b)!=examined.end()||std::any_of(captures.begin(),captures.end(),[&](auto& c){return c.buffer==b;}))return;
 if(!std::any_of(slots.begin(),slots.end(),[&](auto& s){return s.initial.size()==d.ByteWidth;}))return;
 if(examinedBytes+d.ByteWidth>134217728){detail="待比對 GPU 資料超過 128 MB，請分批調整";return;}
 ++sizeCandidates;b->AddRef();examined.push_back(b);examinedBytes+=d.ByteWidth;b->AddRef();candidates.push_back({b,nullptr,nullptr,d});pendingReads=true;
}
inline void ConsiderViews(UINT count,ID3D11ShaderResourceView* const* views){if(!enabled||!views)return;++binds;for(UINT i=0;i<count;++i)if(views[i]){D3D11_SHADER_RESOURCE_VIEW_DESC desc{};views[i]->GetDesc(&desc);if(desc.ViewDimension!=D3D11_SRV_DIMENSION_BUFFER&&desc.ViewDimension!=D3D11_SRV_DIMENSION_BUFFEREX)continue;ID3D11Resource* r=nullptr;views[i]->GetResource(&r);if(r){Consider(r);r->Release();}}}
inline void ReadCandidates(ID3D11DeviceContext* context){
 if(!pendingReads||internalUpload||!enabled||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
 std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;ID3D11Device* device=nullptr;context->GetDevice(&device);if(!device)return;
 internalUpload=true;
 for(size_t i=0;i<candidates.size();){auto& c=candidates[i];if(!c.device){c.buffer->GetDevice(&c.device);if(c.device)c.device->Release();}
 if(c.device!=device){++i;continue;}bool done=false;
 if(!c.staging){auto d=c.desc;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;d.StructureByteStride=0;
 if(FAILED(device->CreateBuffer(&d,nullptr,&c.staging)))done=true;else context->CopyResource(c.staging,c.buffer);}
 if(!done){D3D11_MAPPED_SUBRESOURCE map{};auto result=context->Map(c.staging,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
 if(SUCCEEDED(result)){++readbacks;ObserveLocked(c.buffer,c.desc,map.pData);context->Unmap(c.staging,0);done=true;}else if(result!=DXGI_ERROR_WAS_STILL_DRAWING)done=true;}
 if(done){if(c.staging)c.staging->Release();c.buffer->Release();candidates.erase(candidates.begin()+i);}else ++i;}
 internalUpload=false;device->Release();pendingReads=!candidates.empty();
}
inline HRESULT CreateWith(CreateFn next,ID3D11Device* device,const D3D11_BUFFER_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Buffer** out){
 auto result=next(device,desc,data,out);if(internalUpload||!enabled||FAILED(result)||!desc||!out||!*out)return result;
 {std::lock_guard lock(mutex);++creates;}if(data&&data->pSysMem)Observe(*out,*desc,data->pSysMem);return result;
}
inline HRESULT STDMETHODCALLTYPE Create(ID3D11Device* d,const D3D11_BUFFER_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Buffer** out){return CreateWith(nextCreate,d,desc,data,out);}
inline void UpdateWith(UpdateFn next,ID3D11DeviceContext* c,ID3D11Resource* resource,UINT sub,const D3D11_BOX* box,const void* data,UINT row,UINT depth){
 next(c,resource,sub,box,data,row,depth);if(internalUpload||!enabled||!resource||sub||box||!data)return;
 {std::lock_guard lock(mutex);++fills;}D3D11_RESOURCE_DIMENSION type;resource->GetType(&type);if(type!=D3D11_RESOURCE_DIMENSION_BUFFER)return;
 auto buffer=static_cast<ID3D11Buffer*>(resource);D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);Observe(buffer,desc,data);
}
inline void Upload(ID3D11DeviceContext* context){
 if(internalUpload||!enabled||!context)return;++renderCalls;ReadCandidates(context);if(!dirty.load()||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
 std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
 ID3D11Device* device=nullptr;context->GetDevice(&device);if(!device)return;
 bool pending=false;for(auto& c:captures){if(c.revision==revision)continue;if(c.device!=device){pending=true;continue;}internalUpload=true;context->UpdateSubresource(c.buffer,0,nullptr,slots[c.slot].current.data(),0,0);internalUpload=false;c.revision=revision;++uploads;}
 device->Release();dirty=pending;
}
inline void STDMETHODCALLTYPE Draw(ID3D11DeviceContext* c,UINT n,UINT s,INT b){Upload(c);nextDraw(c,n,s,b);}
inline void STDMETHODCALLTYPE Instanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT s,INT b,UINT first){Upload(c);nextInstanced(c,n,instances,s,b,first);}
inline void STDMETHODCALLTYPE Dispatch(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){Upload(c);nextDispatch(c,x,y,z);}
inline void Run(){std::unique_lock lock(mutex);while(!quit){cv.wait(lock,[]{return quit||request.has_value();});if(quit)break;
 // Coalesce slider changes without blocking the UI or render callbacks.
 cv.wait_for(lock,std::chrono::milliseconds(120),[]{return quit;});if(quit)break;
 auto scan=std::move(*request);request.reset();auto started=epoch;lock.unlock();
 try{auto buffers=ExternalMesh::Build(scan);lock.lock();if(enabled&&started==epoch){bool valid=!buffers.empty();for(auto& b:buffers){auto key=EfmiBridge::Utf8(b.source);auto slot=std::find_if(slots.begin(),slots.end(),[&](auto& s){return s.source==key&&s.initial.size()==b.bytes.size();});if(slot==slots.end()){valid=false;break;}}
 if(valid){for(auto& b:buffers){auto key=EfmiBridge::Utf8(b.source);auto slot=std::find_if(slots.begin(),slots.end(),[&](auto& s){return s.source==key;});slot->current=std::move(b.bytes);}++revision;dirty=true;detail=captures.empty()?"等待 GPU Buffer；請先寫入橋接並 F10 重載":"網格已排入 GPU 即時更新";}else detail="此網格尚未接通；請重新寫入橋接並 F10 重載";}}
 catch(const std::exception& e){if(!lock.owns_lock())lock.lock();detail=std::string("即時更新失敗：")+e.what();}
 }}
inline void Queue(const EfmiBridge::Scan& scan){if(!enabled)return;std::lock_guard lock(mutex);request=scan;cv.notify_one();}
inline void Arm(const std::vector<ExternalMesh::Buffer>& buffers){if(!enabled)return;std::lock_guard lock(mutex);size_t total=0;for(auto& b:buffers){total+=b.bytes.size();if(total>134217728)throw std::runtime_error("Live mesh session exceeds 128 MB; use reload mode");}
 ReleaseCandidates();ReleaseCaptures();slots.clear();request.reset();creates=fills=matches=unsupported=uploads=readbacks=sizeCandidates=0;renderCalls=binds=executions=0;lastUsage=-1;++epoch;++revision;for(auto& b:buffers)slots.push_back({EfmiBridge::Utf8(b.source),b.bytes,b.bytes});dirty=false;detail="已準備即時辨識；回遊戲 F10 重載一次";}
inline void Shutdown(){enabled=false;{std::lock_guard lock(mutex);quit=true;request.reset();cv.notify_all();}if(worker.joinable())worker.join();if(hooks)for(auto h:handles)hooks->disable(hooks->context,h);handles.clear();hooks=nullptr;std::lock_guard lock(mutex);ReleaseCandidates();ReleaseCaptures();slots.clear();dirty=false;detail="即時調整已停用；F10 回到已寫入網格";}
// Each distinct adapter implementation needs its own next relay; never reuse one
// next pointer for different driver addresses.
struct Relay {CreateFn create{};DrawFn draw{};InstancedFn instanced{};DispatchFn dispatch{};UpdateFn update{};CopyFn copy{};BindViewsFn vs{},cs{};BindVerticesFn vertices{};ExecuteFn execute{};};
inline Relay relays[16];
template<size_t I> struct EntryPoints {
 static HRESULT STDMETHODCALLTYPE Create(ID3D11Device* d,const D3D11_BUFFER_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Buffer** out){return CreateWith(relays[I].create,d,desc,data,out);}
 static void STDMETHODCALLTYPE Draw(ID3D11DeviceContext* c,UINT n,UINT s,INT b){Upload(c);relays[I].draw(c,n,s,b);}
 static void STDMETHODCALLTYPE Instanced(ID3D11DeviceContext* c,UINT n,UINT instances,UINT s,INT b,UINT first){Upload(c);relays[I].instanced(c,n,instances,s,b,first);}
 static void STDMETHODCALLTYPE Dispatch(ID3D11DeviceContext* c,UINT x,UINT y,UINT z){Upload(c);relays[I].dispatch(c,x,y,z);}
 static void STDMETHODCALLTYPE Update(ID3D11DeviceContext* c,ID3D11Resource* r,UINT sub,const D3D11_BOX* box,const void* data,UINT row,UINT depth){UpdateWith(relays[I].update,c,r,sub,box,data,row,depth);}
 static void STDMETHODCALLTYPE Execute(ID3D11DeviceContext* c,ID3D11CommandList* list,BOOL restore){if(enabled)++executions;relays[I].execute(c,list,restore);Upload(c);}
 static void STDMETHODCALLTYPE VS(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11ShaderResourceView* const* views){ConsiderViews(count,views);relays[I].vs(c,start,count,views);}
 static void STDMETHODCALLTYPE CS(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11ShaderResourceView* const* views){ConsiderViews(count,views);relays[I].cs(c,start,count,views);}
 static void STDMETHODCALLTYPE Vertices(ID3D11DeviceContext* c,UINT start,UINT count,ID3D11Buffer* const* buffers,const UINT* strides,const UINT* offsets){if(enabled&&buffers){++binds;for(UINT i=0;i<count;++i)Consider(buffers[i]);}relays[I].vertices(c,start,count,buffers,strides,offsets);}
 static void STDMETHODCALLTYPE Copy(ID3D11DeviceContext* c,ID3D11Resource* dst,ID3D11Resource* src){Consider(src);Upload(c);relays[I].copy(c,dst,src);}
};
struct Entry{void* target;void* detour;void** next;};
template<size_t I> inline bool Install(ID3D11Device* device,ID3D11DeviceContext* context,std::vector<void*>& seen){
 auto v=*reinterpret_cast<void***>(device),c=*reinterpret_cast<void***>(context);auto& r=relays[I];
 Entry entries[]{{v[3],reinterpret_cast<void*>(&EntryPoints<I>::Create),reinterpret_cast<void**>(&r.create)},
 {c[12],reinterpret_cast<void*>(&EntryPoints<I>::Draw),reinterpret_cast<void**>(&r.draw)},
 {c[20],reinterpret_cast<void*>(&EntryPoints<I>::Instanced),reinterpret_cast<void**>(&r.instanced)},
 {c[41],reinterpret_cast<void*>(&EntryPoints<I>::Dispatch),reinterpret_cast<void**>(&r.dispatch)},
 {c[48],reinterpret_cast<void*>(&EntryPoints<I>::Update),reinterpret_cast<void**>(&r.update)},
 {c[47],reinterpret_cast<void*>(&EntryPoints<I>::Copy),reinterpret_cast<void**>(&r.copy)},
 {c[25],reinterpret_cast<void*>(&EntryPoints<I>::VS),reinterpret_cast<void**>(&r.vs)},
 {c[67],reinterpret_cast<void*>(&EntryPoints<I>::CS),reinterpret_cast<void**>(&r.cs)},
 {c[18],reinterpret_cast<void*>(&EntryPoints<I>::Vertices),reinterpret_cast<void**>(&r.vertices)},
 {c[58],reinterpret_cast<void*>(&EntryPoints<I>::Execute),reinterpret_cast<void**>(&r.execute)}};
 for(auto& e:entries){if(std::find(seen.begin(),seen.end(),e.target)!=seen.end())continue;uint64_t h=0;
 if(hooks->create(hooks->context,"local.endfield.interaction",e.target,e.detour,e.next,&h)!=BE_Result_Ok)return false;handles.push_back(h);seen.push_back(e.target);}return true;
}
inline bool InstallAt(size_t index,ID3D11Device* d,ID3D11DeviceContext* c,std::vector<void*>& seen){
 switch(index){case 0:return Install<0>(d,c,seen);case 1:return Install<1>(d,c,seen);case 2:return Install<2>(d,c,seen);case 3:return Install<3>(d,c,seen);case 4:return Install<4>(d,c,seen);case 5:return Install<5>(d,c,seen);case 6:return Install<6>(d,c,seen);case 7:return Install<7>(d,c,seen);case 8:return Install<8>(d,c,seen);case 9:return Install<9>(d,c,seen);case 10:return Install<10>(d,c,seen);case 11:return Install<11>(d,c,seen);case 12:return Install<12>(d,c,seen);case 13:return Install<13>(d,c,seen);case 14:return Install<14>(d,c,seen);case 15:return Install<15>(d,c,seen);default:return false;}
}
inline bool Enable(const BE_HookChainApiV1* api){if(enabled)return true;Shutdown();if(!api||api->version!=1||api->struct_size<sizeof(*api)||!api->create||!api->disable)return false;
 wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);auto base=std::wstring(system)+L"\\";
 auto library=LoadLibraryExW((base+L"d3d11.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 auto dxgi=LoadLibraryExW((base+L"dxgi.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library||!dxgi)return false;
 auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(library,"D3D11CreateDevice"));
 auto factoryFn=reinterpret_cast<decltype(&CreateDXGIFactory1)>(GetProcAddress(dxgi,"CreateDXGIFactory1"));if(!create||!factoryFn)return false;
 IDXGIFactory1* factory=nullptr;if(FAILED(factoryFn(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory))))return false;
 hooks=api;adapters=0;std::vector<void*> seen;bool ok=true;
 for(UINT i=0;adapters<8;++i){IDXGIAdapter1* adapter=nullptr;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;if(!adapter)break;
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
 auto result=create(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);adapter->Release();
 if(FAILED(result))continue;ok=InstallAt(adapters*2,device,context,seen);ID3D11DeviceContext* deferred=nullptr;if(ok&&SUCCEEDED(device->CreateDeferredContext(0,&deferred))){ok=InstallAt(adapters*2+1,device,deferred,seen);deferred->Release();}++adapters;context->Release();device->Release();if(!ok)break;}
 factory->Release();if(!ok||!adapters){Shutdown();return false;}
 // Keep system libraries resident while shared-chain relays may reference them.
 quit=false;enabled=true;worker=std::thread(Run);{std::lock_guard lock(mutex);detail="即時入口已啟用；請寫入橋接並 F10 重載一次";}return true;
}
inline std::string Status(){std::lock_guard lock(mutex);return detail+" · 已辨識 "+std::to_string(captures.size())+" 個 GPU Buffer · 建立 "+std::to_string(creates)+"／填入 "+std::to_string(fills)+"／匹配 "+std::to_string(matches)+"／不支援 "+std::to_string(unsupported)+"／上傳 "+std::to_string(uploads)+"／更新回呼 "+std::to_string(renderCalls.load())+"／提交 "+std::to_string(executions.load())+"／綁定 "+std::to_string(binds.load())+"／尺寸候選 "+std::to_string(sizeCandidates)+"／讀回 "+std::to_string(readbacks)+" · 等待網格 "+std::to_string(slots.size())+" · 顯卡入口 "+std::to_string(adapters)+(lastUsage<0?"":" · Usage="+std::to_string(lastUsage));}
#else
inline bool enabled=false;inline bool Enable(const BE_HookChainApiV1*){return false;}inline void Shutdown(){}inline void Queue(const EfmiBridge::Scan&){}inline void Arm(const std::vector<ExternalMesh::Buffer>&){}inline std::string Status(){return "即時 GPU 測試替身";}
#endif
}
