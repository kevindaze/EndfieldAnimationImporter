// Each actor owns its selection and clock. Unity writes are deferred to Gameplay.
struct TimelineDraft {std::string id;float time=0,start=0,end=0;bool loop=false;} timelineDraft,mainTimelineDraft;
bool syncAnimations=false;
bool pendingPreviewPaused=false,pendingSelectionStop=false,pendingPreviewMain=false;
TimelineDraft& DraftTimeline(bool main){return main?mainTimelineDraft:timelineDraft;}
std::shared_ptr<ImportedAnimation::Clip> SelectedTimelineClip(bool main=false){
 auto& draft=DraftTimeline(main);auto key=main?workflow.draft.controlledAnimation:workflow.draft.animation;
 if(main&&workflow.draft.controlledPose!="imported"||!key.starts_with("clip:")){draft={};return {};}
 auto it=animationLibrary.find(key.substr(5));if(it==animationLibrary.end()||it->second->absolute&&it->second->targetActor!=(main?0:1)){draft={};return {};}
 if(draft.id!=it->first)draft={it->first,0,0,it->second->duration,false};return it->second;
}
bool SelectedTimelineActive(bool main=false){auto clip=SelectedTimelineClip(main);auto playing=main?pose.controlledClip:pose.clip;return clip&&pose.pins&&playing&&pose.mode==Mode::Custom&&playing->id==clip->id&&!(main?pose.mainStopped:pose.partnerStopped);}
void StoreTimeline(bool main){if(pendingSelectionStop||!SelectedTimelineActive(main))return;auto& d=DraftTimeline(main);auto seek=main?pendingMainSeek:pendingSeek;d.time=seek?std::clamp(*seek,0.f,SelectedTimelineClip(main)->duration):(main?pose.mainTime:pose.clipTime);d.start=main?pose.mainStart:pose.rangeStart;d.end=main?pose.mainEnd:pose.rangeEnd;d.loop=main?pose.mainRangeLoop:pose.rangeLoop;}
Presets::Json TimelineJson(bool main=false){auto clip=SelectedTimelineClip(main);StoreTimeline(main);auto& d=DraftTimeline(main);return {{"id",clip?clip->id:std::string{}},{"name",clip?clip->name:std::string{}},{"duration",clip?clip->duration:0.f},{"time",d.time},{"start",d.start},{"end",d.end},{"loop",d.loop},{"paused",!SelectedTimelineActive(main)||(main?pose.mainPaused:pose.clipPaused)},{"active",SelectedTimelineActive(main)}};}
void QueueCalibrationPreview(bool paused,bool main=false){StoreTimeline();StoreTimeline(true);if(syncAnimations&&SelectedTimelineClip(main)&&SelectedTimelineClip(!main))DraftTimeline(!main).time=std::clamp(DraftTimeline(main).time,0.f,SelectedTimelineClip(!main)->duration);calibrationMode=true;pendingPreviewPaused=paused;pendingPreviewMain=main;pendingPreview=hook!=0;pendingTimeline=paused;}
void SelectCalibrationAnimation(bool main=false){
 if(workflow.draft.controlledPose=="imported"&&workflow.draft.controlledAnimation.starts_with("clip:")&&!workflow.draft.animation.starts_with("clip:"))workflow.draft.animation="standing";
 StoreTimeline(!main);DraftTimeline(main)={};(main?pendingMainSeek:pendingSeek).reset();pendingPreview=false;pendingSelectionStop=true;
 if(SelectedTimelineClip(main)){QueueCalibrationPreview(true,main);panelStatus=hook?"動畫已選取，暫停預覽已排隊":"動畫資訊已展開；請先連接遊戲入口";}
 else if(SelectedTimelineClip(!main))QueueCalibrationPreview(true,main);
 else panelStatus="動畫已選擇，需重新準備匯出";
}
bool TimelineActionSingle(InteractionPanel::Action action,int index=0,float value=0,bool main=false){
 auto clip=SelectedTimelineClip(main);if(!clip)return false;StoreTimeline(main);calibrationMode=true;auto& d=DraftTimeline(main);
 auto& time=main?pose.mainTime:pose.clipTime;auto& paused=main?pose.mainPaused:pose.clipPaused;auto& scrub=main?pose.mainScrubbing:pose.scrubbing;
 using A=InteractionPanel::Action;
 if(action==A::Seek||action==A::Step){if(!std::isfinite(value))return false;d.time=std::clamp(action==A::Step?d.time+index/30.f:value,0.f,clip->duration);if(SelectedTimelineActive(main)){(main?pendingMainSeek:pendingSeek)=d.time;paused=true;scrub=true;}else QueueCalibrationPreview(true,main);}
 else if(action==A::MarkIn){if(d.end-d.time<.2f){panelStatus="區間至少 0.2 秒";return false;}d.start=d.time;}
 else if(action==A::MarkOut){if(d.time-d.start<.2f){panelStatus="區間至少 0.2 秒";return false;}d.end=d.time;}
 else if(action==A::RangeReset){d.start=0;d.end=clip->duration;}
 else if(action==A::RangeLoop)d.loop=!d.loop;
 else if(action==A::TimelinePlay||action==A::Pause){if(SelectedTimelineActive(main)){paused=action==A::Pause?!paused:false;scrub=paused;if(!paused&&(time>=clip->duration||d.loop&&(time<d.start||time>=d.end)))time=d.loop?d.start:0.f;}else QueueCalibrationPreview(false,main);}
 else return false;
 if(SelectedTimelineActive(main)){(main?pose.mainStart:pose.rangeStart)=d.start;(main?pose.mainEnd:pose.rangeEnd)=d.end;(main?pose.mainRangeLoop:pose.rangeLoop)=d.loop;}
 return true;
}
// Mirror calibration commands in seconds, clamping at each clip's duration.
bool TimelineAction(InteractionPanel::Action action,int index=0,float value=0,bool main=false){
 using A=InteractionPanel::Action;
 auto other=syncAnimations?SelectedTimelineClip(!main):nullptr;
 if(other&&(action==A::MarkIn||action==A::MarkOut)){
  StoreTimeline(main);StoreTimeline(!main);auto& d=DraftTimeline(!main);float t=std::clamp(DraftTimeline(main).time,0.f,other->duration);
  if(action==A::MarkIn?d.end-t<.2f:t-d.start<.2f){panelStatus="區間至少 0.2 秒";return false;}
 }
 if(!TimelineActionSingle(action,index,value,main))return false;
 if(!other)return true;
 if(action==A::Seek||action==A::Step){TimelineActionSingle(A::Seek,0,DraftTimeline(main).time,!main);}
 else if(action==A::MarkIn||action==A::MarkOut){
  StoreTimeline(!main);auto& d=DraftTimeline(!main);float t=std::clamp(DraftTimeline(main).time,0.f,other->duration);
  if(action==A::MarkIn)d.start=t;else d.end=t;
  if(SelectedTimelineActive(!main)){(main?pose.rangeStart:pose.mainStart)=d.start;(main?pose.rangeEnd:pose.mainEnd)=d.end;}
 }
 else if(action==A::TimelinePlay||action==A::Pause){
  bool paused=SelectedTimelineActive(main)&&(main?pose.mainPaused:pose.clipPaused);
  TimelineActionSingle(A::Seek,0,SelectedTimelineActive(main)?(main?pose.mainTime:pose.clipTime):DraftTimeline(main).time,!main);
  if(!paused){TimelineActionSingle(A::TimelinePlay,0,0,!main);if(pendingPreview)pendingPreviewPaused=false;}
 }
 return true;
}
void QueueCalibrationStop(bool main){
 if(syncAnimations){pendingPreview=false;pendingMainStop=pendingPartnerStop=true;}
 else{if(pendingPreviewMain==main)pendingPreview=false;if(main)pendingMainStop=true;else pendingPartnerStop=true;}
}
void PumpCalibrationPreview(){
 auto requested=timelineDraft,requestedMain=mainTimelineDraft;bool partnerPaused=!pose.pins||pose.clipPaused,mainPaused=!pose.pins||pose.mainPaused;
 if(restoreRequested.load()&&pendingPreview){restoreRequested=false;StopPose("restore before preview",true);}
 if(pendingSelectionStop){pendingSelectionStop=false;if(pose.pins)StopPose("animation selection changed",true);}
 if(!pendingPreview||!(pendingTimeline||InteractionPanel::PreviewReady(GameFocused(),InteractionPanel::Editing(),((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON))&0x8000)!=0,calibrationMode)))return;
 bool paused=pendingPreviewPaused,main=pendingPreviewMain;pendingPreview=false;pendingTimeline=false;pendingPreviewPaused=false;
 if(pose.pins)StopPose("preview replaced",true);activePreset=-1;calibrationMode=true;timelineDraft=requested;mainTimelineDraft=requestedMain;
 auto preview=workflow.draft;preview.bones=boneTarget;preview.enabled=true;preview.grip=gripTarget;preview.support=supportTarget;
 if(!PrepareCalibration(preview))return;auto priorStatus=panelStatus;
 if(!BeginPose(armedMode)){calibrationMode=false;if(panelStatus==priorStatus)Event("preview_start_failed_check_log");return;}
 if(pose.clip){SelectedTimelineClip();pose.clipTime=timelineDraft.time;pose.rangeStart=timelineDraft.start;pose.rangeEnd=timelineDraft.end>0?timelineDraft.end:pose.clip->duration;pose.rangeLoop=timelineDraft.loop;pose.clipPaused=syncAnimations?paused:(main?partnerPaused:paused);pose.scrubbing=pose.clipPaused;}
 if(pose.controlledClip){SelectedTimelineClip(true);pose.mainTime=mainTimelineDraft.time;pose.mainStart=mainTimelineDraft.start;pose.mainEnd=mainTimelineDraft.end;pose.mainRangeLoop=mainTimelineDraft.loop;pose.mainPaused=syncAnimations?paused:(main?paused:mainPaused);pose.mainScrubbing=pose.mainPaused;}
 if(paused)Event("custom_animation_paused");
}
