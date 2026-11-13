# 2. 操作をコードで追う

この章はCustomizeFlowController.cppを開きながら読む。操作を「入力→編集用データ→表示→保存」に分解する。

## 2.1 技を選んで編集を始める

入口の専用UIはCustomizeMenuView。UpdateはInputFrame、画面サイズ、選択番号への参照を受け取り、選択番号を変更し、押された項目番号を返す。Drawは現在の選択を受けて色を描き分ける。遷移先のSceneやJSONを扱わない。

Flow::UpdateCreationMenuはUIの要求とInputSystemのMove/Submitをまとめる。技作成ならAttackCategorySelectへ移る。UpdateAttackCategorySelectはカテゴリを確定し、RefreshAttackSlotSummariesで名前を更新してAttackSlotSelectへ移る。

スロット一覧には現在まだImGuiが残る。そこから呼ぶSelectAttackSlotの実行順は次のとおり。

1. `editHistory.Reset()`で別の技のUndoを持ち込まない。
2. 汎用編集フラグを解除し、カテゴリを記録する。
3. `attackEditor.SelectSlot(category, slotIndex)`へ読込を委託する。
4. ViewをResetし、前の技のフォーカス等を消す。
5. MotionEditorをResetForAttackMotionする。
6. Previewを0F・停止状態へ戻す。
7. modeをAttackEditorへ変更する。

SelectSlotはAttackDataをファイルからdraftへ読み、未保存スロット等では新しい値を準備する。戻り値の文字列は処理結果を示すメッセージであり、技データそのものではない。

## 2.2 パラメータを変更する

編集画面の更新入口はUpdateAttackEditorControls。

```text
CaptureEditSnapshot（変更前）
  ↓
attackEditorView.Update(attackEditor, preview, サイズ, メッセージ,
                        motionEditor, 画面種別, 汎用フラグ, Undo/Redo可否)
  ↓
CaptureEditSnapshot（変更後）
  ↓
差分を履歴へ通知
  ↓
返されたCustomizeAttackEditorActionをFlowが実行
```

View::Updateの返すActionは保存データではない。Save、Back、OpenMotionEditor、Undo等の「次に行う操作」の列挙値である。一方、数値やキーの変更は渡された編集担当を介して下書きへ既に反映される。

数値変更と保存要求を分けることで、ボタンを押すたびにJSONを書かずプレビューを更新できる。これを編集用draft方式と呼べる。

### 硬直差は保存値と同じではない

AttackEditorのRefreshFrameAdvantageInputsとApplyFrameAdvantageInputsToDraftを対で読む。前者は保存用スタンからUIの硬直差へ、後者はUIの硬直差からスタンへ変換する担当である。画面に表示された整数がそのままJSONの同名フィールドへ入るとは限らない。

同様にNormalizeDraftForSelectedCategoryは通常技の地上/空中分類、必殺技以外のコマンド、空中技のリアクション等を整える。「UIが表示を禁止する」ことと「データ自体を正規化する」ことは別の防御である。

## 2.3 表示フレームを変更する

プレビューの0は待機。GetPreviewActionFrameはこれを-1へ、プレビュー1以降を内部の0始まりへ変換する。

例: プレビュー6 → 内部5。キーフレームの保存先へプレビュー6を直接渡すと1Fずれる。

GetPreviewTotalFramesは攻撃ならGetAttackTotalFrames、汎用ならMotionData.totalFramesから範囲を取得する。ClampPreviewCurrentFrameは現在の表示位置をその範囲へ収める。

再生中はUpdatePreviewPlaybackがPreview::UpdatePlaybackへ依頼する。返されたboolは「進んだか」。進んだ場合だけ新しい内部フレームに対応する編集表示値と攻撃移動オフセットを取り直す。

技の総フレームを変えた場合、タイムライン表示だけでなくキーの有効範囲やキャンセル範囲にも影響する。各正規化処理を合わせて確認する。

## 2.4 ポーズキーを追加して回転する

MotionEditor::AddWholeBodyKeyframeは内部フレームと総フレームを受け、全身姿勢のキーを追加する。HasMotionKeyframeで現在のフレームに編集できるキーがあるか判定する。

UI上は全身単位でも、モーション内部では各部位のトラックへ回転キーを設定する処理がある。全身キーという編集ルールと、内部配列の格納方式は分けて理解する。

回転ギズモの流れ:

1. Flow::RenderAttackPreviewがキーの存在・停止状態・編集モードからギズモの有効条件を作る。
2. Preview::Render内のDrawRotationGizmoがImGuizmoを使って操作を処理する。
3. 開始姿勢とドラッグの軸・回転量を保持し、ローカルQuaternionを作る。
4. Previewの姿勢を更新し、スキニング行列を再計算して表示する。
5. FlowがConsumeGizmoRotationEditへ出力用の部位番号・軸番号・Quaternionを渡す。
6. 結果があればMotionEditor::ApplyGizmoRotationEditへ内部フレームと総フレームとともに渡す。
7. MotionEditorがdraftの回転キーへ反映する。

Consume型関数の戻り値boolと出力引数を区別する。boolは結果の有無で、姿勢は参照引数に書かれる。消費済み結果を毎フレーム再適用しないための受渡しである。

KeyframeEditing.hのFindOrInsertはキーを時刻順に扱うための共通処理。キー配列を末尾追加したままにすると、サンプラーが時刻順を前提として早期終了し、保存前だけ姿勢が変わらない等の不整合になる。

## 2.5 IKで手を移動する

位置編集は関節を切り離して平行移動する保存方式ではない。

```text
ワールド基準の目標位置
 → 編集開始時の姿勢・チェーンを使ってIK計算
 → 関節の回転結果
 → 現在フレームの姿勢キー
```

BeginIkEditは開始フレームとPreviewを受け、セッションを開始できたかboolを返す。UpdateIkEditは同じフレームとワールド目標位置を受け、結果を回転へ反映する。EndIkEditは開始姿勢を解放する。

移動ギズモではImGuizmo::WORLDを指定する。矢印が関節の回転に追従するローカル軸ではなく、ワールドXYZと平行になる指定である。UIのIKとギズモのIKで別の保存形式を作らない。

ソルバーの評価には、目標までの誤差だけでなく、骨の長さ、可動域、固定する部位、反復終了条件を読む必要がある。本資料はソルバーの数値的正しさを証明するものではない。次章の関連ファイルへ進む。

## 2.6 攻撃移動キーと汎用オフセット

ポーズのIK位置と「全身の移動キー」は別物。

|種類|保存先|意味|
|---|---|---|
|関節IK目標|編集セッションの一時状態|回転を作るための入力。位置そのものを関節キーとして保存しない|
|攻撃移動キー|AttackDataのmovementKeys|対戦中にオブジェクトの移動へ利用する前後・上下の情報|
|汎用モーションのオフセット|MotionDataのrootOffsetKeys|表示用の全身位置補正|

Flow::RenderAttackPreviewはmovementModeのときrootMovementGizmoEnabledを設定する。Previewで位置が変わったらrootMovementChangedを消費し、攻撃ならUpdateAttackMovementKeyframe、汎用ならSetRootOffsetKeyframeへ振り分ける。

攻撃の移動は見た目だけをずらす汎用オフセットと同じにしてはいけない。対戦では判定も含めた位置制御へ接続する必要がある。

## 2.7 保存ボタンを押す

両編集画面のSaveはFlow::SaveDraftMotionへ集まる。

1. draftがなければロードする。
2. EnsureDraftAttackMotionDataIdでスロット用のIDを整える。
3. MotionEditor::SaveDraftへID・総フレーム・ループ指定を渡す。
4. 文字列が`Saved MotionData:`で始まらなければ中断する。
5. 攻撃編集ならAttackEditor::SaveDraftも呼ぶ。
6. こちらが`Saved:`で始まるか確認し、両方成功したときだけ完了とする。

**2ファイルの保存は原子的ではない。** MotionDataの保存後にAttackDataが失敗する可能性があり、コードにも部分成功のメッセージがある。成功判定がboolでなく文字列の接頭辞で結び付いている点も、変更時の依存である。

EnsureDraftMotionDataIdには、古いIDからスロット固有IDへ内容をコピーする処理とキャッシュのUnloadAllがある。したがって「Saveボタン以外では絶対にディスクを書かない」という説明は正しくない。旧データの移行経路を別に確認する。

## 2.8 Undoを押す

CaptureEditSnapshotはAttackの編集状態、Motionのdraft、表示名、汎用フラグ、現在のプレビューフレームを複製する。カメラそのものを保存する処理ではない。

Observeは変更前後と操作ID・継続中フラグ・ホイール操作・UI時刻を受ける。同じドラッグを毎フレーム別操作へ分けず、一つの操作にまとめるための情報である。

Undo/Redoのboolは復元対象があるかを返し、復元内容は出力スナップショットへ書く。RestoreEditSnapshotは再生・ギズモ・IKを止めてからAttackとMotionを戻し、表示値を同期する。最後にViewも再構成して、古いUI参照を次の描画へ残さない。

Undoはメモリ上の編集を戻す。保存したJSONまで自動的に巻き戻すわけではない。
