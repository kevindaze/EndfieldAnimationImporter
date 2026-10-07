#include "external_models.h"
#include <iostream>
int main(){using namespace ExternalModels;int fail=0;auto check=[&](bool ok,const char* label){if(!ok){++fail;std::cerr<<label<<"\n";}};
 Json profile={{"format","endfield-external-model"},{"version",1},{"parts",Json::array({{{"id","horn"},{"name","角"},{"character","chr_example"},{"backend","unity_transform"},{"bone","CustomHorn"}}})}};
 std::vector<Part> parts;check(Read(profile,parts)&&parts.size()==1&&parts[0].bone=="CustomHorn","generic transform descriptor");
 auto malformed=profile;malformed["parts"][0]["backend"]="execute_script";check(!Read(malformed,parts)&&parts[0].bone=="CustomHorn","unknown backend rejected atomically");
 auto shape=profile;shape["parts"][0]["backend"]="efmi_shapekey";shape["parts"][0]["source_file"]="mod.ini";shape["parts"][0]["parameters"]=Json::array({{{"id","shape"},{"name","形態"},{"binding","$shape"},{"min",0},{"max",1},{"default",.5}}});check(Read(shape,parts)&&parts[0].parameters.size()==1,"generic shape descriptor");
 malformed=shape;malformed["parts"][0]["parameters"][0]["default"]=2;check(!Read(malformed,parts),"out-of-range default rejected");malformed=profile;malformed["parts"].push_back(malformed["parts"][0]);check(!Read(malformed,parts),"duplicate part ids rejected");
 auto directory=std::filesystem::temp_directory_path()/"endfield-external-model-profile-tests";std::filesystem::create_directories(directory);auto file=directory/"profile.json";{std::ofstream out(file);out<<shape.dump();}std::vector<std::string> errors;auto loaded=Load({directory},errors);check(errors.empty()&&loaded.size()==1&&std::filesystem::u8path(loaded[0].source)==directory/"mod.ini","relative source resolved against descriptor directory");std::filesystem::remove(file);std::filesystem::remove(directory);
 return fail?1:0;}
