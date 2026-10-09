#define INTERACTION_NO_OVERLAY
#include "free_camera_raw_input.h"
#include <cstdio>
#include <cstdlib>
int main(){
 auto check=[](bool value,const char* message){if(!value){std::puts(message);std::exit(1);}};
 using namespace FreeCameraInput;Clear();enabled=true;
 RAWINPUT input{};input.header.dwType=RIM_TYPEKEYBOARD;input.header.dwSize=offsetof(RAWINPUT,data)+sizeof(RAWKEYBOARD);input.data.keyboard.VKey='W';input.data.keyboard.Message=WM_KEYDOWN;
 check(ProcessRaw(input,sizeof(input),true,true,100),"keyboard packet rejected");check(keys['W']&&(input.data.keyboard.Flags&RI_KEY_BREAK)&&input.data.keyboard.Message==WM_KEYUP,"camera captures movement while game receives release");
 ProcessRaw(input,sizeof(input),true,true,101);check(!keys['W'],"release clears movement");
 input={};input.header.dwType=RIM_TYPEMOUSE;input.header.dwSize=sizeof(input);input.data.mouse.usButtonFlags=RI_MOUSE_RIGHT_BUTTON_DOWN;input.data.mouse.lLastX=12;input.data.mouse.lLastY=-4;
 check(ProcessRaw(input,sizeof(input),true,true,102)&&mouseX==12&&mouseY==-4&&input.data.mouse.lLastX==0&&input.data.mouse.usButtonFlags==0,"camera mouse captured before game input erased");
 check(!ProcessRaw(input,sizeof(RAWINPUTHEADER),true,true,103),"truncated packets must be rejected");
 Clear();hotkey='X';bool previous=false;bool down=true;auto read=[&](int key){return SHORT(down&&(key=='X'||key=='W')?0x8000:0);};
 toggleRequests=0;Poll(read,true,200,previous);check(toggleRequests==1&&keys['W'],"polling toggles shortcut and captures WASD without low-level hook");Poll(read,true,210,previous);check(toggleRequests==1,"held shortcut repeats");down=false;Poll(read,true,220,previous);down=true;Poll(read,true,230,previous);check(toggleRequests==2,"released shortcut rearms");
 down=false;Poll(read,true,240,previous);down=true;Poll(read,false,250,previous);check(toggleRequests==2,"inactive window must not toggle");
 std::puts("PASS: raw keyboard/mouse suppression, bounds, independent shortcut polling");
}
