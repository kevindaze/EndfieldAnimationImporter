#pragma once
#include "preset_settings.h"
namespace Workflow {
inline Presets::Json EntryJson(const Presets::Entry& p){return {{"enabled",true},{"name",p.name},{"controlled",p.controlled},{"target",p.target},{"animation",p.animation},{"support_mode",p.support},{"grip_values",p.grip.Encode()},{"bone_offsets",p.bones.Encode()}};}
inline bool ReadEntry(const Presets::Json& j,Presets::Entry& result){Presets::Bank bank;auto data=bank.Encode();data["slots"][0]=j;data["slots"][0]["enabled"]=true;if(!bank.Read(data))return false;result=bank.slots[0];return true;}
inline Presets::Json Export(const Presets::Entry& p){return {{"format","endfield-interaction-calibration"},{"version",1},{"setting",EntryJson(p)}};}
inline bool Import(const Presets::Json& j,Presets::Entry& p){try{return j.at("format")=="endfield-interaction-calibration"&&j.at("version")==1&&ReadEntry(j.at("setting"),p);}catch(...){return false;}}
struct State {Presets::Entry draft,prepared;bool ready=false;void Invalidate(){ready=false;}bool Prepare(){Presets::Entry p;if(!ReadEntry(EntryJson(draft),p))return false;prepared=p;ready=true;return true;}bool Apply(Presets::Bank& bank,int index)const{if(!ready||index<0||index>=9)return false;auto name=bank.slots[index].name;bank.slots[index]=prepared;bank.slots[index].enabled=true;bank.slots[index].name=name;return true;}};
}
