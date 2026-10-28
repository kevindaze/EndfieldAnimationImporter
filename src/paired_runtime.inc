bool DraftSourcesValid();extern std::string fbxManagerAction;
Presets::Json OperatorState(){auto result=Presets::Json::array();for(auto& [key,name]:CharacterNames::All())if(OperatorRoster::Known(key))result.push_back({{"id",key},{"name",name},{"male",OperatorRoster::Male(key)},{"enabled",operatorWhitelist.Allows(key)}});return result;}
PairedAnimations::Pair* DraftPair(){return pairedAnimations.Match(workflow.draft);}
Presets::Json PairedState(){auto list=Presets::Json::array();for(auto& p:pairedAnimations.pairs){auto roles=Presets::Json::array();for(auto& [role,entry]:p.variants)roles.push_back(role);list.push_back({{"id",p.id},{"name",p.name},{"controlled",p.base.controlled},{"targets",roles}});}auto match=DraftPair();return {{"items",list},{"matching",match?match->id:std::string{}},{"can_create",match==nullptr&&DraftSourcesValid()&&operatorWhitelist.Allows(workflow.draft.target)},{"can_add",match&&DraftSourcesValid()&&operatorWhitelist.Allows(workflow.draft.target)},{"is_supported",match&&match->variants.contains(workflow.draft.target)}};}
void SetDraftEntry(Presets::Entry entry){workflow.draft=entry;workflow.draft.enabled=true;gripTarget=entry.grip;gripCurrent=entry.grip;boneTarget=entry.bones;controlledPose=fbxManagerAction=entry.controlledPose;controlledSupport=entry.controlledSupport;supportTarget=entry.support;selectedIdentity=entry.target;selectedSlot=-1;selectedModel.clear();boneCatalog.clear();++boneGeneration;workflow.Invalidate();}
void ApplyKnownPairOnSourceSelection(){
 auto pair=DraftPair();if(!pair||!operatorWhitelist.Allows(workflow.draft.target))return;
 Presets::Entry entry;if(pairedAnimations.Resolve(pair->id,workflow.draft.target,true,entry))SetDraftEntry(entry);
 pairedUiPending=true;
}
bool LoadPairedDraft(const std::string& id){
 auto selected=pendingRoleSelection.empty()?workflow.draft.target:pendingRoleSelection;Presets::Entry entry;if(!pairedAnimations.Resolve(id,selected,true,entry)){panelStatus="找不到配對動畫";return false;}
 if(!operatorWhitelist.Allows(entry.target)){panelStatus="此角色不在生成角色白名單中";return false;}
 if(pose.pins){restoreRequested=true;}pendingPreview=false;pendingSelectionStop=true;
 pendingRoleSelection.clear();SetDraftEntry(entry);SelectCalibrationAnimation();SelectCalibrationAnimation(true);if(!SelectedTimelineClip()&&!SelectedTimelineClip(true))pendingRoleSelection=entry.target;panelStatus="配對動畫與角色參數已載入";targetsPending=true;pairedUiPending=true;return true;
}
std::string FirstFiveCharacters(const std::string& text){size_t end=0;for(int count=0;end<text.size()&&count<5;++count){auto c=static_cast<unsigned char>(text[end]);size_t bytes=c<0x80?1:c<0xe0?2:c<0xf0?3:4;if(end+bytes>text.size())break;end+=bytes;}return text.substr(0,end);}
std::string PairAnimationName(const std::string& source){if(source.starts_with("clip:")){auto it=animationLibrary.find(source.substr(5));return it!=animationLibrary.end()?it->second->name:"動畫缺失";}static const std::map<std::string,std::string> labels{{"none","無"},{"inherit","預設姿勢"},{"standing","站立"},{"front_waist","正面摟腰"},{"back_waist","背面摟腰"},{"waist_hold","摟腰"},{"head_pat","摸頭"},{"head","頭部測試"},{"handshake","握手"},{"mutual","雙向對視"},{"lookat","對視"}};auto found=labels.find(source);return found==labels.end()?"動畫缺失":found->second;}
std::string PairName(const Presets::Entry& entry){auto role=CharacterNames::Name(entry.target);if(role.starts_with("chr_")){auto separator=role.find('_',4);role=role.substr(separator==std::string::npos?4:separator+1);}return FirstFiveCharacters(PairAnimationName(entry.controlledPose=="imported"?entry.controlledAnimation:entry.controlledPose))+"x"+FirstFiveCharacters(PairAnimationName(entry.animation))+"x"+role;}
bool RenamePaired(const std::string& id,const std::string& name){auto p=pairedAnimations.Find(id);if(!p){panelStatus="找不到配對動畫";return false;}auto previous=*p;if(!pairedAnimations.Rename(id,name)){panelStatus="名稱不可空白或超過 256 位元組";return false;}if(!SavePanelSettings()){*p=previous;panelStatus="重新命名保存失敗";return false;}pairedUiPending=true;panelStatus="配對動畫已重新命名";return true;}
bool RepairPairedNames(){bool changed=false;for(auto& pair:pairedAnimations.pairs){if(pair.customName)continue;bool oldDefault=pair.name.starts_with("clip:")||pair.name.empty()||pair.name==pair.base.animation||(pair.name.starts_with(CharacterNames::Name(pair.base.controlled)+" · ")&&pair.name.find(" / ")!=std::string::npos);if(pair.legacyName&&!oldDefault){pair.customName=true;pair.legacyName=false;changed=true;continue;}auto name=PairName(pair.base);if(pair.name!=name){pair.name=name;changed=true;}if(pair.legacyName){pair.legacyName=false;changed=true;}}return changed;}
bool SavePairedDraft(bool add){
 workflow.draft.grip=gripTarget;workflow.draft.bones=boneTarget;workflow.draft.support=supportTarget;workflow.draft.controlledSupport=controlledSupport;
 if(!operatorWhitelist.Allows(workflow.draft.target)){panelStatus="此角色不在生成角色白名單中";return false;}if(!add&&pairedAnimations.pairs.size()>=1024){panelStatus="配對動畫數量已達上限";return false;}if(!DraftSourcesValid()){panelStatus="動畫缺失或主控不符，請重新選擇對應動畫";return false;}Presets::Entry validated;if(!Workflow::ReadEntry(Workflow::EntryJson(workflow.draft),validated)){panelStatus="設定無效";return false;}
 auto before=pairedAnimations;auto draftBefore=workflow.draft;auto match=DraftPair();bool updating=match&&match->variants.contains(validated.target);
 if(add){if(!match||!(updating?pairedAnimations.Update(match->id,validated):pairedAnimations.Add(match->id,validated))){panelStatus="尚未建立配對動畫或來源不符";return false;}workflow.draft.pairId=match->id;}
 else{if(match){panelStatus="此配對動畫已存在，請新增支援當前角色";return false;}workflow.draft.pairId=pairedAnimations.Create(validated,PairName(validated));}
 if(!SavePanelSettings()){pairedAnimations=std::move(before);workflow.draft=draftBefore;panelStatus="配對動畫保存失敗";return false;}
 workflow.Invalidate();pairedUiPending=true;panelStatus=add?(updating?"已更新當前角色的雙方參數":"已新增當前角色的雙方參數"):"已匯出成配對動畫";return true;
}
bool AssignPairToSlot(int slot,const std::string& id){auto pair=pairedAnimations.Find(id);if(slot<0||slot>=9||!pair)return false;auto before=presetBank.slots[slot];auto name=before.name;presetBank.slots[slot]=pair->base;presetBank.slots[slot].pairId=id;presetBank.slots[slot].enabled=true;presetBank.slots[slot].name=name.empty()?pair->name:name;if(!SavePanelSettings()){presetBank.slots[slot]=before;return false;}presetKeys.fill(true);pairedUiPending=true;return true;}
void PumpRoleSelection(){
 if(pendingRoleSelection.empty())return;pairedUiPending=true;auto target=pendingRoleSelection;pendingRoleSelection.clear();if(!operatorWhitelist.Allows(target)){panelStatus="此角色不在生成角色白名單中";return;}
 if(pose.pins)StopPose("partner selection",true);pendingPreview=pendingSelectionStop=false;
 if(!EnsureInteractionPartner(target))return;
 auto pair=DraftPair();Presets::Entry entry;if(pair&&pairedAnimations.Resolve(pair->id,target,true,entry)){SetDraftEntry(entry);}
 else{workflow.draft.target=target;selectedIdentity=target;selectedSlot=-1;selectedModel.clear();boneTarget.Reset(1);workflow.draft.bones=boneTarget;boneCatalog.clear();++boneGeneration;workflow.Invalidate();}
 targetsPending=true;pairedUiPending=true;panelStatus="互動角色已選擇";
}
void MigratePairedSettings(){
 if(pairStorageInvalid)return;auto before=pairedAnimations;auto bankBefore=presetBank;
 bool changed=pairedAnimations.Migrate(presetBank);changed=RepairPairedNames()||changed;if(!changed)return;
 try{auto path=PanelSettingsPath();if(!path.empty()&&std::filesystem::exists(path)){auto backup=path;backup+=L".before-pairs-"+std::to_wstring(GetTickCount64())+L".json";std::filesystem::copy_file(path,backup,std::filesystem::copy_options::none);}}
 catch(...){pairedAnimations=std::move(before);presetBank=bankBefore;Log("Paired migration backup failed; old settings retained");return;}
 if(!SavePanelSettings()){pairedAnimations=std::move(before);presetBank=bankBefore;Log("Paired migration save failed; old settings retained");}else Log("Legacy presets migrated to paired animations; hotkey assignments retained");
}
