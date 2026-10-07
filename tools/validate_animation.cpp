#include "imported_animation.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc<2)return 2;
 for(int i=1;i<argc;++i){
  try{
   std::ifstream file(argv[i]);Presets::Json json;file>>json;
   ImportedAnimation::Clip clip;
   if(!clip.Read(json)){std::cerr<<"Invalid animation: "<<argv[i]<<"\n";return 1;}
   size_t keys=0;bool changed=false;
   for(auto& track:clip.tracks){keys+=track.keys.size();for(auto& key:track.keys)
    if(!PoseMath::SameRotation(key.rotation,track.keys.front().rotation))changed=true;}
   if(!changed){std::cerr<<"No rotational motion: "<<argv[i]<<"\n";return 1;}
   std::cout<<"VALID "<<clip.id<<" duration="<<clip.duration<<" tracks="<<clip.tracks.size()<<" keys="<<keys<<"\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
 }
 return 0;
}
