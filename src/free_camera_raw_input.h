#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <atomic>
#include <array>
#include <BetterEndfield/HookChain.h>
#include "overlay_panel.h"
#include "free_camera_math.h"
namespace FreeCameraInput {
inline std::atomic_bool blocked{false},enabled{false},ready{false},focusLost{false},toggleHeld{false};
inline std::atomic<int> hotkey{VK_F9},toggleRequests{0},mouseX{0},mouseY{0};
inline std::array<std::atomic_bool,256> keys{},rawKeys{};
inline std::array<std::atomic<ULONGLONG>,256> rawAt{};
inline std::atomic<ULONGLONG> keyboardPackets{0},mousePackets{0},lastMouse{0},filteredPackets{0};
inline HANDLE worker=nullptr,started=nullptr;inline DWORD workerId=0;
inline const BE_HookChainApiV1* api=nullptr;inline uint64_t dataHook=0,bufferHook=0;
using DataFn=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
using BufferFn=UINT(WINAPI*)(PRAWINPUT,PUINT,UINT);
inline DataFn nextData=nullptr;inline BufferFn nextBuffer=nullptr;
inline bool Focused(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
inline bool OwnInput(){return Focused()&&!InteractionPanel::Editing();}
inline void ClearMotion(){mouseX=mouseY=0;}
inline void Clear(){for(auto& key:keys)key=false;for(auto& key:rawKeys)key=false;for(auto& at:rawAt)at=0;ClearMotion();toggleHeld=false;}
inline bool AllReleased(){for(int i=1;i<256;++i)if(i!=VK_NUMPAD0&&i!=VK_SHIFT&&i!=VK_CONTROL&&i!=VK_MENU&&keys[i])return false;return true;}
inline int NormalizeKey(int key,USHORT code,USHORT flags){if(key==VK_SHIFT)return code==0x36?VK_RSHIFT:VK_LSHIFT;if(key==VK_CONTROL)return flags&RI_KEY_E0?VK_RCONTROL:VK_LCONTROL;if(key==VK_MENU)return flags&RI_KEY_E0?VK_RMENU:VK_LMENU;return key;}
inline void RawButton(int key,bool down,ULONGLONG now){rawKeys[key]=down;keys[key]=down;rawAt[key]=now;}
// Observe before clearing the game's copy; do not change device registrations.
inline bool ProcessRaw(RAWINPUT& input,size_t available,bool own,bool suppress,ULONGLONG now){
 if(available<sizeof(RAWINPUTHEADER)||input.header.dwSize>available||input.header.dwSize<sizeof(RAWINPUTHEADER))return false;
 if(input.header.dwType==RIM_TYPEKEYBOARD){
  if(input.header.dwSize<offsetof(RAWINPUT,data)+sizeof(RAWKEYBOARD))return false;
  auto& key=input.data.keyboard;int vk=NormalizeKey(key.VKey,key.MakeCode,key.Flags);bool down=(key.Flags&RI_KEY_BREAK)==0;
  if(vk>0&&vk<255)RawButton(vk,down,now);++keyboardPackets;
  if(suppress){key.Flags|=RI_KEY_BREAK;key.Message=key.Message==WM_SYSKEYDOWN||key.Message==WM_SYSKEYUP?WM_SYSKEYUP:WM_KEYUP;++filteredPackets;}
  return true;
 }
 if(input.header.dwType==RIM_TYPEMOUSE){
  if(input.header.dwSize<offsetof(RAWINPUT,data)+sizeof(RAWMOUSE))return false;
  auto& mouse=input.data.mouse;USHORT flags=mouse.usButtonFlags;
  for(auto item:{std::array<int,3>{VK_LBUTTON,RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_LEFT_BUTTON_UP},{VK_RBUTTON,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_UP},{VK_MBUTTON,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_UP},{VK_XBUTTON1,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_4_UP},{VK_XBUTTON2,RI_MOUSE_BUTTON_5_DOWN,RI_MOUSE_BUTTON_5_UP}}){if(flags&item[1])RawButton(item[0],true,now);if(flags&item[2])RawButton(item[0],false,now);}
  ++mousePackets;lastMouse=now;
  if(own&&enabled&&keys[VK_RBUTTON]&&!(mouse.usFlags&MOUSE_MOVE_ABSOLUTE)){mouseX+=mouse.lLastX;mouseY+=mouse.lLastY;}
  if(suppress){mouse.lLastX=mouse.lLastY=0;mouse.usButtonFlags=mouse.usButtonData=0;mouse.ulRawButtons=0;++filteredPackets;}
  return true;
 }
 return false;
}
inline UINT WINAPI Data(HRAWINPUT handle,UINT command,LPVOID data,PUINT size,UINT header){
 UINT capacity=size?*size:0;UINT result=nextData?nextData(handle,command,data,size,header):UINT(-1);
 if(command==RID_INPUT&&data&&header==sizeof(RAWINPUTHEADER)&&result!=UINT(-1)&&result<=capacity&&result>=sizeof(RAWINPUTHEADER))ProcessRaw(*static_cast<RAWINPUT*>(data),result,OwnInput(),blocked||toggleHeld||toggleRequests.load()>0,GetTickCount64());
 return result;
}
inline UINT WINAPI Buffer(PRAWINPUT data,PUINT size,UINT header){
 UINT capacity=size?*size:0;UINT count=nextBuffer?nextBuffer(data,size,header):UINT(-1);
 if(data&&header==sizeof(RAWINPUTHEADER)&&count!=UINT(-1)){
  size_t offset=0;bool own=OwnInput(),suppress=blocked||toggleHeld||toggleRequests.load()>0;auto now=GetTickCount64();
  for(UINT i=0;i<count&&offset+sizeof(RAWINPUTHEADER)<=capacity;++i){auto& input=*reinterpret_cast<RAWINPUT*>(reinterpret_cast<BYTE*>(data)+offset);size_t bytes=input.header.dwSize;if(bytes<sizeof(RAWINPUTHEADER)||bytes>capacity-offset)break;ProcessRaw(input,bytes,own,suppress,now);offset+=(bytes+sizeof(void*)-1)&~(sizeof(void*)-1);}
 }
 return count;
}
inline bool Install(const BE_HookChainApiV1* hooks,const char* id){
 if(ready)return true;if(!hooks||!hooks->create||!hooks->disable)return false;api=hooks;auto module=GetModuleHandleW(L"user32.dll");
 auto data=module?GetProcAddress(module,"GetRawInputData"):nullptr;auto buffer=module?GetProcAddress(module,"GetRawInputBuffer"):nullptr;
 if(!data||!buffer)return false;
 if(hooks->create(hooks->context,id,reinterpret_cast<void*>(data),reinterpret_cast<void*>(&Data),reinterpret_cast<void**>(&nextData),&dataHook)!=BE_Result_Ok)return false;
 if(hooks->create(hooks->context,id,reinterpret_cast<void*>(buffer),reinterpret_cast<void*>(&Buffer),reinterpret_cast<void**>(&nextBuffer),&bufferHook)!=BE_Result_Ok){hooks->disable(hooks->context,dataHook);dataHook=0;return false;}
 ready=true;return true;
}
// Keyboard polling is independent of raw delivery. No WH_KEYBOARD_LL dependency.
template<class Read> inline void Poll(Read read,bool own,ULONGLONG now,bool& previous){
 for(int key=1;key<256;++key){bool down=(read(key)&0x8000)!=0;auto at=rawAt[key].load();keys[key]=at&&now-at<250?rawKeys[key].load():down;}
 bool down=(read(hotkey.load())&0x8000)!=0;toggleHeld=down&&own;if(down&&!previous&&own)++toggleRequests;previous=down;
}
inline DWORD WINAPI Run(void*){
 MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);auto timer=SetTimer(nullptr,0,10,nullptr);bool previous=true;POINT last{};GetCursorPos(&last);if(started)SetEvent(started);
 while(GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_TIMER){auto now=GetTickCount64();bool own=OwnInput();Poll([](int key){return GetAsyncKeyState(key);},own,now,previous);if(enabled&&!Focused()){focusLost=true;ClearMotion();}POINT point{};GetCursorPos(&point);if(own&&enabled&&keys[VK_RBUTTON]&&now-lastMouse.load()>250){mouseX+=point.x-last.x;mouseY+=point.y-last.y;}last=point;}else{TranslateMessage(&message);DispatchMessageW(&message);}}
 if(timer)KillTimer(nullptr,timer);return 0;
}
inline void Start(){if(!worker){started=CreateEventW(nullptr,TRUE,FALSE,nullptr);worker=CreateThread(nullptr,0,Run,nullptr,0,&workerId);if(worker&&started)WaitForSingleObject(started,2000);}}
inline void Stop(){enabled=blocked=false;if(worker){if(started)WaitForSingleObject(started,2000);PostThreadMessageW(workerId,WM_QUIT,0,0);WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=nullptr;workerId=0;}if(started){CloseHandle(started);started=nullptr;}if(api){if(dataHook)api->disable(api->context,dataHook);if(bufferHook)api->disable(api->context,bufferHook);}api=nullptr;dataHook=bufferHook=0;ready=false;Clear();}
}
