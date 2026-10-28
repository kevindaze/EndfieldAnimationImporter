#pragma once
#include "character_names.h"
#include "preset_settings.h"
#include <set>
namespace OperatorRoster {
// Gender groups follow local NPC prefab metadata (1 = male, 2 = female).
inline bool Male(const std::string& id){static const std::set<std::string> male{"chr_0002_endminm","chr_0006_wolfgd","chr_0015_lifeng","chr_0018_dapan","chr_0020_meurs","chr_0023_antal","chr_0024_deepfin","chr_0029_pograni","chr_0033_camille"};return male.contains(id);}
inline bool Known(const std::string& id){return id!="chr_9000_endmin"&&CharacterNames::All().contains(id);}
struct Whitelist {
 std::set<std::string> disabled;
 bool Allows(const std::string& id)const{return Known(id)&&!disabled.contains(id);}
 void Toggle(const std::string& id){if(!Known(id))return;if(disabled.contains(id))disabled.erase(id);else disabled.insert(id);}
 bool Read(const Presets::Json& data){try{if(!data.is_array()||data.size()>128)return false;std::set<std::string> candidate;for(auto& item:data){auto key=item.get<std::string>();if(!Known(key))return false;candidate.insert(key);}disabled=std::move(candidate);return true;}catch(...){return false;}}
 Presets::Json Encode()const{return Presets::Json(disabled);}
};
}
