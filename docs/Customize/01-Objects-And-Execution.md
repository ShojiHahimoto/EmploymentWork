# 1. オブジェクトと実行の仕組み

## 1.1 「オブジェクト指向」の実体

この部分で重要なのは継承の数ではない。データと、そのデータの寿命を管理する担当をひとまとまりにし、別の担当へ仕事を委託することである。

CustomizeSceneは「技名を書き換える」方法を知らない。Scene::RunSystemsからFlow::RunSystemsを呼ぶ。Flowは現在の内部画面を知り、適切な編集担当を呼ぶ。これが委譲である。

```text
CustomizeScene
  owns World
  owns CustomizeFlowController
         references World （Worldをもう一つ作るわけではない）
         owns CustomizeAttackEditorController
         owns CustomizeMotionEditorController
         owns CustomizeCharacterEditorController
         owns CustomizePreviewController
         owns CustomizeAttackEditorView
         owns CustomizeMenuView（入口用とカテゴリ用）
         owns CustomizeEditHistory
```

`owns`はメンバーとして保持している関係、`references`は既存インスタンスへの参照である。World&を受け取ってもWorldの内容がコピーされるわけではない。

GameObjectは別の概念である。World内でIDとComponentを持つゲーム内のデータであり、Controller自体をWorldへ登録して更新しているわけではない。専用UIのボタンも対戦用GameObjectではない。

## 1.2 各担当が保持するもの

### CustomizeFlowController

内部画面のmode、カテゴリ、編集が汎用か攻撃か、状態メッセージ、各編集担当を保持する。ファイル保存を複数担当へ依頼する順序や、戻る先を決める。

Flowの役割は「交通整理」に近いが、現在はそれだけではない。スナップショットの作成、プレビューとの編集結果受渡しも行う。この実態を無視して「Flowには処理がない」と読んではいけない。

### CustomizeAttackEditorController

AttackDataのdraft、スロット情報、文字入力用バッファ、硬直差の編集値を管理する。データのカテゴリ制約を整える責務を持つ。Viewが地上通常技を空中技へ勝手に変えてよいわけではない。

### CustomizeMotionEditorController

MotionDataのdraft、選択部位、複数選択、姿勢コピー、編集中の角度やオフセット等を保持する。キー追加・削除・プリセット適用を行う。保存する内容と、編集操作を助ける一時状態は別に持つ。

### CustomizePreviewController

モデル、スケルトン姿勢、カメラ、現在表示フレーム、再生状態、ギズモドラッグやIKセッションを扱う。モデルを「どう見せるか」と、画面上で何を掴んだかを計算する。

回転結果を自身だけに保持しても保存されない。Flowを通してMotionEditorへ渡す経路が必要になる。

### CustomizeCharacterEditorController

キャラクターのスロット選択・名前・技の割当て・技候補の選択を担当する。`DrawSlotSelect`、`DrawEditor`、`DrawAttackPicker`は次のCustomizeModeを返す。現在はこの担当内にImGui操作が残っている。

### Viewと共通UI

CustomizeAttackEditorViewは攻撃画面とモーション画面で共通の枠・再生操作・タイムラインを使う。名前にAttackがあっても攻撃画面専用とは限らない。

Rootはボタンの押下捕捉、文字入力、ドロップダウン等の共通操作状態を持つ。UIRendererは文字・画像等の描画命令を実際に描く。Rootは保存先のJSONや技の意味を知らない。

## 1.3 寿命と画面切替

1. CustomizeScene生成時、WorldとFlowが構築される。
2. EnterでUIアクションマップへ切り替え、modeをMainMenuにし、プレビューを初期化する。
3. modeだけを変更した場合、Sceneや各Controllerは破棄されない。
4. スロットを新しく選ぶ際、編集履歴・draft等を明示的に初期化する。
5. Scene::ExitはFlow::Exitへ委託して履歴とプレビューを解放し、World::Clearを呼ぶ。

画面を戻っただけでデータが消えるとは限らない。どこでResetを呼ぶかを読まなければ寿命は分からない。逆に、Sceneを出る際に共有リソース全体も必ず破棄されるとは、このコードだけでは言えない。共有Managerの寿命は別に確認する必要がある。

## 1.4 1フレームの二つの経路

```text
Scene::RunSystems
  Flow::RunSystems
    UpdatePreviewPlayback
    modeに応じた専用UI更新 または Cancel処理

Scene::Draw(renderer)
  Flow::Draw(renderer)
    modeごとの描画
    編集画面では DrawAttackEditor
      View::DrawBackdrop
      RenderAttackPreview
      View::Draw
```

通常の専用UI編集は更新側で確定する。ただし**ギズモ・関節の選択は描画側のRenderAttackPreviewにも処理がある**。さらに未移行のスロット選択やキャラクター編集はDraw内のImGuiでmodeを変更する。

したがって「このプロジェクトはDrawが完全に副作用なし」と理解すると誤る。将来の整理では、通常UI・旧ImGui・ギズモの3経路を見落とさないこと。

## 1.5 関数がvoidでも結果は存在する

`SelectAttackSlot(category, slotIndex)`は戻り値を持たないが、編集履歴を消し、AttackEditorへ読込を依頼し、View・MotionEditor・再生フレームを初期化してmodeを変える。結果は所有メンバーの変更として残る。

`GetDraft()`の参照を受け取る場合も、戻り値がコピーか参照かで意味が変わる。変更可能な参照なら呼出先が元のdraftを書き換えられる。関数の戻り値だけでなく「どのインスタンスを変更するか」を必ず見る。

### 理解確認

- モーション画面へ移るとSceneを作り直すのか。
- Previewの姿勢だけ変えても、なぜ保存内容が変わらない可能性があるのか。
- World&とWorldの値メンバーの違いを説明できるか。
