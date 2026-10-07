// Selection metadata is available without a live pose. Transform writes remain
// on the Gameplay callback, including paused previews requested from menus.
struct TimelineDraft {std::string id;float time=0,start=0,end=0;bool loop=false;} timelineDraft;
bool pendingPreviewPaused=false,pendingSelectionStop=false;
std::shared_ptr<ImportedAnimation::Clip> SelectedTimelineClip(){
 if(!workflow.draft.animation.starts_with("clip:")){timelineDraft={};return {};}
 auto it=animationLibrary.find(workflow.draft.animation.substr(5));if(it==animationLibrary.end()){timelineDraft={};return {};}
 if(timelineDraft.id!=it->first)timelineDraft={it->first,0,0,it->second->duration,false};return it->second;
}
bool SelectedTimelineActive(){auto clip=SelectedTimelineClip();return clip&&pose.pins&&pose.clip&&pose.mode==Mode::Custom&&pose.clip->id==clip->id;}
void StoreTimeline(){if(!SelectedTimelineActive())return;timelineDraft.time=pendingSeek?std::clamp(*pendingSeek,0.f,pose.clip->duration):pose.clipTime;timelineDraft.start=pose.rangeStart;timelineDraft.end=pose.rangeEnd;timelineDraft.loop=pose.rangeLoop;}
Presets::Json TimelineJson(){auto clip=SelectedTimelineClip();StoreTimeline();return {{"id",clip?clip->id:std::string{}},{"name",clip?clip->name:std::string{}},{"duration",clip?clip->duration:0.f},{"time",timelineDraft.time},{"start",timelineDraft.start},{"end",timelineDraft.end},{"loop",timelineDraft.loop},{"paused",!SelectedTimelineActive()||pose.clipPaused},{"active",SelectedTimelineActive()}};}
void QueueCalibrationPreview(bool paused){calibrationMode=true;pendingPreviewPaused=paused;pendingPreview=hook!=0;pendingTimeline=paused;}
void SelectCalibrationAnimation(){
 timelineDraft={};pendingSeek.reset();pendingPreview=false;pendingSelectionStop=true;
 if(SelectedTimelineClip()){QueueCalibrationPreview(true);panelStatus=hook?"動畫已選取，暫停預覽已排隊":"動畫資訊已展開；請先連接遊戲入口";}
 else panelStatus="動畫已選擇，需重新準備匯出";
}
bool TimelineAction(InteractionPanel::Action action,int index=0,float value=0){
 auto clip=SelectedTimelineClip();if(!clip)return false;StoreTimeline();calibrationMode=true;
 using A=InteractionPanel::Action;
 if(action==A::Seek||action==A::Step){if(!std::isfinite(value))return false;timelineDraft.time=std::clamp(action==A::Step?timelineDraft.time+index/30.f:value,0.f,clip->duration);if(SelectedTimelineActive()){pendingSeek=timelineDraft.time;pose.clipPaused=true;pose.scrubbing=true;}else QueueCalibrationPreview(true);}
 else if(action==A::MarkIn){if(timelineDraft.end-timelineDraft.time<.2f){panelStatus="區間至少 0.2 秒";return false;}timelineDraft.start=timelineDraft.time;}
 else if(action==A::MarkOut){if(timelineDraft.time-timelineDraft.start<.2f){panelStatus="區間至少 0.2 秒";return false;}timelineDraft.end=timelineDraft.time;}
 else if(action==A::RangeReset){timelineDraft.start=0;timelineDraft.end=clip->duration;}
 else if(action==A::RangeLoop){timelineDraft.loop=!timelineDraft.loop;}
 else if(action==A::TimelinePlay||action==A::Pause){if(SelectedTimelineActive()){pose.clipPaused=action==A::Pause?!pose.clipPaused:false;pose.scrubbing=pose.clipPaused;if(!pose.clipPaused&&(pose.clipTime>=clip->duration||timelineDraft.loop&&(pose.clipTime<timelineDraft.start||pose.clipTime>=timelineDraft.end)))pose.clipTime=timelineDraft.loop?timelineDraft.start:0.f;}else QueueCalibrationPreview(false);}
 else return false;
 if(SelectedTimelineActive()){pose.rangeStart=timelineDraft.start;pose.rangeEnd=timelineDraft.end;pose.rangeLoop=timelineDraft.loop;}
 return true;
}
void PumpCalibrationPreview(){
 auto requested=timelineDraft;
 if(restoreRequested.load()&&pendingPreview){restoreRequested=false;StopPose("restore before preview",true);}
 if(pendingSelectionStop){pendingSelectionStop=false;if(pose.pins)StopPose("animation selection changed",true);}
 if(!pendingPreview||!(pendingTimeline||InteractionPanel::PreviewReady(GameFocused(),InteractionPanel::Editing(),((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON))&0x8000)!=0,calibrationMode)))return;
 bool paused=pendingPreviewPaused;pendingPreview=false;pendingTimeline=false;pendingPreviewPaused=false;
 if(pose.pins)StopPose("preview replaced",true);activePreset=-1;calibrationMode=true;
 timelineDraft=requested;
 auto preview=workflow.draft;preview.bones=boneTarget;preview.enabled=true;preview.grip=gripTarget;preview.support=supportTarget;
 if(!PrepareCalibration(preview))return;auto priorStatus=panelStatus;
 if(!BeginPose(PresetMode(preview.animation))){calibrationMode=false;if(panelStatus==priorStatus)Event("preview_start_failed_check_log");return;}
 if(pose.clip){SelectedTimelineClip();pose.clipTime=timelineDraft.time;pose.rangeStart=timelineDraft.start;pose.rangeEnd=timelineDraft.end;pose.rangeLoop=timelineDraft.loop;pose.clipPaused=paused;pose.scrubbing=paused;if(paused)Event("custom_animation_paused");}
}
