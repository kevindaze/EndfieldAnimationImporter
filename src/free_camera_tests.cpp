#include "free_camera_math.h"
#include <cstdio>
#include <cstdlib>
int main(){
 FreeCamera::View view;auto require=[](bool ok){if(!ok){std::puts("free camera invariant failed");std::exit(1);}};
 require(view.Capture({1,2,3},PoseMath::YawPitch(90,30)));require(std::abs(view.yaw-90)<.001f&&std::abs(view.pitch-30)<.001f);
 view.Capture({},{});view.Step(.05f,0,0,1,0,0);require(std::abs(view.position.z-.15f)<.0001f);
 FreeCamera::View diagonal;diagonal.Step(.05f,1,1,1,0,0);require(std::abs(std::sqrt(diagonal.position.x*diagonal.position.x+diagonal.position.y*diagonal.position.y+diagonal.position.z*diagonal.position.z)-.15f)<.0001f);
 view.Step(0,1,1,1,10000,10000);require(view.pitch==89&&std::abs(view.yaw)<=180);
 auto previous=view.position;view.Step(NAN,1,1,1,0,0);require(PoseMath::SamePosition(previous,view.position));
 require(FreeCamera::ValidHotkey(0x78)&&!FreeCamera::ValidHotkey('W')&&!FreeCamera::ValidHotkey(0x60)&&!FreeCamera::ValidHotkey(27));
 FreeCamera::View normal,fast;normal.Step(.02f,0,0,1,0,0);fast.Step(.02f,0,0,1,0,0,2);require(std::abs(fast.position.z-normal.position.z*2)<.0001f);
 std::puts("PASS: camera orientation, normalized movement, bounded timestep, pitch limits and reserved shortcuts");
}
