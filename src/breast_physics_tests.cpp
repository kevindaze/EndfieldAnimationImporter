#include "breast_physics.h"
#include "breast_defaults.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
int main(){int errors=0;auto check=[&](bool v,const char* name){if(!v){++errors;std::cerr<<name<<'\n';}};
 BreastPhysics::Spring s;float result=0;
 for(int i=0;i<120;++i)result=s.Step({}, {},1.f/60,2,.52f,.5f,.2f);
 check(result==0,"stationary chest must not invent oscillation");
 result=s.Step({0,.01f,0},{},1.f/60,2,.52f,.5f,.2f);
 check(std::abs(result)>0,"chest movement drives the spring");
 for(int i=0;i<600;++i){float dt=i%2?1.f/30:1.f/144;result=s.Step({0,.01f+std::sin(float(i))*.02f,0},{},dt,2,.52f,.5f,.2f);check(std::isfinite(result)&&std::abs(result)<=.5f,"variable frame rate stays finite and bounded");}
 for(int i=0;i<360;++i)result=s.Step({0,.01f,0},{},1.f/60,2,.52f,.5f,.2f,true);
 check(std::abs(result)<.0001f,"pause settles without new driving force");
 s.Step({0,.02f,0},{},1.f/60,2,.52f,.5f,.2f);
 check(s.Step({10,0,0},{},1.f/60,2,.52f,.5f,.2f)==0,"teleport resets instead of causing an impulse");
 s.Step({}, {},1.f/60,2,.52f,.5f,.2f);check(s.Step({},PoseMath::YawPitch(180,0),1.f/60,2,.52f,.5f,.2f)==0,"large rotation discontinuity resets");
 check(s.Step({}, {},1,2,.52f,.5f,.2f)==0&&!s.ready,"long stalled frame resets");
 check(s.Step({std::numeric_limits<float>::quiet_NaN(),0,0},{},.016f,2,.52f,.5f,.2f)==0&&!s.ready,"invalid input cannot propagate NaN");
 s.Reset();check(!s.ready&&s.angle==0&&s.velocity==0,"explicit seek or switch reset clears history");
 BreastPhysics::AnimationResponse animation;float maximum=0;
 for(int i=0;i<240;++i){float time=i/60.f;auto position=PoseMath::Vector3{0,.02f*std::sin(time*6.2831853f),0};float a=animation.Step(position,{},1.f/60,10,20,2.1f,1,.7f,false);maximum=std::max(maximum,std::abs(a));check(std::abs(a)<=20*.01745329252f,"imported animation respects calibrated maximum");}
 check(maximum>3*.01745329252f,"small animated chest movement produces a visible calibrated response");
 for(int i=0;i<420;++i)result=animation.Step({}, {},1.f/60,10,20,2.1f,1,.7f,true);
 check(std::abs(result)<.0001f,"paused animation response settles to zero");
 animation.Reset();maximum=0;for(int i=0;i<180;++i){float a=animation.Step({},PoseMath::YawPitch(i/60.f*30,0),1.f/60,10,20,2.1f,.5f,.7f,false);maximum=std::max(maximum,std::abs(a));}
 check(maximum>3*.01745329252f,"rotation without translation also drives imported-animation physics");
 animation.Reset();for(int i=0;i<180;++i)result=animation.Step({}, {},1.f/60,10,20,2.1f,.5f,.7f,false);
 check(result==0,"playing a static pose does not manufacture continuous shaking");
 auto profiles=nlohmann::json::parse(breastDefaultData);check(profiles.size()==20,"all upstream default characters are represented");
 BreastPhysics::DirectionalResponse directional;PoseMath::Vector3 angles{};float peak=0;
 for(int i=0;i<240;++i){angles=directional.Step({0,.04f*std::sin(i/60.f*6.2831853f),0},{},{0,0,.1f},1.f/60,1.8f,.52f,.35f,false);peak=std::max(peak,BreastPhysics::Length(angles));check(std::abs(angles.y)<.00001f&&std::abs(angles.z)<.00001f,"vertical acceleration drives pitch, never sideways yaw");}
 check(peak>.01f,"directional inertia responds to chest acceleration");
 auto chestFrame=PoseMath::YawPitch(90,0);auto boneWorld=PoseMath::Multiply(chestFrame,BreastPhysics::RotationVector({0,0,1.5707963f}));
 auto forwardInBone=PoseMath::Rotate(PoseMath::Inverse(boneWorld),PoseMath::Rotate(chestFrame,{0,0,1}));
 auto output=BreastPhysics::BoneOutput(boneWorld,boneWorld,chestFrame,{.1f,0,0});auto tip=PoseMath::Rotate(PoseMath::Inverse(chestFrame),PoseMath::Rotate(output,forwardInBone));
 check(std::abs(tip.x)<.00001f&&std::abs(tip.y)>.05f,"bone orientation conjugation preserves anatomical vertical response for a rotated bone and character");
 PoseMath::Quaternion reconstructed{};check(BreastPhysics::ChestFrame({},PoseMath::Rotate(chestFrame,{0,1,0}),PoseMath::Rotate(chestFrame,{-1,0,0}),PoseMath::Rotate(chestFrame,{1,0,0}),reconstructed),"chest frame reconstructs from anatomical joints");
 auto facing=PoseMath::Rotate(reconstructed,{0,0,1});check(std::abs(facing.x-1)<.0001f&&std::abs(facing.y)<.0001f,"chest frame matches anatomical forward direction");
 for(int i=0;i<420;++i)angles=directional.Step({}, {},{0,0,.1f},1.f/60,1.8f,.52f,.35f,true);
 check(BreastPhysics::Length(angles)<.0001f,"paused directional response settles");directional.Reset();
 for(int i=0;i<180;++i)angles=directional.Step({}, {},{0,0,.1f},1.f/60,1.8f,.52f,.35f,false);
 check(BreastPhysics::Length(angles)==0,"stationary directional physics does not invent periodic motion");
 BreastPhysics::MotionSamples trajectory;float peakAcceleration=0;double time=0;
 for(int i=0;i<360;++i){float dt=i%3==0?1.f/30:1.f/120;time+=dt;auto a=trajectory.Add({0,float(time*.2),0},dt);peakAcceleration=std::max(peakAcceleration,BreastPhysics::Length(a));}
 check(peakAcceleration<.01f,"constant velocity with uneven frame timing does not manufacture acceleration bursts");
 trajectory.Reset();peakAcceleration=0;time=0;for(int i=0;i<900;++i){float dt=i%2?1.f/45:1.f/90;time+=dt;auto a=trajectory.Add({0,.02f*float(std::sin(time*6.2831853)),0},dt);if(i>20)peakAcceleration=std::max(peakAcceleration,BreastPhysics::Length(a));}
 check(peakAcceleration>.5f&&peakAcceleration<1.5f,"stable periodic bobbing remains periodic and bounded under frame jitter");
 trajectory.Reset();for(int i=0;i<12;++i)trajectory.Add({},1.f/60);auto spike=trajectory.Add({0,.1f,0},1.f/60);check(BreastPhysics::Length(spike)==0,"an isolated position glitch resets the trajectory instead of injecting a violent impulse");
 BreastPhysics::DirectionalResponse inertiaOnly,withLag;float originalPeak=0,lagPeak=0;double directionCorrelation=0;auto tilted=PoseMath::YawPitch(0,23);
 for(int i=0;i<900;++i){float t=i/60.f;PoseMath::Vector3 p{0,.02f*std::sin(t*6.2831853f),0};auto original=inertiaOnly.Step(p,tilted,{0,0,.1f},1.f/60,1.8f,.52f,.35f,false);auto response=withLag.Step(p,tilted,{0,0,.1f},1.f/60,1.8f,.52f,.35f,false,.55f);if(i>180){originalPeak=std::max(originalPeak,BreastPhysics::Length(original));lagPeak=std::max(lagPeak,BreastPhysics::Length(response));auto delta=BreastPhysics::RotationVector(response);auto tip=PoseMath::Rotate(tilted,PoseMath::Rotate(delta,{0,0,.1f}));auto rest=PoseMath::Rotate(tilted,{0,0,.1f});directionCorrelation+=(tip.y-rest.y)*(.02f*6.2831853f*std::cos(t*6.2831853f));}check(BreastPhysics::Length(response)<=.35f,"velocity lag respects the character amplitude limit");}
 check(lagPeak>originalPeak*2,"continuous bobbing gets a stronger relative response with velocity lag");check(directionCorrelation<0,"23-degree tilted chest moves opposite vertical body velocity on average");
 for(int i=0;i<420;++i)angles=withLag.Step({},tilted,{0,0,.1f},1.f/60,1.8f,.52f,.35f,true,.55f);
 check(BreastPhysics::Length(angles)<.0001f,"velocity lag settles on pause");
 for(auto& [key,p]:profiles.items()){check(key.starts_with("chr_")&&p["frequency"].get<float>()>0&&p["damping"].get<float>()>0&&p["limit"].get<float>()>=0,"character calibration is valid");check(p["axis"]=="X"||p["axis"]=="Y"||p["axis"]=="Z","valid per-character bone axis");}
 std::cout<<(errors?"FAIL":"PASS")<<": breast physics response and discontinuities\n";return errors?1:0;
}
