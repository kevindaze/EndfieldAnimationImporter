# EndfieldAnimationImporter 更新紀錄

## 1.0.4-Alpha（2026-10-07）

- 移除動畫啟動前的 0.4～3 公尺距離、0.25 公尺高度差與遠距離視線限制。隊伍中的已建立模型可從遠處開始動畫。
- 雙人站位依主控位置與朝向建立，不依配角原本待機位置；配角在第一幀直接定位，握手亦不繼承遠處地面高度。
- 保留隊伍成員、角色綁定、有效模型／骨架與姿勢租約檢查。未建立模型仍無法播放，未新增角色生成機制。
- 遠處直接定位寫入目標座標，避免插值終點從大座標相減產生殘留誤差；雙人放置不再播放接近過程。
- 加入遠距離／高度差、重疊位置、主控朝向與停止還原測試。Windows x64 編譯、9 項原生測試、UI 與語言回歸均通過。未新增遊戲內驗證。

## 1.0.3-Alpha（2026-10-07）

- 遊戲內面板標題列最左邊與網頁分頁列最左邊加入繁體中文／English 選單，切換後即時更新文字。
- 遊戲內語言保存於 Interaction/ui-language.txt；網頁使用 localStorage，兩者獨立保存。
- 加入英文按鈕、功能區、時間軸、支撐、骨架分類、提示及原生檔案視窗翻譯。使用者動畫名稱、ID、檔案路徑及參數資料保留。
- 翻譯來源統一為 ui/translations.tsv；tools/generate_translations.py 產生原生／網頁詞庫。Release 只附網頁執行用 JS，不附 TSV、產生器或文件。
- 新增 README.en.md，中文手冊加入英文連結與語言使用方式；保留 repo README 中使用者既有調整。
- Windows x64 編譯、9 項原生測試、原 UI 回歸與語言切換／偏好／動態更新／還原測試通過。未新增遊戲內驗證。

## 1.0.2-Alpha（2026-10-07）

- 修正 EFMI 橋接測試的路徑比較：使用 filesystem::equivalent 確認輸出位於所選實際資料夾，避免路徑正規化／Windows Junction 造成誤判。未停用橋接輸出位置檢查。
- 增加含「.」路徑的正規化檢查，並以 TEMP 指向 Windows Junction 驗證。
- 本機原生 8 項測試及 Junction 情境通過；重新編譯 DLL 與打包。GitHub 遠端需推送後再次確認，未新增遊戲內驗證。

## 1.0.1-Alpha（2026-10-07）

- git-release 整理為可獨立建置的原始碼 repo：src、CMake、測試與必要 vendored headers。
- 保留使用者既有 UI、工具與 README；舊 DLL／workflow 已備份至 artifacts 的 repo-migration-backup 目錄。
- GitHub Actions 改為 Windows x64 編譯與測試後打包，不再壓縮既有 DLL。
- 發布 ZIP 僅包含新 DLL、必要執行資源、範例與授權說明；排除原始碼、測試、建置腳本、docs 與外部模型描述。
- 發布工作保留原有預設分支自動發布行為，也支援版本標籤；舊 Release 與本機 ZIP 不覆蓋。
- CI 使用合成 EFMI 測試資料，不依賴本機 Mods 或旁邊的 BEM checkout。
- 本機 Windows x64 編譯成功，8 項原生測試與 UI 回歸測試通過；ZIP 完整性、內容排除、新 DLL 一致性及同版本禁止覆蓋驗證通過。GitHub Actions 尚未在遠端執行，未新增遊戲內驗證。

## 1.0.0-Alpha（2026-10-07）

### 發布包更新（2026-10-07）

- 遊戲內控制面板標題改為 `EAI面板(Num0)`，展開與收合一致。
- 文件僅保留於專案 `docs/`，Release ZIP 不含 docs／doc。
- Release ZIP 移除 external-models 說明資料夾與舊翅膀描述檔；通用 EFMI 掃描保留。
- 後續程式更動即更新 artifacts 套件；每版更新本紀錄，使用手冊僅按要求更新。
- 面板標題修改已編譯通過；未新增遊戲內驗證。

- 首次公開發布版本，以 0.31.1 功能為基礎。
- 名稱改為 EndfieldAnimationImporter，版本改為 1.0.0-Alpha。
- 移除寫死本機 EFMI 路徑的 perlica-wings.json；通用 EFMI 資料夾掃描保留。
- 使用手冊位於 docs/README.md；舊開發紀錄保存於本文件下方，歷史段落可能與現行功能不同。
- 發布文件僅維護於專案 docs/，不打包進 Release ZIP；明確要求 Release 時再更新發布套件。

---

# 0.29.4 Deferred Context 路徑

F10 已確認會改變模型，表示檔案橋接有效；但即時狀態仍未觀察到任何相同大小的候選。前版只從立即 Context 採集方法地址，未採集各顯卡 Deferred Context 的獨立方法地址。本版兩者一併注册，共最多十六組 next relay，重複地址去重。

延後錄製時收集綁定候選，不在 Deferred Context 做 staging Map 或即時上傳。ExecuteCommandList 執行後從立即 Context 處理讀回及待更新資料，保留完整內容匹配要求。狀態新增「提交」計數。GPU 回歸測試實際建立 Deferred Context、錄製頂點綁定、FinishCommandList、提交到立即 Context，再確認讀回與即時更新。

這修正了尚未覆蓋的渲染路徑，但使用者環境的根因仍未確認。更新重啟後照常啟用即時、寫入橋接、F10，再拖曳測試。

# 0.29.3 從實際綁定資源驗證網格

0.29.2 遊戲測試已有建立與渲染回呼，但沒有內容匹配。本版增加 VS／CS 的 Shader Resource、頂點 Buffer 綁定及複製來源候選探索，減少依賴建立時初始資料。只對與已寫入檔案相同大小的 Buffer 做非阻塞 staging 讀回；完整內容唯一匹配後才允許既有 DEFAULT Buffer 更新，不凭大小直接修改。

每次寫入最多檢查 64 個候選；失敗候選不逐幀重讀。讀回使用 DO_NOT_WAIT，不主動 Flush 等待 GPU。狀態增加綁定、尺寸候選、讀回與等待網格數量。重新寫入會清空候選與舊捕獲。

更新重啟後啟用即時、調整網格、寫入橋接、F10，模型顯示後再拖曳。GPU 測試涵蓋建立入口未觀察到的 SRV 資源：讀回完整匹配後接通並更新，拒絕同大小但不同內容的資源。實際 EFMI 遊戲效果仍待確認。

# 0.29.2 即時入口覆蓋修正

之前只使用預設顯卡的 D3D11 方法地址，且「建立」僅計算附帶初始資料的成功呼叫，0 無法區分未攔到入口與延後初始化。本版枚舉最多八個可建立 D3D11 裝置的顯卡入口、去除重複地址，各入口保存独立 next relay。新增完整 UpdateSubresource 填入資料時的精確比對，並在 CopyResource 前嘗試上傳，避免資料先被複製再更新。

仍只接通完整內容唯一匹配的可更新 DEFAULT Buffer，部分更新與未知格式不捕獲。狀態增加「填入／顯卡入口」，更新回呼計數不再依賴是否有待上傳資料，Num0 狀態分行顯示，不會截斷後半段。

更新並重啟遊戲後：啟用即時 → 調整該網格 → 寫入橋接 → F10 → 等模型顯示 → 檢查捕獲數量與再次拖曳。實際 EFMI 是否接通尚待驗證；這是針對已發現入口覆蓋與診斷缺口的修正，不是確認了使用者環境的根因。

# 0.29.1 即時接通診斷

尚未宣稱修復 EFMI 接通問題。本版補上「建立／匹配／不支援／上傳／更新回呼」計數，避免只有 0 個 GPU Buffer 無法判斷原因。啟用即時後，調整任一網格、寫入橋接、回遊戲按 F10 並等模型顯示，再查看狀態。

建立為 0：建立 Buffer 的攔截沒有觀察到初始化資料；建立大於 0 但匹配為 0：生成資料沒有匹配；不支援大於 0：資料匹配但 GPU Buffer Usage／CPU 存取格式不支援本版更新。上傳數只表示實際呼叫更新，不代表遊戲畫面驗證成功。

# 0.29.0 即時網格調整原型

外部模型調整頁提醒：BEM 與 EFMI 可以同時啟動，BEM 請選「XInput 自啟動」。仍須停用 BEM 第一人稱／相機模組並重啟遊戲。

使用步驟：啟用「即時網格調整（原型）」→ 調整要控制的網格 → 寫入橋接 → 回遊戲按 EFMI 重載鍵（通常 F10）。狀態出現已捕獲 Buffer 數量後，再拖曳大小、角度、位移或支點，可即時更新已接通的網格。新增其他部件或尚未捕獲的 LOD，需重新寫入並重載。

即時模式利用 BEM 共用 Hook 鏈辨識已生成網格的完整位置 Buffer 內容，背景重算後更新 D3D11 GPU Buffer；不更動原始模組檔案，也不修改 BEM 主機。只支援可更新的 DEFAULT Buffer；不可變 Buffer 跳過。原 INI 的形態／開關參數仍須寫入橋接並重載。未顯示捕獲数量代表尚未接通，實際 EFMI 遊戲環境仍待測試。

即時變動不自動保存；要保留請按「寫入橋接」。停用即時後按 F10 會回到最後写入的橋接資料。原型每次接通最多 128 MB 網格資料與 64 個捕獲 Buffer，與動畫資料無限制設定無關。法線與切線仍保留原值。

每個模組新增「全部還原預設值」，還原原 INI 宣告的參數，以及支援網格的大小、角度、位移和支點。參數需寫入並重載；已接通網格可即時還原。各組件的還原按鈕與調整項目使用相同子層縮排。

驗證：八組 CTest、網頁操作測試通過；WARP GPU 回讀確認可更新 Buffer 的資料修改成功，並跳過不可變與內容不符的 Buffer。實際 EFMI 即時接通尚待使用者測試。

# 0.28.5 快捷鍵播放保持至原鍵停止

小鍵盤 1～9 啟動的預設持續播放，再按同一鍵停止並還原。一般移動、滑鼠點擊、切出視窗與非自訂動作時間上限不再取消該预設。播放期間按其他插槽鍵不切換，先按原鍵停止；面板停止仍可使用。FBX 自動循環至停止，內建握手保持接觸姿勢。校準播放原有循環選項維持不變。

角色切換、模型重載、角色或骨架失效、租約失效及寫入錯誤仍停止並還原。停止時清除活動插槽，避免後續校準誤用保持狀態。七組 CTest 與網頁測試通過，新增握手時間上限／失焦保持、不同鍵不切換、FBX 結尾循環與原鍵停止測試。遊戲內效果待確認。

# 0.28.4 FBX 幹員名稱與校準自動選角

新匯入 FBX 在原動畫名稱與管理員附加動作後加上綁定幹員名稱，例如「原名稱 · 正面雙手摟腰 · 佩利卡」。既有動畫名稱保持原樣；重新匯入可取得新命名。

Num0 與網頁校準播放都先讀取動畫 target_character，驗證隊伍中有唯一有效對應角色後才切換互動對象，並同步互動角色區。角色缺席或存在歧義時，提示需要加入哪位幹員，保留原草稿選角；主控不符仍沿用原提示。互動預設不自動改寫角色。自動換角後需重新更新骨架清單，避免沿用前一角色的掃描節點。

七組 CTest 與網頁操作測試通過，涵蓋自動切換、缺席具名提示與 UI 同步。遊戲內播放待驗證。

# 0.28.3 新增 Draw Component 掃描格式

外部模型掃描同時支援 Perlica 的 [mesh:名稱] 與 Endmin D-Out! 的 ; Draw Component 編號 名稱 註解。保留子網格顯示條件、繪製索引範圍，解析 ib/vb0 的 ref Resource 引用；清單顯示 Component 編號 / 名稱，不猜測未命名部件的用途。

更新後至「外部模型調整」重新掃描，展開 Endmin D-Out! → 網格變形。沿用大小、角度、位移、支點設定與寫入後 EFMI 重載。原始模組檔案不修改，BEM 主機維持原版，使用前仍須停用 BEM 相機模組並重啟遊戲。

七組 CTest 與網頁測試通過；實際來源唯讀掃描可支援網格由 92 增加至 110。測試核對 Endmin shoes 的 18084 索引、407265 起點及 Component9_VB0 資源。實際遊戲變形外觀仍待驗證。

# 0.28.2 使用前須停用 BEM 相機模組

使用前請停用 BEM 相機模組並重啟遊戲。僅關閉第一人稱視角可能仍會佔用 CameraManager.TailLateTick，造成互動入口連接失敗。模組介紹名稱與網頁設定頁提供提示；BEM 目前不顯示 manifest description，因此名稱加上簡短提示。

本機 BEM 主機已還原為原始備份，本更新包不依賴主機修補、不包含自訂第一人稱。保留網格變形與其餘互動功能。

# 0.28.1 遊戲入口衝突診斷

本機日誌確認 0.27.0 連接失敗：BEM camera 模組先成功安裝 CameraManager.TailLateTick 獨占 Hook，互動模組的共用鏈因此不能加入。並非外部模型掃描改壞入口；同一入口的獨占與共用機制不能並存。模組不會強制覆寫相機 Hook。

面板與網頁現在區分入口衝突、Runtime 尚未就緒、契約不相容與其他失敗，日誌記錄階段及 BE_Result 編號。失敗時清空尚未有效的下一層指標。此版本只改善診斷，尚未修正主機／相機的共存機制；暫時需在 BEM 停用相機模組、重新啟動遊戲，再連接互動入口。僅關閉第一人稱功能可能仍保留 Hook。

保留 0.28.0 網格調整功能。七組 CTest 與網頁測試通過，衝突恢復需遊戲內驗證。既有握手素材納入 interaction/motions，建置不再依賴已搬動的根目錄 assets。

# 0.28.0 外部網格大小、角度與位移

Num0／網頁「外部模型調整」重新掃描後，各模組新增「網格變形」。展開配件即可調整大小 XYZ（倍）、角度 XYZ（度）、位移 XYZ（cm）及支點 XYZ（cm）。支點預設為網格包圍盒中心；希望繞翅膀或角的根部旋轉時，先把支點移到根部。座標是模型局部座標，不是遊戲世界座標。

調整 → 寫入橋接 → 回遊戲按 EFMI 重載鍵（本專案 F10）。預設同步同名網格，包含 LOD；網頁可以取消同步。按「還原此網格（含同名 LOD）」後同樣需要寫入及重載。停用橋接並重載也會回到原始網格，但 EFMI 持久形態參數仍遵循上一版的保存方式。

根據繪製索引只修改所選配件的頂點；每次從原始 Buffer 重新計算縮放、旋轉、位移，避免反覆寫入造成累積變形。同時替換相同檔案的資源別名，將支援的形態鍵位置差值做相同縮放／旋轉，使拍動仍以變形後網格為基礎。原始 INI／Buffer 不修改，新增 BEM.ExternalMesh.ini、BEM.GeneratedMeshes/<版本>/buffer*.buf 及 BEM.GeneratedMeshes/settings.json。舊產生版本保留，避免覆寫遊戲正在使用的 Buffer。

目前支援 float3 位於起始位置、stride 12／16 的位置 Buffer，以及 R16_UINT／R32_UINT 索引；形態鍵支援單槽 packed position delta 與每頂點一個 int32 的 map。不明格式、缺少資源、索引越界、未選網格共用頂點或衝突變形會拒絕寫入，不猜測布局。原有蒙皮權重保留，不會建立新骨架；大小與角度是靜態變形，拍動仍由原模組控制。

打包法線／切線暫時保留原始值，較大角度及非等比縮放可能造成光照不正確。建議先等比大小 1.1、角度 10 度、位移 5 cm 做遊戲內驗證。

七組 CTest 與網頁操作測試通過。實際 Mods 唯讀測試辨識 92 個支援網格，翅膀兩組 LOD 成功在記憶體產生四個 Buffer；沒有寫入遊戲模組資料。遊戲內外觀與重載效果仍待驗證。

# 0.27.0 外部模型調整、模組掃描與 EFMI 重載橋接

Num0 面板第三頁「外部模型調整」位於模組設定右邊；網頁同樣有第三頁。選擇 Mods 或單一模組資料夾，掃描 INI 網格註解／繪製條件與 global persist 參數。清單表示模組定義，不表示部件目前可見。名稱優先參考部件註解與描述檔，缺少名稱時保留變數名。

調整參數 → 寫入橋接 → 回遊戲按 EFMI 重載键（目前專案為無修飾 F10）。橋接將已修改參數轉成具名命名空間的 Constants post 賦值；只寫 BEM.ExternalControls.ini，不更動原始模組 INI、Shader 或 d3dx_user.ini。參數不是逐幀即時通訊，寫入也不代表遊戲已套用。自動拍動仍由原模組執行，想固定翅膀形態時同時關閉自動拍動。

只支援能讀取的持久數值變數；範圍由原檔數值、條件及形態鍵描述推導，未知範圍須實測。不能自動把任意網格變成關節骨架，也未新增不存在的尺寸／旋轉參數。停用橋接將自有 INI 改名為 .bem-disabled，需重載；已套用參數可能留在 EFMI 持久設定中，因此停用不等於還原數值。搬資料夾或改原模組後重新掃描。多個資料夾各自橋接同一變數可能衝突，建議固定掃描整個 Mods。

中文路徑、命名空間、形態連續值、未編輯參數不覆寫與原檔保留已有離線測試；遊戲內 EFMI 重載套用仍待使用者驗證。

# 0.26.0 通用外部模型描述檔原型

更新全部骨架同時載入 external-models 與本機 ExternalModels 描述檔。Unity 實際節點放入「模型骨架」，包含已註冊且在 Bip001 之外的骨架；沿用 XYZ 調整。EFMI 翅膀形態鍵只列辨識結果與橋接未接通狀態，尚不能控制大小或動作。規格與範例見 external-models/README.md。

# 0.25.3 待機姿勢造成的重播偏移

絕對局部 FBX 動畫固定只有旋轉軌道的節點位置，以及未寫入動畫的父節點參考姿勢。參考值按角色與骨架名稱保存，重播不再重新取用當下待機值；原本有位置軌道的骨盆等節點仍播放原始位移。停止後依所有權規則還原遊戲最新姿勢。舊 Bip001 快取相容，其他必要節點第一次播放時建立參考；無須重新匯入 FBX。離線測試涵蓋待機變化、重播、父節點及位移軌道，遊戲畫面仍需實測。

# 0.25.2 Num0 面板與一致骨架參考

面板改為小鍵盤 0（Num Lock 開啟），不再使用 F8。FBX 未控制 Bip001 的參考值以角色身份保存至本機 overlay-settings，重新播放沿用，避免每次擷取不同待機姿勢造成校準參考改變。不同角色各自保存，停止仍還原最近遊戲值。每次播放自動記錄位置与參考，便於排查移動後偏差。此版是針對已知參考重擷取問題的修正，不能宣稱已完全解決使用者所見 XYZ 偏差，仍需遊戲內比較同動畫同幀。更換實際骨架的模型需另行更新參考。

---
# 0.25.1 角色不符的具體提示

主控不符時顯示所需男管／女管與互動角色；預設角色未找到時提示加入指定角色並更新隊伍；動畫角色不符時提示需要的角色与目前角色。網頁与 F8 共用原生 detail。角色名稱來自本機上游 combat dictionary；未知角色退回 ID。

---
# 0.25.0 自動辨識男女管理員

Gameplay 主控自動識別 chr_0002_endminm／chr_0003_endminf。校準設定與互動預設保留當前主控識別，男女管切換清除主控骨架校準、清單与已準備快照，需重新掃描／校準再準備。已保存插槽不改寫身份，主控不符時拒絕播放。原女管 schema 2 設定仍可讀取。時間軸、骨架根穩定与配角 Y 跟隨皆沿用。男管骨架与附加姿勢外觀仍需遊戲實測。

---
# 0.24.8 雙角色骨架根穩定與恢復高度跟隨

FBX absolute-local 播放時也固定女管 Bip001 未被動畫控制的位置與旋轉。管理員附加姿勢略過已綁定骨架，避免同節點重複寫入。恢復互動角色跟隨女管 root Y，並保留互動角色自己的 Y 校準。±200 cm X Z 範圍保持。停止時兩人的固定骨架仍還原最近觀察的遊戲姿勢。

---
# 0.24.7 穩定 FBX 骨架根

針對日志證實暫停時模型 root 穩定，但 Bip001 仍被遊戲更新改動的情況，固定互動角色 Bip001 未被 FBX 控制的位置／旋轉至播放開始的參考值。FBX 提供該節點的動畫通道時保留通道，骨盆動畫仍播放；停止時還原最近觀察到的遊戲骨架值並釋放控制權。僅適用 absolute-local FBX 動畫。此版不宣稱所有遊戲渲染階段都已驗證，需測試暫停時 Y=80～90。

---
# 0.24.6 互動角色獨立高度與加大人物位移範圍

FBX 自訂互動的配角 root 世界 Y 使用互動開始時配角基準高度 + 配角自己的 Y 校準（cm），不再跟隨管理員 Y 或旋轉傾斜帶來的垂直分量。X Z 與朝向仍以管理員為參考。動畫本身骨盆位移不移除。雙角色「整個人物」root X Z 範圍由 ±100 cm 增至 ±200 cm，Y 与其他骨架範圍不變；網頁、F8、匯入匯出驗證一致。

---
# 0.24.5 雙角色高度診斷

此版增加定位紀錄，尚未宣稱修正視覺高度跳變。修改任一角色 root 位移或旋轉後，記錄 10 秒雙角色 root 世界／局部位置、父節點位置與縮放、骨盆／腳位置，包含遊戲更新後寫入前與模組寫入後；每秒最多約 10 次（即時參數修改另記錄）。舊 root audit 只記互動角色，無法判断管理員的跳變。

測試：暫停在同一幀，僅將女性管理員整個人物 Y 從 58 改到 60 再返回 58，等待約 10 秒，提供測試結果即可依日誌定位。

---
# 0.24.4 面板按鈕整理

時間軸合併為「繼續／暫停播放時間軸」，依播放狀態切換。保存動畫在左、匯出動畫片段在右；匯出仍保存所選動畫，如需指定區間先保存該區間為新動畫再選取匯出。準備按鈕改為「準備匯出到[互動預設]」。左右按鈕分開滑鼠懸停反饋，移出面板清除反饋。

---
# 0.24.3 F8 面板整理

功能區改用較大白色標題並可收合；校準操作依流程排列，播放／停止、逐格、標記、匯出／保存使用同列左右按鈕。时间與區間標記顯示一位小數，註解集中在各區下方並使用小字。移除 root XYZ 說明，互動預設与槽位改顯示動畫名稱，缺失時不露出內部 ID。保存與匯出及校準設定備份功能保留。修正上一格的 -1 索引被誤當成切換槽位。

---
# 0.24.2 分開保存與匯出動畫

F8 時間軸「保存新動畫片段至動畫庫」只輸入名稱，保存一份至模組動畫庫並加入列表。需要分享時，在選擇動畫選單選取該片段，按「匯出所選動畫 JSON」另選位置；匯出不建立新的動畫列表項目，不包含校準設定。

---
# 0.24.1 恢復動畫 JSON 導入

網頁與 F8「動畫匯入」頁新增「導入動畫 JSON」，讀取本模組轉換／時間軸另存的動畫資料，驗證後保存並加入列表。同 ID 更新既有項目；不套用校準設定，也不重新做 FBX 重定向。保留 0.24.0 取消單動畫容量與時長上限的行為。校準設定 JSON 仍從小鍵盤插槽讀取。

---
# 0.24.0 完整 FBX 動畫與取消單動畫上限

使用者確認後，取消轉換／匯入／磁碟載入／裁切的單動畫 1 MB 上限，取消 120 秒播放時長上限，以及單軌道 4096、單動畫 20000 key 上限。保留格式、有限數值、時間遞增、骨架／角色等驗證。

FBX 改成只生成一個「完整動畫」項目，不再自動輸出預覽加 30 秒分段。F8 時間軸可以在整份動畫內預覽、標記區間、Loop 及保存新片段。裁切不覆寫完整來源。既有分段仍保留；要取得完整動畫需重新匯入原 FBX，ID 使用新的 _full。

自訂動畫不再被校準模式固定 wall-clock timeout 截斷；以動畫時間到終點結束，暫停／Loop 可持續到使用者停止。角色失效、控制權遺失、適用移動／焦點與寫入失敗仍停止還原。其他內建互動原型的限制未一併移除。

長軌道取樣改用二分搜尋。沒有配置自動分頁載入或串流，動畫仍整份解析至記憶體。無人為單檔容量與播放長度上限，不代表系統記憶體、浮點精度與執行平台無限。

原始 FBX 512 MB／一小時來源限制、五分鐘轉換工作逾時、128 個動畫列表與目前骨架 track 相容範圍仍保留；這些不是這次要求取消的兩項單動畫限制。

驗證使用大於 1 MB、7200 秒、22001 個 key 的資料，以及原始 FBX 266.7 秒完整轉換。未使用 Computer Use，本版實際遊戲載入與長時間播放仍需測試。

---
# 0.23.0 F8 動畫時間軸與分段

## 使用

1. 選擇已匯入 FBX 動畫，按「時間軸播放／繼續（面板內預覽）」。此入口啟用校準播放並保留 F8 面板，預覽直接使用遊戲角色。
2. 拖曳「時間（秒）」滑桿會定位並暫停；需 Gameplay 回呼運作，畫面於下一次更新反映。
3. 用前一格／後一格精調，每格是 1/30 秒的編輯時間格，不代表所有來源 FBX 都是 30 fps，也不是來源 key 數。
4. 按「標記起點」「標記終點」，區間至少 0.2 秒。
5. 開啟「區間 Loop」從起點循環；關閉後回到原本整段播放／校準 Loop 規則。
6. 按「命名並保存為新動畫片段」，在原生保存視窗輸入檔名（名稱不含 .json），同時保存檔案並加入動畫列表。保存的是新的動畫資產，不是校準設定；列表刷新後選取即可播放。

## 範圍與原理

此版在目前所選片段內剪輯，例如第 1 段為 0–30 秒；沒有取消既有 FBX 自動分段，也不提供跨片段或整份 267 秒來源的單一時間軸。欲預覽另一段，先停止，再選該段並開始時間軸播放。

定位與寫骨架由 Gameplay 回呼處理，UI 只送出時間請求。定位強制完整預覽權重，允許停在 0 秒及最後一格，不被播放端淡入淡出或結束條件清除。播放／暫停沿用動畫時鐘。

裁切保留範圍內的原 key，起訖時間落在兩個 key 中間時插值產生新 key，時間減去起點；保留 target_character、manager_action、distance、角色軌道及位移資訊。裁切會重新經過 Clip 驗證、大小上限與動畫列表容量檢查。來源不覆寫。沒有額外收進骨架校準偏移；校準仍由設定匯出保存。

停止並重新啟動會重設編輯區間；標記尚不跨啟動持久保存。Loop 不做首尾無縫化。新片段若保留管理員姿勢，播放仍需要同樣角色與適當校準。

已通過 Release 編譯、四項 CTest、網頁測試，包含實際 native 邏輯的 seek、step、range loop、端點預覽及裁切旋轉／位移邊界。未使用 Computer Use，未驗證本版原生 UI 的遊戲內外觀。既有 FBX 不需重新匯入。

---
# 0.22.0 FBX 肢段方向重定向修正

已確認舊 global rotation delta 保留了來源與目標不同的參考肢段方向。這次實際佩利卡 bind pose 與原 FBX 的 0、1、2、5、10 秒抽查，左右上臂／前臂／大腿／小腿方向誤差最大 48.64 度、平均 27.18 度。

新版在旋轉差轉換後，以來源當前關節間方向校正目標肢段，再按目標父子階層重建局部旋轉。保留遊戲骨架長度與原重定向的翻轉方向資訊。相同抽查誤差降至 float 計算精度內；这不是蒙皮／手指／所有幀外觀正確的保證。ForeTwist 與 corrective 系統仍需後續研究。

本次輸出角色是佩利卡 chr_0004_pelica，最新日誌亦顯示播放佩利卡，不是陳。原來源 FBX 不變；不同角色需各自重新匯入。既有動畫不會因升級模組自動重算，必須停止播放後重新匯入同一 FBX；相同角色與管理員動作更新相同動畫 ID。測試先選「不添加動作」並保持校準歸零。

未修改本機設定；磁碟保存設定仍可見先前主控右臂大角度偏移，這不代表目前 UI 一定仍在套用它，請在測試時確認完整骨架還原狀態。本版不自動清除使用者參數。

---
# 0.21.3 介面精簡

網頁與 F8 移除 JSON 動畫導入入口及舊握手接觸校準。動畫以 FBX 導入，骨架調整統一使用完整九軸設定。校準設定 JSON 匯出／插槽讀取保留；原有動畫資料與已保存參數未刪除，舊動畫解析器仍為既有資料相容保留。

本版沒有改動重定向或姿勢算法。0.21.2 本次紀錄確認 dt 約 1/60、clipTime 前進、paused=false；after_write 的配角手臂／手掌取樣誤差接近零，before_write 則可見遊戲更新改動。這驗證此回呼內寫入，但不證明渲染時間的最終姿勢；扭曲仍需處理蒙皮輔助骨、參考姿勢與更新順序。

---
# 0.21.2 配角動畫靜止診斷

F8 校準頁新增配角 FBX 播放時間與暫停狀態。每秒記錄 Playback audit 的 before_write / after_write，包含目前 clip、目標角色、回呼 dt、站位時鐘、播放時間，以及配角六個手臂／手掌軌道的 sample、actual、sample_change_deg、sample_error_deg、previous_write_error_deg 和 key 數。

此版目的是查明陳第 1 段手部不動的 runtime 原因，沒有宣稱已修好。before_write 差異代表兩次本模組更新之間的改寫，可能是正常 Animator，不能獨自證明改寫發生於渲染前。after_write 可驗證此回呼內的最終 Transform，不等於已擷取實際渲染姿勢。

測試：匯入新版後，播放陳第 1 段 10 秒，確認未暫停，停止。可再測一次不添加管理員動作的版本。原 FBX 不需為診斷重新匯入；沒有清除或更改使用者設定。將觀察結果交回後可讀取日誌定位。

---
# 0.21.1 接觸方向與手腕修正

腰部接觸方向由骨盆的校準軸及當前脊椎方向重建正交座標，摸頭使用頭部方向。修正左手掌法線鏡像，手腕折彎上限 60 度、軸向翻掌上限 110 度，保留每秒旋轉速度限制。輔助扭轉骨保留捕捉的相對方向並跟隨上臂，不再套用未驗證的 50% / 75% 反向補償。這不等於已完成遊戲原生 twist／corrective 系統重建。

肩部仍維持站姿；本版先隔離接觸框架與翻掌問題，未加入鎖骨 IK。極端姿勢可能仍超出手臂範圍，不會拉長手臂。既有動畫不用重新匯入；停止後重新播放即可使用新的運算。保留使用者校準設定，建議先以 Root 旋轉歸零比較，再加入所需偏移。

未使用 Computer Use，實際外觀仍需遊戲內確認。

---
# 0.21.0 管理員雙手姿勢

FBX 匯入的管理員動作新增正面雙手摟腰、背面雙手摟腰、雙手摸頭。正面與摸頭採面對面站位，背面採同方向、管理員在後方。兩手獨立追蹤當前 FBX 的腰部或頭部，保留手腕角度限制、平滑、停止還原及失敗回復。

新選項在匯入時套用；既有舊「輕摟」動畫仍保留單手版本以相容原設定。要使用新動作，重新匯入原 FBX 並選擇動作，結果以不同動畫 ID 保存。動畫列表會標示所選姿勢。

接觸距離使用初始估值；大幅移動或不同身材可能需要校準 Root 或骨架，手臂不會為了碰到目標而拉長。未使用 Computer Use，本版尚需遊戲內確認接觸外觀。

---
# 0.20.0 動畫選單、校準 Loop 與手臂扭轉骨

F8 校準頁的「選擇動畫」現在開啟選單，包含內建及已匯入動畫；選取後再按播放。
網頁與 F8 校準頁均提供 Loop 開關。Loop 只在校準模式啟用，保留站位與參數並循環播放；關閉後播完目前這輪再還原。暫停仍保留當前時間。設定會保存，但不修改原始 FBX 或動畫 JSON。來源動畫若首尾姿勢不同，循環接點仍可能跳姿勢。

實際轉換報告的 UpperArm.L / LowerArm.L / Hand.L 對應左側骨架，右側亦對應右側，沒有左右交換。此版讓雙方的 UpArmTwist / UpArmTwist1 輔助骨隨手臂旋轉更新，並在停止或失敗時還原。暫用 50% / 75% 軸向扭轉分配，尚未由遊戲蒙皮權重確認；不修改未知的 ty corrective 骨。這是手部扭曲的修正嘗試，不能取代遊戲內外觀確認。

保留 0.19.3 的滑桿拖曳捕捉與管理員參考站位修正。最新場景紀錄的 root 位置誤差為 0，但不能由修正後紀錄單獨證明之前跳動的唯一原因。

安裝：關閉遊戲後由 BEM 匯入本版 ZIP，重新啟動模組與遊戲。既有 FBX 動畫可直接測試，不需要重新轉換。
驗證：Release 編譯、四項 CTest、網頁回饋測試通過；未使用 Computer Use，未做本版遊戲內目視驗證。

---
# 0.18.0 FBX 匯入與 bind pose 重定向

使用者可直接選擇 FBX，模組會在本機轉換為內部動畫資料；不需要自行製作 JSON。

使用方式：
1. 關閉遊戲，匯入本版 ZIP，再重新啟動。
2. 連接遊戲入口、選擇互動角色（本次為佩利卡）。停止播放並讓角色回到正常待機；建議先還原骨架參數，避免舊校準偏移混入動作。
3. 動畫匯入頁按「導入 FBX」，或 F8 → 動畫匯入 → 導入 FBX，選擇原始 FBX。
4. 返回 Gameplay，模組讀取 Mesh.bindposes 與骨架父子关系，啟動隱藏 Blender 程序做背景轉換。原生面板或網頁會顯示轉換狀態；最多等待五分鐘。
5. 成功後自動選取新預覽動畫，再按「播放動畫」。長動畫另列為每段最多約 30 秒的片段，不會自動串接。

目前支援單一動畫人形骨架，來源命名支援 Mixamo 與本次 Chocolate_rig；不支援多角色 FBX、未烘焙約束、任意命名骨架、非人形。FBX 大小上限 512 MB，來源長度上限一小時。需 Blender 安裝在 Program Files/Blender Foundation，本機已有 Blender 5.2。沒有 Blender、缺少可靠 bind pose、骨架名稱歧義或骨架縮放不支援時，明確拒絕，不以待機旋轉代替 bind pose。

新版讀取 Unity Mesh.bindposes 的逆骨架矩陣，從模型空間重建目標骨架，做左右手座標系轉換、以世界方向差重定向，再按目標父骨架重建局部旋轉。保留骨架的均勻縮放，骨盆位移依身體比例轉移。不含臉部 BlendShape、頭髮、衣物、道具或接觸 IK。模型站位錨點仍固定，骨盆位移屬視覺動畫，未處理移動碰撞與導航。部分不參與蒙皮的輔助父骨架以當次參考姿勢固定。

內部動畫 schema v2 使用 absolute_local 旋轉与可選局部 position，只操作 partner；角色 ID 綁定，不能將佩利卡轉換結果套到另一角色。既有 v1 JSON 仍可使用。v2 不再套用舊握手程序化手指校準；全部骨架的通用位移／旋轉／縮放設定仍可疊加。

轉換結果與報告：%LOCALAPPDATA%/BetterEndfield/Interaction/FbxImports/<job>/
骨架參考：%LOCALAPPDATA%/BetterEndfield/Interaction/SkeletonReferences/
正式動畫列表：%LOCALAPPDATA%/BetterEndfield/Interaction/Animations/

重複匯入相同 FBX 到相同角色會更新相同動畫 ID；正在播放的動畫持有原快照。更新角色模型或骨架後應重新匯入。匯入前會驗證全部輸出片段及 128 個動畫列表上限；磁碟保存若中途失敗，會回報已匯入數量，可重試。

測試狀態：已通過離線播放／還原、矩陣重定向、骨盆位移、座標系與錯誤拒絕測試。用本次實際 FBX 和合成測試骨架完成端到端轉換；合成動畫不供遊戲使用。尚未取得實際佩利卡的 bind pose，因此沒有聲稱已輸出或目視驗證正確的佩利卡動畫。此版在遊戲內匯入時會自動產生新的目標角色動畫；bind pose 取得和姿勢效果仍需遊戲內驗證。

---

# 0.17.3 唯讀骨架姿勢參考

停止播放、更新全部骨架後，匯出兩角色目前局部／世界位置與旋轉、父子關係至本機 SkeletonReferences 目錄。這是 current Gameplay pose，不是真正 bind pose；用於研究重定向座標軸。匯出不修改骨架。播放中拒絕輸出已覆寫的參考。網頁顯示實際檔案路徑。此版不宣稱已修好 FBX 重定向，需取得參考後繼續處理。

# 0.17.2 網頁校準播放修正

校準播放不再要求遊戲視窗在前景。BEM 網頁按播放後由下一次 Gameplay 回呼啟動；F8 原生面板仍先收起。網頁播放按鈕附近顯示執行狀態，日誌記錄啟動拒絕原因。需實際遊戲驗證；遊戲若背景停止更新，仍需回遊戲恢復更新。

# 0.17.1 校準播放修正

遊戲內按「播放動畫」會自動收起 F8 面板、啟用校準模式，等滑鼠放開再啟動，避免啟動點擊立即取消。F8 可重新開啟面板調整參數。網頁播放仍需回遊戲。啟動失敗會回報狀態；實際遊戲行為需驗證。

# 0.17.0：完整骨架節點的九軸校準

## 使用

1. 匯入新版 ZIP 並重啟遊戲，控制女性管理員，連接入口、更新隊伍並套用互動角色。
2. 展開「骨架設定」，按「更新全部骨架」，回 Gameplay 等待掃描。再開啟面板查看兩人的實際節點。
3. 選擇動畫並播放。在網頁或 F8 面板展開角色 → 部位 → 節點，調整 XYZ 位移、XYZ 旋轉、XYZ 縮放。網頁可搜尋名稱，參數在展開節點後才產生；縮放 API 不可用時停用該控制。
4. 停止播放會移除新增修正並恢復遊戲動畫；還原預設會清除該角色／全部校準。
5. 準備匯出 → 匯出 JSON 或套用小鍵盤槽位。新增骨架參數納入完整設定，舊檔沒有這些欄位時採用零修正。

## 參數語意與範圍

- 掃描已驗證 Animator 的模型根節點，以及 Bip001 骨架樹下的實際 Transform，包括附屬節點。骨骼名稱与階層取自 runtime；不硬編碼頭、腿、腰、臉部的候選名稱。
- 「整個人物」是模型根節點的局部修正，主要改變顯示模型；不改寫 Gameplay Entity 的物理／導航座標。
- 位移 XYZ：相對父節點的局部軸，-100～100 cm，預設 0。
- 旋轉 XYZ：疊加在當前動畫上的局部旋轉增量，-180～180 度，預設 0。
- 縮放 XYZ：乘上當前局部縮放，0.1～3 倍，預設 1。縮放使用上游已存在的 Transform.get_localScale／set_localScale 契約；連接時驗證可用性。
- 修正作用於正在播放的預覽，沒有播放時僅編輯設定。原有握手接觸校準仍放在相容區，可與完整骨架修正疊加。
- 自訂動畫依檔案長度播放一次。內建預覽在校準模式或遊戲面板編輯時延長至約 120 秒。
- 沒有自動全身 IK、穿模／接觸修正或 BlendShape 編輯。位移、縮放可能改變肢體長度與既有握手對齊，需要實際調整。

## 保存、驗證與限制

bone_offsets 是稀疏物件；key 使用角色索引 0／1 加上相對階層名稱及同名序號，value 為 [位移X,Y,Z,旋轉X,Y,Z,縮放X,Y,Z]。只保存非預設值，沒有記錄記憶體位址。JSON 完整功能需 0.17.0 或之後版本，舊模組不會套用新增欄位。

每個角色掃描上限 1,200 個 Transform、深度 48；校準設定最多 1,200 個非預設節點。找不到已配置節點時拒絕開始，避免套用到其他骨架。換互動對象時清除對方骨架修正並要求重新掃描。

網頁通訊含封裝上限約 220 KB；較大的單份校準檔可從遊戲面板讀取（最多 4 MB）。網頁提示超出上限時改用遊戲面板操作；原生設定副本讀取上限 32 MB。網頁 JSON 匯出仍直接寫入 Interaction/Exports，不使用 WebView2 下載介面。

編譯成功，四組 C++ 測試與網頁流程測試通過，涵蓋雙角色根節點、九軸設定 JSON／槽位往返、180 幀不累積、停止恢復、缺失骨骼／API、過期清單拒絕、按需建立控制與匯出前完成待處理更新。本次未使用 Computer Use，未自動匯入 BEM；真實遊戲骨架數量、比例變更與視覺穩定性仍需實機驗證。

以下保留舊版紀錄，請以前述說明為準。
# 0.16.2：角色、校準與骨架區域排版

網頁依「互動角色 → 校準模式 → 骨架設定 → 小鍵盤預設」排列。互動角色操作分行，套用後顯示目前角色；校準按鈕下方固定顯示最近一次成功匯出的完整路徑，儲存中與失敗也在此顯示。四個校準按鈕各有說明。骨架設定為獨立大標題，下方是粗體摺疊入口。遊戲面板同步調整校準／骨架順序，加入匯出路徑與按鈕說明。

Windows 編譯、四組 C++ 測試與網頁流程測試通過。未使用 Computer Use，實際視覺布局仍需使用者確認。
# 0.16.1：避開 WebView2 下載介面

網頁「匯出 JSON」改用原生模組保存檔案，位置為 %LOCALAPPDATA%/BetterEndfield/Interaction/Exports；成功回覆會顯示完整路徑並提供 JSON 內容。移除 Blob 下載與下載面板，避免再次觸發使用者回報的「開啟下載資料夾後網頁卡住」。遊戲面板原有另存新檔保持可用。

若網頁已一直卡在開啟畫面：先完全退出 BEM（含系統匣），執行附帶 Repair-BEM-WebView.ps1，再開啟 BEM 匯入新版。腳本僅將 third-party/webview-profile 改名備份，保留模組、預設與動畫。仍需實機確認是否恢復；尚未確認 WebView2 卡住的根本原因。
# 0.16.0：兩頁介面與校準設定工作流程

## 使用順序

1. 匯入新版 ZIP，重啟遊戲。F8 開啟／關閉面板，Esc 返回遊戲。
2. 「動畫匯入」頁導入自訂動畫 JSON；「瀏覽動畫」列表可拖曳排序或刪除。排序保存並同步選擇動畫清單，刪除前詢問確認。
3. 「模組設定」頁第一列選擇互動角色；主控固定女性管理員。首次需連接入口並等待 Gameplay 更新隊伍。
4. 在「校準模式」選擇動畫，按「播放動畫」，關閉 F8 面板後才開始播放；可再次開啟面板調整。自訂動畫依檔案長度播放一次，沒有自動循環。
5. 「骨架設定」預設摺疊，分主控與互動角色，可各自還原預設；底部可全部還原。人物位置、頭頸、腰腿仍為明確標示的未實作區域。
6. 校準完成後按「準備匯出」。右側「匯出 JSON」輸出這份快照；或將「已準備的校準設定」匯入小鍵盤槽位。再次修改角色、動畫或骨架參數時需重新準備。
7. 各槽的「讀取 JSON」接受單份校準設定檔，直接填入該槽；停止播放不會清除校準數值。小鍵盤同一鍵開始／停止，不同鍵先還原再切換。

## 保存與相容性

- 第一次升級到這版，小鍵盤 1～9 全域開關預設啟用；空白槽位保持關閉。保存後可自行停用，後續載入保留設定。Num Lock 需開啟。
- 舊版九组預設、已導入的動畫及校準資料仍可載入。網頁的「全部預設備份」摺疊區保留舊匯入／匯出功能，與單份校準 JSON 分開。
- 動畫格式維持 endfield-interaction-animation v1，只接受自訂 JSON。FBX／BVH／VMD 不能直接導入。
- 單份校準格式為 endfield-interaction-calibration v1：setting 包含 controlled、target、animation、grip_values、name、enabled；主控必須為 chr_0003_endminf。匯出引用動畫 ID，不包含動畫檔；分享時需另附動畫 JSON。
- 準備快照只在目前模組執行期間保留；需要長期保存請匯出 JSON 或填入槽位。遊戲面板匯出可選保存位置；網頁嘗試下載並同時提供 JSON 文字備份。
- 動畫排序保存於 %LOCALAPPDATA%/BetterEndfield/Interaction/animation-order.json。刪除動畫不刪除引用它的槽位，也不立刻中斷正在播放的副本；新播放會拒絕缺失動畫。

## 自訂動畫校準

手掌位置為依角色朝向換算的額外位置修正（cm），直接修正手骨，不提供完整手臂 IK；大偏移可能拉伸手腕。手腕 XYZ 為額外局部旋轉。手指依已驗證的骨架幾何取得彎曲軸，預設數值代表零額外修正；與內建握手的絕對握合角語意不同。骨骼或幾何缺失時保留原動畫並記錄日誌。停止播放後移除本模組的位置與旋轉覆寫。

## 驗證範圍

Windows 模組編譯成功，四組 C++ 離線測試及網頁流程測試通過；涵蓋設定快照、失效檢查、JSON 角色／動畫／参数完整往返、拖曳排序、刪除保留引用、自訂動畫校準不累積和停止還原。沒有使用 Computer Use，未自動匯入 BEM；遊戲面板布局、拖曳手感與新校準的視覺效果仍需實機確認。

以下為舊版紀錄；操作請以前述 0.16.0 說明為準。
# 0.15.0：F8 切換操作、明確摺疊分類、導入動畫

- F8 按一次開啟面板，再按一次或 Esc 關閉；不需要按住 F8 或 Alt。
- 開啟時面板取得鍵盤焦點、釋放游標並停用遊戲主視窗的視窗輸入；關閉、切到其他程式或卸載時還原。此方式沒有新增 Raw Input hook，遊戲若仍接收背景裝置輸入，需實機回報確認。
- 編輯時不觸發本模組的小鍵盤快捷鍵，握手編輯時間延長至 120 秒。遊戲自身的待機動畫仍可執行。
- 摺疊列顯示「展開／收起」、包含的部位摘要與階層縮排。人物根節點、頭頸、腰、腿的未實作功能仍明確標示。
- 點「導入動畫 JSON」選檔；成功後在預設動作列切換至 clip:<id>，選互動角色並儲存，再使用開始／停止或小鍵盤。
- 套件 examples/arm_rotation_test.interaction-animation.json 是雙角色前臂旋轉測試，並非完整握手動畫。
- 原生面板接受最多 1 MB，網頁接受最多 60 KB；目前只支援下方自訂 JSON，不能直接讀 FBX、BVH、VMD。
- 動畫保存於 %LOCALAPPDATA%/BetterEndfield/Interaction/Animations。預設匯出只保存動畫 ID；分享時需另外提供動畫 JSON，接收者先導入動畫再導入預設。
- 自訂動畫沒有自動重定向、IK、位移或循環；手部校準參數只影響既有握手模式。缺少或重複骨骼時拒絕播放。
- 編譯、四組 C++ 離線測試及網頁操作測試通過。沒有使用 Computer Use；視覺布局、F8 焦點與遊戲輸入隔離需實機驗證。

## 自訂動畫 JSON v1

必要欄位：format="endfield-interaction-animation"、version=1、id、name、duration、actors；distance 可選，預設 1.2 公尺。

id 使用英數、底線或連字號，最多 80 字元。duration 為 0.2～120 秒；distance 為 0.4～3 公尺。
actors 必須各包含一份 role="controlled"（女性管理員）與 role="partner"（所選角色）。各角色 tracks 為 1～64 條，不可重複 bone。

每條軌道指定實際骨骼名稱 bone（Bip001 開頭），及 keys：每個 key 包含 time 和 rotation=[x,y,z,w]。rotation 是相對開始時骨骼局部姿勢的四元數旋轉增量，不是歐拉角，也不是世界座標。應採用已確認的遊戲骨架及軸向。
每條軌道至少兩個 key，時間嚴格遞增，從 0 到 duration；全檔最多 20,000 個 key。兩角色軌道同步播放，先完成站位，再播放並淡入淡出；結束或取消後恢復遊戲姿勢。

以下保留舊版紀錄；舊版 Alt 與 F8 動畫啟用說明不適用於 0.15.0。
# 0.14.0：實驗遊戲畫面面板與骨架摺疊

## 面板操作

- 模組載入後，遊戲畫面右側出現收合的「互動面板」；點標題展開／收合，拖標題移動，滾輪捲動。
- 按住 Alt 使用遊戲游標（沿用 BEM MMD 面板的使用方式）。先以視窗／無邊框模式測試；獨佔全螢幕覆蓋尚未驗證。
- 面板是 Win32 非啟動覆蓋視窗，跟隨遊戲視窗，切到其他程式會隱藏；不是嵌入 Unity Canvas 或直接載入原網頁。
- 支援連接／更新隊伍、九組開始停止、選取預設編輯、角色／動作切換、啟用狀態、校準、快捷鍵、即時滑桿與保存。
- 九組預設清單預設摺疊。預設編輯行點擊循環切換槽位，角色／動作行點擊循環選擇；角色必須先掃描隊伍。
- 「保存目前校準到此預設」會啟用該槽並保存；「儲存全部設定」保存所有預設與當前校準。
- 滑桿更動是暫時的；保存目前校準到槽位後，下一次啟動才會使用新參數。只儲存全部設定會保存校準編輯值，不會自動覆寫每個槽的獨立參數。
- 網頁新增顯示／隱藏面板按鈕，仍保留 JSON 匯入／匯出。

## 骨架分類

遊戲面板和網頁各自依主控／互動角色分組，人物根節點、頭／頸、腰／脊椎、左右腿、右手臂／手部可摺疊。手部再分手掌位置、手腕局部旋轉、五根手指及三個指節。尚未實作的部位明確顯示提示，沒有無效滑桿；本版仍只控制既有手部參數。

## 狀態、保存與限制

- Windows UI 執行緒只取得值型別快照、更新設定及排隊指令；Unity 物件存取與姿勢更動仍在既有 Gameplay 回呼。
- 面板滑鼠位置與拖曳捕捉不會被本模組當成移動／攻擊取消握手。未新增攔截遊戲 Raw Input 的 hook，因此是否觸發遊戲自身攻擊、游標解鎖与焦點行为仍須遊戲內確認；若點擊會攻擊，先停止姿勢再回報。
- 面板保存於 `%LOCALAPPDATA%/BetterEndfield/Interaction/overlay-settings.json`，跨模組版本保留。網頁讀取原生預設同步面板設定；網頁儲存仍使用 BEM 配置並更新面板副本。
- 保存副本記錄 BEM 基底配置；離線修改／匯入 BEM 配置後，舊副本不會覆蓋新配置。面板設定被覆寫前，可在網頁匯出 JSON 備份。
- 顯示布局、全螢幕、Alt 游標、點擊遊戲輸入等尚未實機驗證。沒有使用 Computer Use 或自動匯入 BEM。

# 0.13.0：女性管理員固定主控、小鍵盤 1～9

- 主控固定 chr_0003_endminf（女性管理員），預設互動對象為佩利卡，仍可另選其他隊伍角色。
- 九組互動預設使用小鍵盤 1～9；需要 Num Lock。已移除 F1～F8 輪詢，不攔截原有遊戲功能，上排數字不會啟動。
- 移除「啟用握手」及舊 F8 啟用入口。每個預設的「開始／停止互動」按鈕可代替小鍵盤，即使全域快捷鍵關閉也可使用。開始指令在遊戲回到前景後執行。
- 保留「停止並還原」作為停止任何姿勢並取消排隊的入口；保留校準模式讓切回面板調參數時維持握手，最長 120 秒。日常互動不用開啟校準模式。
- 舊 schema 1 的八組設定會轉為 schema 2 九組；原佩利卡＋女管理員組交換兩人的校準參數。舊版其他配對保留互動角色參數，主控參數重設為預設。第九組預設關閉，全域快捷鍵轉換後先關閉，需要確認校準並重新儲存／啟用。
- JSON 匯出 version 2，匯入接受 version 1／2，仍須完整驗證。主控與互動角色不可相同（頭部測試除外）。
- 使用：連接 → 更新隊伍清單 → 回遊戲等待 → 選互動角色 → 設定動作並保存目前角色與校準至槽位 → 開始／停止，或啟用／儲存小鍵盤快捷鍵。
- 離線測試涵蓋九組、女性管理員辨識、舊設定轉換與資源還原。新主控的實際手掌方向與握合仍需遊戲內驗證。本次未使用 Computer Use，未匯入 BEM。

# 0.12.0：八組互動預設、F1～F8 與 JSON 匯入／匯出

## 使用

1. 匯入新版 ZIP，重啟遊戲載入模組，連接診斷入口。
2. 更新隊伍角色清單，回 Gameplay 等待掃描。選取互動對象，調整握手校準。
3. 在 F1～F8 的欄位選擇動作，再按「保存目前角色與校準至此槽」。此按鈕保存該槽至 BEM。
4. 勾選全域快捷鍵開關，按「儲存全部預設與快捷鍵」。遊戲視窗前景時按指定鍵啟動，同鍵停止，另一鍵先還原再切換。
5. 「套用此預設」會排入 Gameplay 更新，套用完整角色資源 ID、動作模式與 42 項手部校準。之後按其快捷鍵；全域快捷鍵關閉／F8 未綁定時，也可按原本 F8 啟動。
6. 「匯出全部設定」輸出 JSON 到文字框，複製保存成 .json。可貼上 JSON，或以檔案選擇器讀入後按「匯入並套用所選預設」。匯入成功會保存八組及快捷鍵開關，回 Gameplay 套用所選組，不自動開始互動。JSON 必須包含八組完整預設，所選組必須啟用。

## 範圍與限制

- 預設保存主控資源 ID、目標資源 ID、動作模式、42 項校準值和啟用狀態；以資源 ID 重新找當前實例，不保存地址或隊伍槽位。
- 主控仍只支援佩利卡。其他互動角色須通過既有骨架與 Animator 驗證，手部外觀需各自校準。
- 動作識別為 handshake / standing / mutual / lookat / head。不是自訂 AnimationClip 或 FBX 匯入；來源動作資產已隨模組打包。
- F1～F8 可能同時觸發遊戲功能；本模組不攔截原有按鍵。全域快捷鍵預設關閉。
- 若 F8 槽位已綁定且全域開啟，F8 使用該預設；否則保留旧版手動啟用的 F8 行為。按住按鍵或帶著按鍵返回前景不會重複啟動。
- 校準模式是臨時明確啟用的選項，不隨預設匯入或重啟自動開啟。一般模式保留失焦停止與既有逾時規則。
- 修改／儲存預設不會改變正在執行的姿勢；下一次啟動或按「套用此預設」才使用新內容。
- 未使用 Computer Use；未自動匯入 BEM。原生與模擬面板測試通過，遊戲內完整快捷鍵與匯入效果仍待使用者驗證。

# 0.11.0：按鈕回饋與互動角色選擇

- 所有操作按鈕顯示處理中、防止重複點擊，並依原生業務回覆顯示成功／失敗；排隊、F8 待啟動與真正姿勢事件分開呈現。
- 連接後按「更新隊伍角色」，回 Gameplay 等待更新，再選取角色並按「套用互動角色」。重新啟用握手，回遊戲按 F8。
- 清單使用實際模型名稱，尚未加入角色名稱翻譯。主控目前仍限定佩利卡；互動對象可選其他已實例化隊伍角色，但必須通過既有骨架、Animator 與雙臂驗證。
- 以隊伍槽位及掃描取得的模型名稱重新驗證目標；切換主控、目標失效或骨架不相容會拒絕／停止。姿勢中不得更換對象。
- 清單更新只在既有 Gameplay 回呼讀取 Unity；面板不直接存取角色物件。
- 本版角色選擇不持久化；校準參數仍為單一設定，其他角色需另行校準。
- 已有離線測試；未使用 Computer Use，未匯入 BEM，其他角色的外觀效果仍需遊戲內確認。

# Live Grip Calibration 0.10.0

The user confirmed stable, jitter-free contact in 0.9.0, but incomplete visual grip. This release adds adjustment tools; it does not claim a finished calibrated hand pose.

Workflow: import/restart once, Connect, enable Calibration Mode, Enable Handshake, then return to Gameplay and press F8. Calibration Mode preserves an owned handshake when switching to the panel for up to 120 seconds of gameplay callbacks (130-second wall-clock ceiling). F8, movement while the game is focused, actor/target invalidation and lease loss still stop/restore. Unchecking Calibration Mode restores ordinary focus-loss and 20-second contact behavior. Ordinary mode does not retain background poses.

Each actor has 21 live fields: palm X/Y/Z offset in centimeters in its own body-facing frame (+X right, +Y up, +Z forward), calibrated hand-local X/Y/Z rotation in degrees, and three joint angles for each of five fingers. Offsets are limited to +/-8cm, wrist angles to +/-60 degrees, finger angles to -20..100 degrees. Values are interpolated before Unity writes. Edits require the next Gameplay callback; no rebuild or restart is needed after this version is loaded. Large combined settings can exceed reach and cause existing restore/cancellation; reduce values and restart F8 if needed.

For wrist rotation, the IK wrist target is recomputed using the actually rotated hand-to-palm proxy offset, preserving the requested hand contact relationship. Per-actor position fields intentionally permit separating/overlapping the two palm proxies for manual skin alignment; zero offsets retain coincident proxies. Finger controls use the previously calibrated local bend axes. Real skin contact, thumb opposition and finger collision constraints are not automatically solved.

The UI provides Relax Fingers, Reset Defaults, Load Saved Profile and Save Perlica + Female Endministrator Profile. Reset/relax change live values without overwriting the saved profile. Saving uses Better Endfield's documented readConfig/saveConfig bridge, so the module's configuration survives restart/package updates. Native initialize/configuration callbacks read a bounded, validated 42-number grip_values string; absent/invalid data keeps defaults or the previous valid profile. Calibration Mode is not persisted and starts off after module initialization.

Four offline tests pass, including valid profile roundtrip, malformed/out-of-range rejection, smoothing, live offset/wrist changes without rebinding, preservation of requested palm relationships and explicit calibration focus/timeout cleanup. Prior source, IK, gripping/restoration and metadata tests remain. The actual WebView bridge, background Gameplay updates and final appearance require runtime verification.

---

# Palm Orientation and Grip 0.9.0

The user reported that 0.8.2 visually achieved hand contact. This update preserves its measured shoulder placement, two-bone IK, contact clock and restoration behavior.

For each hand, the wrist-to-middle-finger-base direction and index-to-little-finger-base span define a calibrated palm plane. After aligning the hand longitudinal direction, a signed roll around that direction turns the palm normal toward the character's left side. The two facing characters therefore have opposing palm normals. This geometry convention and visible palm roll require runtime verification.

Each complete right thumb/index/middle/ring/little finger chain is checked as FingerN -> FingerN1 -> FingerN2 -> FingerNNub. Curl axes come from the measured segment direction crossed with the calibrated palm normal, transformed into each bone's local frame. The module does not assume a universal Euler bend axis. The prototype uses modest thumb angles and increasing curls toward the little finger, without finger contact collision solving.

Finger curl starts after 0.5 seconds of reaching and blends in over 0.6 seconds. Fingers uncurl during the last 0.5 seconds before arm withdrawal. All 30 finger controls, when available, retain fresh Animator restoration bases under the two existing pose leases. Missing/degenerate geometry skips that actor's curl as a whole while preserving working hand contact. Incomplete finger geometry is reported in the log. Stop, failed setters and ownership loss use the existing pair cleanup.

Four offline tests pass. Added checks cover opposing palm roll, curl bending toward the palm, complete 30-joint runtime binding, progressive curling, unchanged shared palm contact, full finger restoration and rollback after a finger setter failure. Existing short-arm, alternating Animator twist, source parsing and older pose tests remain. Runtime direction, grip amount, clipping and actual finger-to-hand contact remain unverified. This is a procedural grip prototype, not a complete authored handshake animation.

Close/restart Endfield after importing Interaction-Diagnostics-0.9.0-win-x64.zip. Control Perlica with female Endministrator nearby. Connect -> Enable Handshake -> return to Gameplay -> F8. Observe palm orientation and whether fingers curl inward rather than backward. F8 again or movement cancels; approximately 21 seconds including setup/withdrawal automatically restores. Return to Gameplay for cleanup before disabling.

---
# Adaptive Shoulder Alignment 0.8.2

Actual 0.8.1 logs repeatedly reported contact_unreachable and IK target outside arm reach. A fixed 0.85m root distance did not account for opposing right-shoulder lateral offsets or actual arm lengths. No rendered success was established for 0.8.1.

This update measures both right-arm lengths and shoulder positions before writing poses. It predicts shoulder offsets after the facing turns, shifts the partner anchor laterally to align right shoulders and uses 70% of the summed arm lengths for forward shoulder spacing (bounded 0.32-0.80m). Placement remains visual model-root control.

The shared contact solver now uses shoulder centers shifted by the measured palm proxy offset, with actual upper-arm + forearm reach; it no longer treats extra hand length as arm reach about the wrong center. Contact follows the two live centers with smoothing instead of caching one target through all idle shoulder motion. Full contact is held for 20 seconds to support visual investigation, then blends out/restores; F8/input/focus/target/ownership cancellation remain. No claim of in-game stability is made until runtime verification.

Measured arm lengths, requested shoulder spacing, unreachable geometry and post-write palm gaps are logged. Offline regression adds short arms and opposite shoulder offsets, ensuring adaptation avoids the earlier fixed-spacing failure. This is still a palm-proxy IK test without curled fingers or finished authored handshake motion.

---
# Handshake Contact Correction 0.8.1

User testing of 0.8.0 showed separated hands and visible jitter. Previous offline tests did not reproduce Unity hierarchy propagation or changing Animator twist. This patch adds hierarchical transform simulation and explicit contact/control regressions.

Close the game, import Interaction-Diagnostics-0.8.1-win-x64.zip, confirm 0.8.1 and restart. Control Perlica with female Endministrator nearby on level ground. Connect -> Enable Handshake -> return to Gameplay -> F8. A second F8 or movement/focus/target loss stops and restores. About six seconds from activation to automatic restoration.

Changes:
- Handshake-only visual root separation is 0.85m; ordinary standing preview remains 1.2m.
- Each right UpperArm -> Forearm -> Hand -> Finger2 chain must be unique and is checked at runtime. Arm axes, root-relative reference rotations and segment lengths are measured before any write. Fixed references prevent each new Animator twist from changing the aiming roll.
- Both arms use analytic two-bone IK toward a shared contact point with stable downward/backward elbow poles. Hand rotations aim the calibrated hand-to-Finger2 direction toward the other actor. The midpoint of Hand/Finger2 is only a palm proxy, not a measured skin contact point. Fingers are not curled.
- Both target sets are computed before writes, and all six local rotation writes participate in existing ownership-aware restoration. Fresh Animator bases are retained for cleanup but do not feed twist into the active solver.
- CMU trials remain loaded, but direct noisy arm-direction retargeting is replaced by contact IK. Their paired forearm-height signal contributes at most 1.5cm of shared vertical motion at half speed (60 source frames/second), blended in/out over 0.5 seconds. This is an IK contact prototype inspired by the source timing/motion, not complete authored CMU animation playback.
- Unreachable arm geometry stops/restores with contact_unreachable. No bone length or position is stretched. During full contact the diagnostic log records Hand/Finger2 proxy gap in meters every 0.5 seconds after writes. A small logged gap with continued visible jitter would suggest a later pose writer/frame-order issue and require runtime timing investigation.

Four offline tests pass, including real source parsing, IK length/reach checks, real transform hierarchy propagation in the simulator, both palm proxies meeting within 0.1mm, alternating Animator arm twists yielding the same output, six-bone cleanup, partner write failure rollback and automatic completion. These checks do not establish actual game ABI, visually correct palm orientation, finger contact, absence of later pose overwrites, or stable rendering. Those remain the user's runtime test.

The models still use visual root placement; colliders, gameplay movement, AI follow and physics are not coordinated. Existing stop-before-disable/shutdown callback limitations apply. Older notes below describe earlier versions and are superseded for handshake behavior by this section.

---

# CMU Handshake Preview 0.8.0

## Test workflow

Close Endfield before importing Interaction-Diagnostics-0.8.0-win-x64.zip as an update. Confirm 0.8.0 and enable it, then restart Gameplay controlling Perlica with female Endministrator instantiated nearby. Stand on open level ground 0.4-3m apart. Open the module page: Connect -> Enable Handshake -> return to Gameplay -> press/release F8.

The existing 1.2m visual standing anchors blend in over 0.6 seconds. The module then samples paired CMU trials 18_01 and 19_01 on one clock (303 frames; default 120 fps), aiming each character's right upper arm and forearm toward the source motion's body-relative directions. Blend-in is 0.25 seconds; blend-out is 0.35 seconds. It restores automatically after approximately 3.5 seconds. F8 again, WASD/jump/mouse input, focus loss, character/target change or lease loss stops/restores. Stop and return to Gameplay for cleanup before disabling the module.

This is an experimental RIGHT-ARM motion retarget preview, not full-body animation/PlayableGraph playback. Root translation and heading from the mocap are deliberately omitted in favor of the already tested model anchors. Legs, torso, wrist orientation, fingers, hand-contact IK and authoritative gameplay positions are not controlled. Exact contact, source/target axis convention, hand twist, playback speed and rendered stability require the user's game test. Do not interpret compilation/offline tests as in-game acceptance.

The source ASF/AMC files and CMU attribution/usage notes are bundled next to the DLL under motions/. Load errors disable this preview without preventing older modes. Actual runtime binding requires unique Bip001_R_UpperArm -> Bip001_R_Forearm -> Bip001_R_Hand chains on both body Animator roots, verified in prior scan logs and rechecked each session. Both pose leases are acquired before any mutation. Arm base poses follow fresh Animator updates, avoiding accumulated offsets; cleanup restores still-owned writes before releasing the two leases. Existing shutdown-without-Gameplay-callback limitations apply.

Validation: four offline tests pass. Source tests load both real 303-frame clips and check finite/changing trajectories and opposite-vector aiming. Runtime simulation exercises missing assets/mapping, both leases, both arm outputs, four-bone restoration, partner setter failure rollback and automatic completion in addition to prior mode regressions. Simulation does not model Unity's actual transform hierarchy or confirm native ABI/render behavior.

Format references: https://mocap.cs.cmu.edu/info.php and https://graphics.cs.cmu.edu/nsp/course/cs229/info/Acclaim_Skeleton_Format.html . The parser supports the bundled XYZ-axis, rotational-DOF, full-specified degree files; this is not a general ASF importer.

---
# Interaction Diagnostics / Follow Interface Research 0.7.0

## Follow-control research pass

The user confirmed 0.6.1 restored normal preview operation. Source inspection has not established a reversible squad-follow pause/resume mechanism. This version adds a read-only research pass, not an unverified freeze/teleport implementation. Prior preview/LookAt behavior remains available.

With the game closed import Interaction-Diagnostics-0.7.0-win-x64.zip, confirm 0.7.0 and restart. In Gameplay control Perlica with female Endministrator instantiated in the squad. Open the module page, Connect -> Research Squad Follow Interfaces, return to Gameplay briefly for the callback, then check follow_research_complete. Full output is in Interaction.Diagnostics.log beside the native DLL. Requesting research stops/restores an active preview first.

The pass uses exported IL2CPP metadata APIs (their names were verified in this client's GameAssembly.dll) to enumerate the actual partner Entity, MovementComponent, CharacterController and SquadManager methods and field signatures. Existing known read-only Entity movement/controller getters are invoked using Host descriptors. Discovered methods are never invoked; field values, raw offsets, native addresses, login tokens and account data are not dumped. Missing exports/objects are reported. Enumeration has a 1500-member overall budget, bounded parameters/parent depth and per-class method/field counts; limit messages indicate incomplete output.

The implementation uses the same dynamic-export approach as upstream Host; type-name allocations are freed through the exported IL2CPP allocator. Existing hook, game-thread, GC pin and lifecycle handling are reused. Three offline tests pass (math, simulated runtime and metadata enumeration/bounds/name cleanup). Actual metadata retrieval and candidate follow-control semantics require the next runtime log. No AI pause/resume capability is claimed from method names alone.

## 0.6.1 regression fix — supersedes 0.6.0 gating below

The user's real 0.6.0 log repeatedly reported moving=true for both actors and refused every standing attempt. The getter's meaning as actual displacement was not verified; treating it as a stationary predicate was incorrect. 0.6.1 removes movement/airborne/raw-mode gating and makes these getters optional diagnostics only. Missing diagnostic components no longer disable placement. Cancellation returns to the proven 0.5.0 keyboard/mouse, focus, lifecycle, ownership and timeout checks. This release does not claim automatic cancellation from an NPC's actual movement or gamepad input.

Close game, update with Interaction-Diagnostics-0.6.1-win-x64.zip, confirm 0.6.1 and restart. Use the same Connect -> Enable Standing Preview -> F8 workflow. The observed true/true flags now allow placement, verified by regression simulation; actual fixed-client behavior still needs confirmation. Restoration remains tested. Still a visual model preview with no authoritative gameplay positioning or collision integration.

## Gameplay-state update

Import Interaction-Diagnostics-0.6.0-win-x64.zip with the game closed, confirm 0.6.0 and restart. Connect, then use Check Both Movement States if needed; return to Gameplay for its queued callback. The UI event reports controlled/partner readiness, moving/airborne booleans and raw move_mode. ready must be true for both and moving/airborne false before standing preview starts. This report reads state only. If the reply stays queued while out of game, return to the game, wait a frame and reopen the page.

0.6.0 reads source-backed Entity.IsValid/get_movementComponent and MovementComponent.get_isMovingOnGround/get_isInAir/get_moveMode descriptors, referenced by upstream Actions. Both actors must have readable nonmoving/nonairborne state before placement. Every placement callback rechecks them; detected gameplay movement/airborne or unreadable state stops the preview and restores the roots/heads. This catches actual actor motion beyond the existing keyboard/mouse polling; gamepad behavior still needs real-client verification. Raw mode values are diagnostic only; no guessed enum-number gating is used.

The user confirmed 0.5.0 visual placement works and cancels on movement. 0.6.0 compiles and passes regression plus simulated-runtime checks for moving partner refusal, gameplay-state movement cancellation without key polling, partner airborne cancellation and missing-component refusal. Real state resolution and stationary flags remain pending in-game. No authoritative gameplay teleport/collider integration was found verified in the inspected sources, so this release remains a model preview. It does not stop AI, freeze locomotion or change authentication/security/game files.

## Standing preview update

Close the game and import Interaction-Diagnostics-0.5.0-win-x64.zip as an update, confirm version 0.5.0 and enable. Restart Gameplay controlling Perlica; bring female Endministrator to level open ground 0.4-3m away, and remain idle with keyboard/mouse. Open the module page, Connect -> Enable Standing Preview, return to game and press/release F8. The controlled model stays at its starting position; both model roots turn to face each other, and the partner model eases to 1.2m separation along their initial horizontal separation direction over 0.6 seconds. Mutual LookAt then runs on the placed models. F8 again, ten seconds, focus/character/target loss or detected WASD/jump/mouse-button input stops/restores.

This is VISUAL MODEL-ROOT PLACEMENT, not a verified gameplay movement/teleport API. Colliders, gameplay entity positions, AI/navigation, root motion and camera following are not coordinated. Do not interpret the models' temporary positions as actual authoritative character locations. Gamepad input and physical interaction with terrain are unverified; use stationary keyboard/mouse tests. Floor-height difference >0.25m, pair distance outside 0.4-3m or invalid transforms are refused. No raycast/collision clearance is claimed. Avoid slopes, edges, objects and combat.

Both roots share the two independently acquired pose leases from mutual mode. Placement reads current local position/rotation before writing world anchors and records its own resulting local writes. When game updates replace a prior module write, the new game pose becomes the restore base. Stop restores only local components still matching our own writes and still owned by us, avoiding obsolete world-space teleports after parent motion. Roots restore before head cleanup; all ownership/handles are then released. Shutdown without a Gameplay callback can still leave restoration unverified and must be handled by stopping first as below.

0.4.0 reciprocal LookAt was confirmed working by the user. 0.5.0 builds and passes offline math and simulated-runtime checks for anchors, facing, interpolation endpoints, bounded distances/heights, two-root restore and partner placement failure rollback. Placement stability, render/physics separation and normal gameplay resumption are still pending real-client tests. Existing head and LookAt modes remain selectable.

## Mutual LookAt update

Close the game and import Interaction-Diagnostics-0.4.0-win-x64.zip as an update, confirm 0.4.0 and enable it. Restart Gameplay controlling Perlica, with female Endministrator instantiated in the squad. Place the two roughly facing each other within 0.2-20m. Open the module page, select Connect -> Enable Mutual LookAt, return to game and press/release F8. Both heads steer toward each other's head; F8 again stops/restores both. Thirty-second timeout and focus/character/target cleanup remain enabled. Single-direction LookAt and the original head test remain separately selectable modes.

Both models require unique body-root Animators and observed Head/Neck chains. Two independent pose leases are acquired before any write. If the second acquisition fails, the first is released with no pose changes. Each frame computes both smoothed outputs before either write to avoid order-dependent target positions. If the second setter fails, the first successful write is restored and both leases/handles are released. Each head preserves its own fresh animated base, and restoration does not overwrite an outsider's pose after ownership loss.

This version only modifies two head bones, not body facing, positioning, eyes, neck/spine or animation clips. The turn limits still apply; characters with bodies facing away cannot achieve unrestricted mutual gaze. New reciprocal control and manager-bone orientation remain unverified in game. The user confirmed single-direction 0.3.0 LookAt works. Offline math/simulated-runtime checks pass, including dual ownership, independent body frames, 600-frame convergence, two-sided restoration, partner conflict rollback, setter failure, removal and ownership loss.

The shutdown-without-callback limitation below applies independently to both heads. Stop/restore in Gameplay before disabling a loaded module.

## LookAt update

Close the game and import Interaction-Diagnostics-0.3.0-win-x64.zip as an update of the same module ID. Verify 0.3.0, enable and restart. In Gameplay control Perlica, with female Endministrator instantiated in the squad. Open the module page, select Connect -> Enable LookAt, return to the game, then press/release F8. A second F8 restores; focus loss/character change/target removal also stops, and LookAt times out after 30 seconds. The old 20-degree head test remains available as a separate mode; stop the current pose before changing modes.

The target identity is deliberately limited to the observed chr_0003_endminf_postmodel( prefix; male Endministrator and Laevatain remain unverified. It searches for the target's unique Head under Neck, rechecks squad membership/model and reads world head positions each frame. Invalid/coincident/near-vertical directions and targets outside 0.2-20m stop without further pose changes. Desired yaw/pitch are computed in the controlled model's facing frame, constrained to +/-55 and +/-20 degrees, exponentially smoothed and converted into the head parent's frame before offsetting the Animator's current head rotation.

This is bounded head steering toward the other character, not eye IK or an exact gaze solver. Bone orientation, root facing and target convergence need visual verification in this game. Start with the target at front/side-front. No neck/spine/body/root-motion or facial modifications are included.

0.2.0 F8 rotation/restoration and walking stability were confirmed by the user. 0.3.0 builds and passes the extended offline tests, including body-frame conversion, smoothing, clamps, moving targets, target removal and useful business-error transport. LookAt itself has not been verified in game.

## Original head-test workflow and restoration

Windows x64 package; native code builds independently of the .NET UI. Diagnostic scanning is read-only. This release adds an explicitly armed F8 head-rotation test.

Close the game, import Interaction-Diagnostics-0.2.0-win-x64.zip on the Third-party Modules page as an update of the same module ID, verify version 0.2.0 and enable it. Restart Gameplay and control Perlica. Open the module page, Connect, then Enable F8 Test; expect armed_press_F8_in_game. Return to the game, press/release F8 once to apply a local head Y offset of 20 degrees, again to stop. Begin idle, then compare walking. Visual direction and stability remain unverified.

The test automatically stops after ten seconds, on focus loss, character/model change or pose ownership loss. These checks require a Gameplay TailLateTick; a paused process cannot execute cleanup. Alt-Tab out will stop an active pose test on the next callback.

Before disabling a running module, request Stop and Restore, return to Gameplay for a tick, then check pose_status off. During shutdown the module briefly waits for a Gameplay callback; if none arrives it releases ownership/GC handles and reports restoration unverified rather than calling Unity from the Host worker. Normal Animator resumption is expected to restore its own pose but remains unverified in-game.

Only the observed Perlica model prefix chr_0004_pelica_postmodel( is eligible. One Animator must share the model-root Transform, excluding weapon Animators; one Bip001_Head directly under Bip001_Neck must exist in the bounded hierarchy search. PoseLease v1 is required. Busy/ambiguous/unavailable targets are rejected. Keep MMD/Actions inactive during this test.

Each frame applies the offset to the current animation pose. If the head still equals our prior write, the previous unmodified base is reused, avoiding accumulated rotation when animation is culled. Stopping restores that base only when the head still equals our write and we retain ownership. Fresh animation poses are not overwritten with an old activation snapshot.

The Host may reject connection with result 5 (Conflict) because its built-in Camera module already owns CameraManager.TailLateTick. Turning off individual camera effects may not unload that module. Stop and collect the module status/log; do not alter hooks or security settings to force connection. A compatible scheduling entry is required before this package can scan in that configuration.

The scan acknowledgement means queued, not completed. A scan runs on a subsequent Gameplay TailLateTick. Full results append to Interaction.Diagnostics.log beside the imported DLL. Look for `Scan complete` or the explicit missing-object/contract message. No output can be treated as character identification until correlated with which character you are controlling.

0.1.0 connected and captured Perlica's skeleton successfully in the user's Gameplay session. 0.2.0 compiles and passes quaternion and simulated-runtime tests: 600-frame non-accumulation, animation-aware restoration, invalid inputs, wrong character, weapon Animator exclusion, ambiguous bones, pose conflict, setter failure and resource cleanup. These do not verify game ABI, scene transitions or visible F8 behavior. IK, animation playback and facial control are absent.



# 0.30.1 移除雙腳固定

校準選單移除雙腳固定與腿部 IK，保留不固定、左／右腳、左／右膝。舊槽位與校準 JSON 的 `both_feet` 讀入時改成 `none`，保留其餘參數；保存與匯出不再產生雙腳模式。

# 0.30.0 校準支撐模式

Num0 面板與網頁校準區新增「支撐模式」選單：不固定、左腳固定、右腳固定、雙腳固定、左膝固定、右膝固定。目前作用於互動角色的匯入動畫，主控與內建握手等動作不套用此層。

使用方式：角色正常站立時選擇動畫與支撐模式 → 播放／暫停校準 → 準備匯出到互動預設 → 套用槽位並保存。校準 JSON 與插槽保存 `support_mode`，快捷鍵重播會套用各槽位自己的模式；舊設定缺少此欄位時使用不固定。動畫片段保存／匯出仍保存原始動畫，支撐模式屬於校準設定，沒有烘焙進 FBX／動畫 JSON。

腳部模式記錄播放前腳踝接觸高度，在動畫與骨架校準後補償互動角色 root；雙腳模式額外修正大腿、小腿與腳部朝向，保持兩腳接觸。膝蓋模式以第一個評估姿勢的膝蓋位置為支點，適合已校準好的跪姿，不會自動把站姿膝蓋壓到地上。播放中更換模式會在 Gameplay 更新重新建立支點；需要一致起點時，選好模式後停止再重播。

互動角色 root 旋轉繞支點作用，XYZ 位移仍可移動整組支點；主控旋轉仍帶動整個雙人場景。時間軸定位保留支點，逐幀先移除自己的 IK 層再重新採樣，不累加前一幀修正。停止與寫入失敗沿用姿勢所有權及還原流程。缺少腳／腿階層或雙腳目標超出腿長時會提示並拒絕／停止，不能拉長骨架冒充固定。

此版不做地形射線偵測、自動腳底厚度估計或動畫接觸區間辨識；先在平地正常站立時啟動，用 root Y 校準鞋底高度。跳躍／抬腳動畫選不固定。網頁測試與 8 組 CTest 通過，新增涵蓋所有模式保存／讀取、舊設定相容、固定支點、120 次非累積更新、支點旋轉、時間軸定位、切換解除、缺少骨架及部分寫入失敗還原。尚未在遊戲內驗證外觀。

# 0.29.5 匯入動畫直接定位

匯入動畫播放時，第一個 Gameplay 更新直接套用主控與配角的校準站位、朝向及動畫姿勢。移除原本 0.6 秒站位滑行、等待站位完成才推進時間的延遲，以及起始姿勢淡入。動畫檔本身的動作內容保留，不需重新匯入；停止仍使用原有還原機制。

新增模擬回歸確認第一幀直接到位、套用動畫旋轉與位置，且時間立即推進。編譯與 8 組 CTest 通過；尚未在遊戲內驗證外觀。EFMI 即時網格項目仍擱置。

# 0.18.1 FBX 身體骨架檢查修正

FBX 身體動畫轉換只檢查 Bip001 身體骨架及其父節點，排除未使用的眼睛、睫毛與其他附件骨架。修正佩利卡睫毛非等比例縮放造成整份 FBX 匯入被拒絕的問題；身體骨架本身的縮放限制仍保留。已用實際佩利卡骨架參考完成原始 FBX 轉換並通過動畫格式驗證，尚未在遊戲中驗證動作外觀。

# 0.19.0 FBX 匯入時選擇管理員預設動作

在網頁「動畫匯入」或 F8 動畫匯入頁先選管理員預設動作，再導入 FBX：

- 不添加動作：保留原本單人 FBX 行為。
- 保持站姿：擷取管理員開始播放時的站姿旋轉並保持，配角繼續播放 FBX。
- 保持站姿＋輕摟腰部：兩人並肩、朝向一致，約 0.43 公尺間距；管理員右手掌以手臂 IK 追蹤配角腰部左側。

這個選擇保存為動畫的 manager_action，並用不同動畫 ID 保存各個版本，避免覆蓋同一 FBX 的其他管理員動作版本。動畫列表名稱顯示管理員動作，校準及小鍵盤預設引用此動畫時會一起播放。既有動畫維持不添加動作；要加入新預設動作請重新匯入 FBX。

開始前請讓管理員正常站立，停止目前動畫，再選互動角色及匯入選項。站姿保留原有位置資料、只保持骨架旋轉；右手輕摟以程序式 IK 補上，不是新增 Blender 雙人動作。手指保留開始時的姿勢。大幅跳躍、翻身或位移可能使腰部超出手臂範圍，這時限制手臂伸展而不拉長骨架，不保證繼續接觸。停止、移動取消及寫入失敗使用原有骨架所有權與還原機制。

已完成 C++ 編譯、四組 CTest、網頁選項傳遞測試；新增模擬測試涵蓋站姿保持、配角動畫共存、腰部移動追蹤、近距離站位、缺少骨骼拒絕及還原。尚未驗證遊戲內手掌方向與衣服表面接觸，沒有使用 Computer Use。

# 0.19.1 FBX 雙人佈置以管理員為基準

修正 FBX root XYZ 校準隨管理員位置／朝向、配角初始站位而改變的問題。FBX 播放開始時固定管理員位置與朝向為共同參考：一般動畫的配角站在管理員前方、面向管理員；輕摟預設維持並肩、朝向一致。配角起始位置僅作距離及平地檢查，不再決定 FBX 佈置的參考朝向。

FBX 的 `0|@root` 與 `1|@root` 位移／旋轉在站位階段套用，先於動畫與管理員腰部 IK。主控 root 位移移動整個佈置，旋轉帶動配角相對位置；配角 root XYZ 是沿管理員左右、上下、前後的額外偏移（cm）。縮放及其他骨骼維持原有局部設定。每幀從固定基準推算，不疊加上一幀的結果。

不用重新導入 FBX。舊 root 設定的座標含義改變，請視需要重新校準、準備匯出並更新槽位。移動仍會取消互動；走到新位置後重新播放，會用新的管理員位置與朝向重建相同佈置。大幅 FBX 骨盆位移仍屬動畫內容，root 校準不會把它消除。

新增回歸測試：平移與 90 度轉向後重播、配角初始站在不同方向、即時主控 root 調整帶動配角、root 只套用一次及停止還原。C++ 編譯、四组 CTest 與網頁測試通過；尚未遊戲內驗證，沒有使用 Computer Use。

# 0.19.2 固定動畫時間校準與管理員手腕限制

校準模式增加「暫停動畫校準」。播放 FBX 後勾選，固定目前 FBX 時間與採樣姿勢，root 佈置、骨架偏移及管理員腰部 IK 仍繼續即時更新；取消勾選接續播放。F8 面板提供相同的暫停／繼續按鈕。新播放不沿用暫停狀態；暫停不是保存到動畫或槽位的設定。

避免以不同動畫時間點比較 Y 軸校準。FBX 的骨盆位移仍是動畫內容，不會被 root 偏移自動消除。先暫停在同一姿勢，再比較 root Y=83 與 89，預期只有六公分差異。沒有證實遊戲內跳動只由動畫時間造成；此功能能隔離這項變因。使用者既有校準參數保留，不自動重設。

管理員輕摟的手腕 IK 增加相對開始姿勢的 65 度旋轉限制，避免為追蹤手掌方向而產生極端彎折。這不重寫配角 FBX 的手部或手指軌道，也不保證衣服接觸外觀。尚未遊戲內驗證。

編譯、四組 CTest 及網頁测试通過。新增測試涵蓋：固定 FBX 時間時 Y 83→89 恰差 6 cm、持續更新不累加、暫停時 IK 仍追蹤、恢復動畫時間及管理員手腕旋轉上限。校準播放仍有 130 秒時間上限；移動取消及停止還原維持原有機制。沒有使用 Computer Use。

# 0.19.3 滑桿拖曳保護與腰部 IK 方向修正

F8 骨架滑桿按下時固定動作、骨架 key、參數欄位、起始值與滑軌範圍，拖曳改用相對起點的位移。畫面定時重建、列位置變動或尺寸變動不再重新決定拖曳目標。點下滑轨不直接跳至絕對值，移動滑鼠才相對調整，以便精細校準。網頁原有數字輸入及滑桿仍可使用。

兩個入口的 root 參數修改新增輸入紀錄及完整 Gameplay callback 後的位置讀回。日誌 `Root input` 記錄 key、欄位、數值與修訂；`Root applied` 記錄預期／實際／更新前的 root 世界位置、動畫時間及可取得的配角骨盆與腳部位置。網頁骨架狀態顯示 root Y 與套用誤差。這些記錄用於判別輸入、root 與骨盆動畫層，沒有在遊戲中確認使用者的跳位已消失。

管理員輕摟改以配角當下腰與脊椎方向構造接觸位置／手掌方向，不再固定使用模型 root 軸。上臂及前臂以骨骼方向與彎曲平面雙軸對齊，避免單軸最短旋轉帶來的 roll。手肘平面保持連續；手臂旋轉增加每秒 240 度變化上限，手腕保留相對開始姿勢 65 度限制。保留停止與失敗清理機制。這些修正針對管理員程序式摟腰，不重寫配角 FBX 軌道。

不需重新匯入 FBX，保留使用者的 root 校準值。C++ 編譯、四組 CTest 與網頁測試通過；新增完整 transport + PumpPose 的 83.0 到 89.0 拖曳測試、固定拖曳目標、手肘平面奇異方向連續性、配角脊椎傾斜時手腕不突然翻轉。尚未遊戲內確認接觸外觀，沒有使用 Computer Use。










# 0.31.0 UI 與自動校準

- 導入動畫增加「選擇 Blender 資料夾」，可指定含 blender.exe 的安裝資料夾或 Blender Foundation 資料夾。有效位置保存至本機設定，重啟後沿用；無效位置不覆蓋既有設定。
- 管理員姿勢標示為測試功能；連接遊戲入口移到主控上方。
- 移除校準啟用開關。校準區播放與時間軸操作自動進入校準，停止還原後退出。
- 選擇匯入動畫即顯示該動畫時間、區間與標記，連接成功時在 Gameplay 回呼建立第一幀暫停預覽；切換動畫不沿用前一段時間。未連接時可先查看資訊。
- 循環校準動畫放在支撐下、播放上。保存動畫與另存動畫片段同時只在所選動畫已進入預覽時顯示。保存加入模組動畫庫；另存將所選區間寫到使用者指定 JSON，不新增動畫庫項目。
- 匯出設定改稱另存參數設定；還原骨架預設與人物根節點分類同層縮排。
- 即時網格調整標示未完成；EFMI 即時功能仍擱置。

原生與網頁同步更新。套件編譯與自動測試通過，遊戲內畫面仍需實測。

# 0.31.1 Blender 路徑與快捷鍵切換

導入區顯示實際解析到的 Blender 資料夾（包含自動搜尋結果）；未找到時提示選擇資料夾。播放快捷鍵預設時，按不同的已設定快捷鍵會還原目前動畫並啟動下一個；按目前快捷鍵仍為停止。

## GitHub 發布流程設定（2026-10-07，模組版本不變）

- git-release 新增 GitHub Actions：每次分支 push 封裝現有執行檔，預設分支的新版本自動建立 Release。
- ZIP 名稱及 Release 標籤取自 module.json；預發布後綴保留。同版本 Release 不覆蓋。
- 新增獨立封裝腳本，ZIP 根目錄直接放 module.json；排除開發設定、根 README、文件、外部模型與暫存，保留動作資料與來源說明。
- 本次只設定發布工具，沒有變更模組功能、DLL 或模組版本。GitHub 尚未連接，雲端執行待首次 push 驗證。
- 本機驗證：ZIP 生成成功、CRC 與 module.json 版本檢查通過；14 個執行資源檔案的目錄結構已確認。重複輸出同名 ZIP 被拒絕，舊產物保留。環境未提供 YAML 解析套件，工作流程未經自動 YAML 解析；GitHub 雲端發布與遊戲內載入尚未驗證。

### 封裝範圍調整（2026-10-07，模組版本不變）

- 按使用者指定，ZIP 根目錄僅含 examples、native、tools、ui 與 module.json；四個資料夾遞迴包含執行資源，沿用暫存與私人 FBX 排除規則。
- 根目錄 NLOHMANN-LICENSE.MIT 不再加入 ZIP；模組 DLL 與版本未變更。
- 本機封裝與 CRC 檢查通過，確認 ZIP 根目錄恰好五個指定項目。驗證包另存 artifacts/github-package-five-only，未覆蓋舊 ZIP。GitHub 雲端執行仍待 push 驗證。
