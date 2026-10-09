#pragma once
#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
namespace InteractionPanel {
enum class Action {SyncAnimations,BlenderFolder,SupportMode,ExternalLive,ExternalModReset,ExternalMeshReset,ExternalMeshValue,ExternalFolder,ExternalScan,ExternalValue,ExternalApply,ExternalRemove,Connect,Scan,Start,Stop,Calibration,Hotkeys,Enabled,Motion,Target,Capture,Grip,Save,Import,UiMode,Page,DeleteAnimation,ReorderAnimation,DraftTarget,DraftMotion,Preview,Prepare,Export,ImportSetting,Reset,BoneScan,BoneValue,ImportFbx,FbxAction,Pause,Loop,Seek,Step,MarkIn,MarkOut,RangeLoop,SliceSave,TimelinePlay,RangeReset,ExportAnimation,ImportControlledFbx,ControlledMotion,MainSupportMode,MainLoop,MainPreview,MainStop,MainTimelinePlay,MainPause,MainSeek,MainStep,MainMarkIn,MainMarkOut,MainRangeLoop,MainRangeReset,MainSliceSave,MainExportAnimation};
inline Action ActorAction(Action action,bool main){if(!main)return action;switch(action){case Action::SupportMode:return Action::MainSupportMode;case Action::Loop:return Action::MainLoop;case Action::Preview:return Action::MainPreview;case Action::Stop:return Action::MainStop;case Action::TimelinePlay:return Action::MainTimelinePlay;case Action::Pause:return Action::MainPause;case Action::Seek:return Action::MainSeek;case Action::Step:return Action::MainStep;case Action::MarkIn:return Action::MainMarkIn;case Action::MarkOut:return Action::MainMarkOut;case Action::RangeLoop:return Action::MainRangeLoop;case Action::RangeReset:return Action::MainRangeReset;case Action::SliceSave:return Action::MainSliceSave;case Action::ExportAnimation:return Action::MainExportAnimation;default:return action;}}
inline bool MainAction(Action& action){switch(action){case Action::MainSupportMode:action=Action::SupportMode;return true;case Action::MainLoop:action=Action::Loop;return true;case Action::MainPreview:action=Action::Preview;return true;case Action::MainStop:action=Action::Stop;return true;case Action::MainTimelinePlay:action=Action::TimelinePlay;return true;case Action::MainPause:action=Action::Pause;return true;case Action::MainSeek:action=Action::Seek;return true;case Action::MainStep:action=Action::Step;return true;case Action::MainMarkIn:action=Action::MarkIn;return true;case Action::MainMarkOut:action=Action::MarkOut;return true;case Action::MainRangeLoop:action=Action::RangeLoop;return true;case Action::MainRangeReset:action=Action::RangeReset;return true;case Action::MainSliceSave:action=Action::SliceSave;return true;case Action::MainExportAnimation:action=Action::ExportAnimation;return true;default:return false;}}
struct Command {Action action;int index=0;float value=0;std::string text;float extraValue=0;};
struct SliderDrag {
 Command command;float initial=0,low=0,high=1,step=1;int startX=0;float pixels=1;bool active=false;
 float Value(int x)const{return std::clamp(std::round((initial+(x-startX)*(high-low)/std::max(1.f,pixels))/step)*step,low,high);}
};
struct Slot {bool enabled=false;std::string name,target,animation,controlledAnimation;bool importedControlled=false;};
struct Bone {std::string key,name,group;int actor=0;std::array<float,9> values{0,0,0,0,0,0,1,1,1};};
struct ExternalControl {int index;std::string key,name,binding;float low,high,value;bool integer,dirty;};
struct ExternalMod {std::string name,file;std::vector<ExternalControl> controls;std::vector<std::string> meshes;};
struct ExternalMesh {int index;std::string key,mod,name,reason;bool supported;std::array<float,12> values;};
struct Snapshot {bool syncAnimations=false;float mainTime=0,mainDuration=0,mainStart=0,mainEnd=0;bool mainRangeLoop=false,mainActive=false,mainPaused=true,mainLoop=false;std::string mainSupport="none",mainAnimation="none";std::vector<int> animationActors;bool externalLive=false;std::string externalLiveStatus;std::vector<ExternalMesh> externalMeshes;std::string externalFolder,externalReloadKey;std::vector<ExternalMod> externalMods;std::vector<std::string> externalErrors;std::vector<std::string> externalModels;std::vector<Bone> bones;float timelineTime=0,timelineDuration=0,rangeStart=0,rangeEnd=0;bool rangeLoop=false,timelineActive=false;bool scaleAvailable=false;bool connected=false,calibration=false,hotkeys=false,clipPaused=false,loopEnabled=false;int active=-1;std::array<Slot,9> slots;std::array<float,42> grip{};std::vector<std::string> targets,animations;std::vector<std::string> animationNames;std::string blenderFolder;std::string supportMode="none";std::string status,controlled,draftTarget,draftAnimation,exportJson,exportPath,fbxAction,playbackTime;bool prepared=false;};
inline float SliderValue(int field,float fraction){float low=field<3?-8.f:field<6?-60.f:-20.f,high=field<3?8.f:field<6?60.f:100.f;float step=field<3?.1f:1.f;return std::clamp(std::round((low+std::clamp(fraction,0.f,1.f)*(high-low))/step)*step,low,high);}
inline bool MouseCancelsPose(bool mouseDown,bool overPanel){return mouseDown&&!overPanel;}
inline bool ToggleOpen(bool down,bool focused,bool& previous,bool& open){bool edge=down&&!previous;previous=down;if(edge&&focused){open=!open;return true;}return false;}
inline bool PreviewReady(bool focused,bool editing,bool mouseDown,bool calibration=false){return (focused||calibration)&&(!editing||calibration)&&(!focused||!mouseDown);}
inline bool GameInputAllowed(bool focused,bool editing){return focused&&!editing;}
using Read=Snapshot(*)();using Execute=void(*)(Command);
#ifdef INTERACTION_NO_OVERLAY
inline bool RequestBlenderPicker(){return false;}inline bool RequestAnimationSavePicker(bool main=false){return false;}inline bool RequestExternalPicker(){return false;}inline bool RequestFbxPicker(bool main=false){return false;}inline bool Start(Read,Execute){return false;}inline void Stop(){}inline void Show(bool){}inline bool MouseOver(){return false;}inline bool Editing(){return false;}
#else
bool RequestBlenderPicker();bool RequestAnimationSavePicker(bool main=false);bool RequestExternalPicker();bool RequestFbxPicker(bool main=false);bool Start(Read,Execute);void Stop();void Show(bool);bool MouseOver();bool Editing();
#endif
}
