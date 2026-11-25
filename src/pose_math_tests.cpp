#include "pose_math.h"
#include "mouth_contact.h"
#include <cstdio>
#include <limits>
using namespace PoseMath;
int main(){
 int failures=0;
 auto check=[&](bool ok,const char* name){if(!ok){std::fprintf(stderr,"FAIL: %s\n",name);++failures;}};
 Quaternion identity{},offset=Yaw20();
 check(SameRotation(Multiply(identity,offset),offset),"identity -> expected 20 degree yaw");
 Quaternion negative{-offset.x,-offset.y,-offset.z,-offset.w};
 check(SameRotation(offset,negative),"opposite quaternion signs describe same pose");
 Quaternion observed=identity,base=identity,last{};bool wrote=false;
 for(int i=0;i<600;++i){base=AnimationBase(observed,last,base,wrote);last=Multiply(base,offset);Normalize(last);observed=last;wrote=true;}
 check(SameRotation(observed,offset),"600 non-animated frames cannot accumulate yaw");
 check(SameRotation(base,identity),"original base is retained for restore when animator is culled");
 Quaternion fresh{0.05f,0.02f,0.01f,0.9985f};Normalize(fresh);
 auto animation=AnimationBase(fresh,last,base,true);
 check(SameRotation(animation,fresh),"fresh animation wins over stale base");
 check(!SameRotation(fresh,last),"restore leaves a fresh animation pose untouched");
 Quaternion tinyUpdate=last;tinyUpdate.x+=0.0001f;Normalize(tinyUpdate);
 check(SameRotation(AnimationBase(tinyUpdate,last,base,true),tinyUpdate),"small real animation changes are preserved");
 Quaternion invalid{0,0,0,0};check(!Normalize(invalid),"zero quaternion rejected");
 invalid={std::numeric_limits<float>::quiet_NaN(),0,0,1};check(!Normalize(invalid),"NaN rejected");
 invalid={0,0,0,10};check(!Normalize(invalid),"invalid magnitude rejected");
 float yaw=0,pitch=0;
 check(LookAngles({0,0,2},yaw,pitch)&&yaw==0&&pitch==0,"front target is neutral");
 check(LookAngles({2,0,0},yaw,pitch)&&yaw==55,"right target clamps yaw");
 check(LookAngles({-2,0,0},yaw,pitch)&&yaw==-55,"left target clamps yaw");
 check(LookAngles({0,2,1},yaw,pitch)&&pitch==-20,"high target clamps pitch");
 check(!LookAngles({0,0,0},yaw,pitch),"coincident target rejected");
 check(LookAngles({0,0,1000},yaw,pitch)&&yaw==0,"distant target accepted");
 auto body=YawPitch(90,0);auto local=Rotate(Inverse(body),{2,0,0});
 check(LookAngles(local,yaw,pitch)&&std::abs(yaw)<0.001f,"world right is front of a rotated model");
 check(Smooth(0,55,1.0f/60)>0&&Smooth(0,55,1.0f/60)<55,"smooth turn has no instant jump");
 check(Smooth(10,-55,-1)==10,"invalid delta time cannot jump pose");
 check(std::abs(Smooth(Smooth(0,55,0.02f),55,0.02f)-Smooth(0,55,0.04f))<0.0001f,"smoothing independent of normal frame rate");
 Vector3 anchor{};Quaternion facing{},other{};
 check(StandingAnchors({0,0,0},{2,0,0},anchor,facing,other,body)&&SamePosition(anchor,{1.2f,0,0}),"standing anchor follows controlled facing");
 check(SameRotation(facing,YawPitch(90,0))&&SameRotation(other,YawPitch(270,0)),"standing facing directions opposite");
 check(StandingAnchors({0,0,0},{1,50,0},anchor,facing,other)&&SamePosition(anchor,{0,0,1.2f}),"partner floor height does not affect placement");
 check(StandingAnchors({0,0,0},{0,0,0},anchor,facing,other),"coincident pair can be placed");
 check(StandingAnchors({0,0,0},{1000,500,-1000},anchor,facing,other)&&SamePosition(anchor,{0,0,1.2f}),"distant partner placement is independent of idle location");
 check(SameRotation(Nlerp({},YawPitch(180,0),1),YawPitch(180,0)),"rotation blend endpoint");
 const MouthContact::Entrance mouth{{},{1,0,0},{0,1,0},{0,0,1}};
 std::vector<Vector3> surface;
 std::vector<size_t> triangles;
 std::vector<float> hits;
 std::array<std::vector<float>,3> sectionHits;
 for(int ring=0;ring<3;++ring){
  const float z=-.01f+.02f*ring,halfHeight=.01f+.01f*ring;
  surface.insert(surface.end(),{{-.020f,-halfHeight,z},{.020f,-halfHeight,z},
                                {.020f,halfHeight,z},{-.020f,halfHeight,z}});
  if(!ring)continue;
  for(size_t edge=0;edge<4;++edge){
   const auto a=(ring-1)*4+edge,b=(ring-1)*4+(edge+1)%4;
   triangles.insert(triangles.end(),{a,b,b+4,a,b+4,a+4});
  }
 }
 triangles.insert(triangles.end(),{0,2,1,0,3,2,8,9,10,8,10,11});
 const Vector3 lipBox{.008f,.0045f,.0015f};
 const MouthContact::Probe shaft{{0,0,.03f},{0,0,-.01f}};
 const auto frontSection=MouthContact::ShaftThickness(surface,triangles,shaft,sectionHits);
 check(std::abs(frontSection-.030f)<.00001f&&MouthContact::LipBoxContact(surface,triangles,lipBox,hits),
       "actual mesh encloses the lip box at the narrower section");
 auto tilted=surface;
 const Quaternion oblique{std::sin(.65f),0,0,std::cos(.65f)};
 for(auto& v:tilted)v=Rotate(oblique,v);
 const MouthContact::Probe tiltedShaft{Rotate(oblique,shaft.base),Rotate(oblique,shaft.tip)};
 check(std::abs(MouthContact::ShaftThickness(tilted,triangles,tiltedShaft,sectionHits)-frontSection)<.00001f,
       "an oblique face angle cannot turn shaft length into opening thickness");
 for(auto& v:surface)v.z-=.020f;
 const auto rearSection=MouthContact::ShaftThickness(surface,triangles,{{0,0,.01f},{0,0,-.03f}},sectionHits);
 check(std::abs(rearSection-.050f)<.00001f&&
       MouthContact::OpeningForThickness(rearSection)>MouthContact::OpeningForThickness(frontSection),
       "the thicker rear section requests a larger but bounded opening");
 for(auto& v:surface)v.x+=.040f;
 check(!MouthContact::LipBoxContact(surface,triangles,lipBox,hits),"the lip box excludes a nearby cheek pass");
 for(auto& v:surface){v.x-=.040f;v.z+=.033f;}
 check(!MouthContact::LipBoxContact(surface,triangles,lipBox,hits),"a tip 3 mm in front of the aperture cannot open the lips");
 for(auto& v:surface)v.z-=.002f;
 check(MouthContact::LipBoxContact(surface,triangles,lipBox,hits),"the tip surface starts contact at the lip box");
 for(auto& v:surface)v.z-=.10f;
 check(!MouthContact::LipBoxContact(surface,triangles,lipBox,hits),"a mesh entirely behind the lip box clears contact");
 Vector3 delta{};
 MouthContact::Probe probe{{.03f,-.04f,.16f},{.016f,.005f,.004f}};
 check(MouthContact::FollowTarget(mouth,probe,false,delta)==MouthContact::FollowGate::Accepted&&delta.x<0&&
       std::abs(delta.z)<.00001f,"aiming aligns laterally without supplying penetration depth");
 const auto corrected=MouthContact::ProjectToTipDepth(mouth,probe,{-.012f,-.019f,-.045f});
 check(std::abs(corrected.z-probe.tip.z)<.00001f&&
       std::abs(MouthContact::Length(MouthContact::Subtract(corrected,probe.base))-
                MouthContact::Length(MouthContact::Subtract(probe.tip,probe.base)))<.00001f,
       "smoothing correction preserves both depth and rigid length");
 probe.tip={.2f,0,.01f};
 check(MouthContact::FollowTarget(mouth,probe,true,delta)==MouthContact::FollowGate::Distance,"a distant mouth releases the follow");
 probe={{0,0,-.16f},{.016f,0,-.03f}};
 check(MouthContact::FollowTarget(mouth,probe,false,delta)==MouthContact::FollowGate::Behind,"rear approach does not acquire a follow");
 probe={{.025f,.02f,.17f},{.037f,0,.044f}};
 const auto length=MouthContact::Length(MouthContact::Subtract(probe.tip,probe.base));
 check(MouthContact::FollowTarget(mouth,probe,false,delta)==MouthContact::FollowGate::Accepted&&
       std::abs(MouthContact::Length(MouthContact::Subtract(MouthContact::Add(probe.tip,delta),probe.base))-length)<.00001f,
       "the tip aims at a distant target without extending the shaft");
 const Vector3 pivot{.04f,.14f,.12f},tip{.04f,.14f,.02f},edge{.06f,.14f,.02f};
 const auto rotation=MouthContact::AimRotation({0,0,-1},{0,-.5f,-1});
 const auto rotatedTip=MouthContact::RotateAround(pivot,rotation,tip);
 const auto rotatedEdge=MouthContact::RotateAround(pivot,rotation,edge);
 check(SamePosition(MouthContact::RotateAround(pivot,rotation,pivot),pivot)&&
       std::abs(MouthContact::Length(MouthContact::Subtract(rotatedTip,pivot))-.1f)<.00001f&&
       std::abs(MouthContact::Length(MouthContact::Subtract(rotatedEdge,rotatedTip))-.02f)<.00001f&&
       rotatedTip.y<tip.y,"rigid rotation holds the pivot, length and cross section");
 const MouthContact::Probe attachment{{0,0,0},{0,0,.12f}};
 const auto nearPalm=MouthContact::PalmAnchor({.02f,-.04f,.12f},{.02f,.04f,.12f});
 check(MouthContact::FollowPoint(attachment,nearPalm,false)==MouthContact::FollowGate::Accepted,
       "a reachable palm acquires a rigid attachment");
 const auto aimedTip=MouthContact::RotateAround(attachment.base,
  MouthContact::AimRotation(attachment.tip,nearPalm),attachment.tip);
 check(std::abs(MouthContact::Length(aimedTip)-.12f)<.00001f&&
       MouthContact::Length(MouthContact::Subtract(aimedTip,nearPalm))<.003f,
       "aiming at a hand preserves length and approaches the palm");
 check(MouthContact::FollowPoint(attachment,{.06f,0,.12f},false)==MouthContact::FollowGate::Distance&&
       MouthContact::FollowPoint(attachment,{.06f,0,.12f},true)==MouthContact::FollowGate::Accepted&&
       MouthContact::FollowPoint(attachment,{.095f,0,.12f},true)==MouthContact::FollowGate::Distance,
       "body attachment uses distinct acquisition and release distances");
 check(MouthContact::FollowPoint(attachment,{0,0,.16f},false)==MouthContact::FollowGate::Reach,
       "a close point outside the fixed radius cannot acquire an attachment");
 Vector3 pelvisAnchor{};
 check(MouthContact::PelvicAnchor({-.10f,0,0},{.10f,0,0},{0,.18f,0},pelvisAnchor)&&
       SamePosition(pelvisAnchor,{0,-.048f,.036f}),"pelvic anchor is relative to thighs and spine");
 check(MouthContact::PelvicAnchor({1.1f,2,3},{.9f,2,3},{1,2.18f,3},pelvisAnchor)&&
       SamePosition(pelvisAnchor,{1,1.952f,2.964f})&&
       !MouthContact::PelvicAnchor({},{},{},pelvisAnchor),"pelvic anchor follows body rotation and rejects invalid skeletons");
 const MouthContact::Probe pelvicProbe{{.03f,-.04f,.11f},{.03f,-.04f,.29f}};
 for(const auto offset:{Vector3{.025f,0,.08f},Vector3{.06f,0,0},Vector3{0,0,-.09f},Vector3{0,0,.18f},Vector3{}}){
  const auto target=MouthContact::Add(pelvicProbe.base,offset);
  const auto approach=MouthContact::FollowPelvis(pelvicProbe,target);
  const auto segment=MouthContact::Subtract(
   MouthContact::RotateAround(pelvicProbe.base,approach.rotation,pelvicProbe.tip),pelvicProbe.base);
  const auto atTargetDepth=MouthContact::Add(pelvicProbe.base,MouthContact::Scale(segment,approach.reach/.18f));
  check(approach.gate==MouthContact::FollowGate::Accepted&&SamePosition(atTargetDepth,target)&&
        std::abs(MouthContact::Length(segment)-.18f)<.00001f,
        "pelvic alignment passes through every reachable point with fixed length");
 }
 const auto extendedTarget=MouthContact::Add(pelvicProbe.base,{.12f,0,.18f});
 const auto extended=MouthContact::FollowPelvis(pelvicProbe,extendedTarget);
 const auto aimedSegment=MouthContact::Subtract(
  MouthContact::RotateAround(pelvicProbe.base,extended.rotation,pelvicProbe.tip),pelvicProbe.base);
 check(extended.gate==MouthContact::FollowGate::Accepted&&
       std::abs(MouthContact::Length(aimedSegment)-.18f)<.00001f&&
       SamePosition(MouthContact::Add(pelvicProbe.base,MouthContact::Scale(aimedSegment,extended.reach/.18f)),extendedTarget),
       "pelvic alignment aims beyond the mesh tip without stretching the mesh");
 const auto atLimit=MouthContact::Add(pelvicProbe.base,{0,0,.234f});
 check(MouthContact::FollowPelvis(pelvicProbe,atLimit).gate==MouthContact::FollowGate::Accepted,
       "pelvic alignment is active at 1.3 times mesh length");
 const auto tooFar=MouthContact::Add(pelvicProbe.base,{0,0,.2345f});
 check(MouthContact::FollowPelvis(pelvicProbe,tooFar).gate==MouthContact::FollowGate::Reach,
       "pelvic alignment releases beyond 1.3 times mesh length");
 check(MouthContact::FollowPelvis({pelvicProbe.base,pelvicProbe.base},pelvicProbe.tip).gate==MouthContact::FollowGate::Invalid,
       "zero-length geometry cannot acquire a pelvic anchor");
 check(MouthContact::JawAngle(.0275f,.03f,.07f)==0&&
       MouthContact::JawAngle(.0275f,.005f,.07f)>.2f&&MouthContact::JawAngle(.10f,0,.035f)<=.42f,
       "jaw only supplements a missing opening and stays within its rotation limit");
 check(MouthContact::JawAngle(MouthContact::OpeningForThickness(.057f),.002f,.06f)>
       MouthContact::JawAngle(MouthContact::OpeningForThickness(.045f),.002f,.06f),
       "the measured thick sections remain distinct in the jaw response");
 const Vector3 hinge{},lowerLip{0,0,.07f},lowerTooth{0,0,.05f};
 const PoseMath::Quaternion jawDelta{std::sin(.15f),0,0,std::cos(.15f)};
 const auto movedLip=MouthContact::RotateAround(hinge,jawDelta,lowerLip);
 const auto movedTooth=MouthContact::RotateAround(hinge,jawDelta,lowerTooth);
 check(movedLip.y<0&&movedTooth.y<0&&
       std::abs(MouthContact::Length(MouthContact::Subtract(movedLip,movedTooth))-.02f)<.00001f,
       "lower lip and tooth follow a common jaw hinge without changing their spacing");
 check(MouthContact::ToLocalDelta({0,0,2},{2,0,0},{0,2,0},{6,8,10},delta)&&
       SamePosition(delta,{5,3,4}),"world following round-trips through a rotated and scaled palette frame");
 if(!failures)std::puts("PASS: quaternion direction, 600-frame non-accumulation, live-animation preservation and invalid-input checks");
 return failures?1:0;
}
