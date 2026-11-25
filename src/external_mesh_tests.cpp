#include "live_external_mesh.h"
#include "efmi_scan_cache.h"
#include "contact_audio.h"
#include <iostream>
#include <chrono>
#include <cstring>
int main(int argc,char** argv){using namespace ExternalMesh;int fail=0;auto check=[&](bool ok,const char* name){if(!ok){++fail;std::cerr<<name<<"\n";}};
 Bytes wave(48);
 std::memcpy(wave.data(),"RIFF",4);
 Put<uint32_t>(wave,4,40);
 std::memcpy(wave.data()+8,"WAVEfmt ",8);
 Put<uint32_t>(wave,16,16);Put<uint16_t>(wave,20,1);Put<uint16_t>(wave,34,16);
 std::memcpy(wave.data()+36,"data",4);
 Put<uint32_t>(wave,40,4);Put<int16_t>(wave,44,-32000);Put<int16_t>(wave,46,16000);
 auto softer=ContactAudio::WithVolume(wave,25);
 check(Get<int16_t>(softer,44)==-8000&&Get<int16_t>(softer,46)==4000&&
       std::equal(wave.begin(),wave.begin()+44,softer.begin())&&ContactAudio::WithVolume(wave,100)==wave,
       "impact volume scales only the WAV samples and preserves full volume");
 auto dir=std::filesystem::current_path()/("bem-mesh-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);auto file=dir/"mod.ini";std::string source="namespace = TestMod\n[TextureOverrideTest]\nib = ResourceIndex\nvb0 = ResourcePosition\n; [mesh:horn]\ndrawindexed = 3,0,0\n[ResourceIndex]\nformat = R32_UINT\nfilename = Index.buf\n[ResourcePosition]\nstride = 16\nfilename = Position.buf\n[ResourcePosition_0]\nstride = 16\nfilename = Position.buf\n[ResourceDelta]\nstride = 12\nfilename = Position_merged_packed_pos_delta.buf\n[TextureOverrideSecond]\nib = ResourceIndex\nvb0 = ResourceSecond\n; [mesh:fin]\ndrawindexed = 3,0,0\n[ResourceSecond]\nstride = 16\nfilename = Second.buf\n";
 auto write=[&](const std::filesystem::path& path,const Bytes& bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());};
 Bytes positions(64),indices(12),mapping(16),delta(36);for(int i=0;i<4;++i){Put<float>(positions,i*16,float(i==1));Put<float>(positions,i*16+4,float(i==2));Put<uint32_t>(positions,i*16+12,0xAB123456);Put<int32_t>(mapping,i*4,i<3?i:-1);}for(int i=0;i<3;++i){Put<uint32_t>(indices,i*4,i);Put<float>(delta,i*12,1);}
 write(dir/"Position.buf",positions);write(dir/"Second.buf",positions);write(dir/"Index.buf",indices);write(dir/"Position_merged_map.buf",mapping);write(dir/"Position_merged_packed_pos_delta.buf",delta);{std::ofstream out(file,std::ios::binary);out<<source;}
 auto mod=EfmiBridge::Parse(file,source,dir);check(mod.meshes.size()==2&&mod.meshes[0].count==3,"literal mesh draw scanned");Probe(mod,mod.meshes[0]);Probe(mod,mod.meshes[1]);check(mod.meshes[0].supported,"known vertex format probed");mod.meshes[0].values={10,0,0,0,0,90,2,2,2,0,0,0};mod.meshes[0].dirty=true;EfmiBridge::Scan scan;scan.mods={mod};auto buffers=Build(scan);check(buffers.size()==2,"position and shape buffers generated");auto p=Vertex(buffers[0].bytes,16);check(std::abs(p[0]-.1f)<.0001f&&std::abs(p[1]-2)<.0001f,"scale rotate translate around pivot");check(Get<uint32_t>(buffers[0].bytes,28)==0xAB123456,"packed attribute bytes preserved");check(std::equal(positions.begin()+48,positions.end(),buffers[0].bytes.begin()+48),"unselected vertex unchanged");auto d=Vertex(buffers[1].bytes,0);check(std::abs(d[0])<.0001f&&std::abs(d[1]-2)<.0001f,"shape delta transformed without translation");check(Read(dir/"Position.buf")==positions,"source position buffer unchanged");auto ini=Ini(buffers,"revision");check(ini.find("post Resource\\TestMod\\Position = ref ResourceBEMBuffer0")!=ini.npos&&ini.find("post Resource\\TestMod\\Position_0")!=ini.npos,"namespace aliases cover dynamic shape reset source");check(ini.find("filename = BEM.GeneratedMeshes/revision/buffer0.buf")!=ini.npos,"generated buffer isolated");
 LiveBuffers live;
 for(float x:{20.f,25.f,30.f}){
  scan.mods[0].meshes[0].values[0]=x;
  live.Queue(scan);
 }
 live.Flush();
 auto first=LivePath(dir/"Position.buf");
 auto second=LivePath(dir/"Second.buf");
 check(live.Error().empty()&&IsLiveFile(first)&&!std::filesystem::exists(second),
       "first slider publishes a buffer without writing a bridge");
 if(IsLiveFile(first))check(std::abs(Get<float>(Read(first),LiveHeaderSize+16)-.3f)<.0001f,
                             "latest value reaches the live buffer");
 scan.mods[0].meshes[1].values[0]=40;
 scan.mods[0].meshes[1].dirty=true;
 live.Queue(scan);live.Flush();
 check(live.Error().empty()&&IsLiveFile(first)&&IsLiveFile(second),
       "second part can join live updates without reloading");
 if(IsLiveFile(second))check(std::abs(Get<float>(Read(second),LiveHeaderSize)-.4f)<.0001f,
                              "second part receives its own transform");
 auto reloaded=mod;
 LoadLiveSettings(reloaded);
 check(reloaded.meshes[0].values[0]==30&&reloaded.meshes[1].values[0]==40,
       "live values saved per appearance");
 live.Stop();
 DisableLiveFiles(scan);
 check(!std::filesystem::exists(first)&&!std::filesystem::exists(second)&&Read(dir/"Position.buf")==positions,
       "disabled live files leave the originals intact");
 std::filesystem::remove(dir/"BEM.GeneratedMeshes");
 const auto cacheFile=dir/"model-cache.json";
 EfmiScanCache::Scan cold(cacheFile);
 const auto firstScan=cold.ReadMod(file,dir);
 check(cold.hits==0&&cold.scanned==1&&firstScan.meshes[0].supported&&cold.Changed()&&cold.Write(cacheFile),
       "cold model scan saves parsed metadata and probe results");
 EfmiScanCache::Scan warm(cacheFile);
 const auto warmScan=warm.ReadMod(file,dir);
 check(warm.hits==1&&warm.scanned==0&&!warm.Changed()&&warmScan.meshes[0].values==firstScan.meshes[0].values,
       "unchanged model uses cached mesh support and pivot");
 const auto positionFile=dir/"Position.buf";
 const auto earlier=std::filesystem::last_write_time(positionFile);
 auto changedPositions=positions;
 Put<float>(changedPositions,16,2);
 write(positionFile,changedPositions);
 std::filesystem::last_write_time(positionFile,earlier+std::chrono::seconds(2));
 EfmiScanCache::Scan changed(cacheFile);
 const auto changedScan=changed.ReadMod(file,dir);
 check(changed.hits==0&&changed.scanned==1&&changedScan.meshes[0].values[9]==100&&changed.Write(cacheFile),
       "same-size vertex changes invalidate the cached pivot");
 {std::ofstream out(file,std::ios::app);out<<"\n; INI changed";}
 EfmiScanCache::Scan changedIni(cacheFile);
 changedIni.ReadMod(file,dir);
 check(changedIni.hits==0&&changedIni.scanned==1,"INI changes invalidate the cached catalog");
 {std::ofstream out(cacheFile);out<<"{unfinished";}
 EfmiScanCache::Scan damaged(cacheFile);
 check(damaged.ReadMod(file,dir).meshes[0].supported&&damaged.scanned==1,"corrupt cache falls back to a normal scan");
 std::filesystem::remove(cacheFile);
 write(positionFile,positions);
 scan.mods[0].meshes.push_back(scan.mods[0].meshes[0]);scan.mods[0].meshes.back().name="unselected-shared";scan.mods[0].meshes.back().dirty=false;bool refused=false;try{Build(scan);}catch(...){refused=true;}check(refused,"unselected shared mesh refused");
 if(argc>1){auto real=EfmiBridge::Discover(std::filesystem::path(std::u8string(argv[1],argv[1]+std::strlen(argv[1]))));int supported=0,selected=0;for(auto& m:real.mods)for(auto& mesh:m.meshes){
  Probe(m,mesh);if(mesh.supported)++supported;
  auto label=EfmiBridge::MeshLabel(mesh.name);
  if(argc>2?label.starts_with(argv[2]):label=="魅魔翅膀.001"){
   check(mesh.supported,"real selected mesh supported");
   if(mesh.supported){mesh.values[0]=1;mesh.dirty=true;++selected;}
  }
 }try{
  auto start=std::chrono::steady_clock::now();
  auto result=Build(real);
  check(argc>2?(selected>0&&!result.empty()):(selected==2&&result.size()>=4),"real mesh generated");
  auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"Read-only real mesh test: "<<supported<<" supported meshes; "<<selected<<" selected meshes; "<<result.size()<<" generated buffers (not written); "<<ms<<" ms\n";
 }catch(const std::exception& e){++fail;std::cerr<<"Real mesh build: "<<e.what()<<"\n";}}
 for(auto& name:{"mod.ini","Position.buf","Second.buf","Index.buf","Position_merged_map.buf","Position_merged_packed_pos_delta.buf"})std::filesystem::remove(dir/name);std::filesystem::remove(dir);return fail?1:0;}
