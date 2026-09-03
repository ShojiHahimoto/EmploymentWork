# 格闘ゲームエンジン フレームワーク設計

2026-05-14 時点の設計思想を基準とする。

## 運用方針

このファイルをプロジェクトの設計基準として扱う。

- 実装判断で迷った場合は、このファイルの原則を優先する
- 新しい設計思想が増えた場合は、このファイルに追記・整理する
- コード変更時は、ここに書かれた禁止事項と更新順序からブレないようにする
- 文字コードやコメント方針などの実装ルールは `CODING_RULES.md` を参照する

## プロジェクト前提

DirectX11 + C++ による格闘ゲーム専用エンジン。

- 60fps 固定
- DeltaTime は使用しない
- 完全フレームベース制御

## アーキテクチャ原則

Data と Logic を完全分離する。

- Data: GameObject / Component
- Logic: System

GameObject はデータのみを保持し、System がすべての処理を担当する。

## 全体構造

基本的な流れは次の通り。

```text
App -> Game -> SceneManager -> Scene -> World -> GameObject 群
```

System 群は World に対して処理を実行する。

## World

World は全 GameObject を保持し、生成・削除キューを管理する。

- Update 中に GameObject 配列を直接変更してはいけない
- 生成・削除はフレーム終端でまとめて反映する
- World は GameObject 群に対する唯一の管理単位とする
- バトル中に相手 Player を頻繁に参照するため、World はバトル用 Player 2 体の GameObjectId を保持する
- Player 同士の関係はポインタではなく GameObjectId で表現し、System が必要なタイミングで World から Component を取得する

## GameObject

GameObject はデータのみを保持する。

保持する代表的なデータ:

- Transform
- Velocity
- Collider
- State
- Lifetime

GameObject にロジックを持たせてはいけない。

現行実装では、World が `std::vector<GameObject>` として GameObject 群を保持する。

- GameObject は `GameObjectId` と Component 群だけを持つ
- GameObject はデバッグ表示や識別用の name を持つ
- GameObject は大分類用の `GameObjectTag` を持つ
- GameObject ごとの Component 群は `std::vector<std::unique_ptr<Component>>` として保持する
- `TransformComponent`、`CameraComponent`、`VelocityComponent`、`StateComponent` などは共通基底 `Component` を継承する
- `Component` の継承は型管理のためだけに使い、ゲームロジックは持たせない
- Component の追加、取得、存在確認は World が提供する
- System は World の GameObject 群を走査し、必要な Component を参照・変更する
- GameObject 自身に Update やゲームロジックを持たせない
- Player などの対象判定は、Tag と必要 Component の有無を両方確認する

## Transform 設計

Transform は Component と System に分離する。

- TransformComponent は座標、回転、スケール、親子 ID、ワールド行列キャッシュのみを持つ
- 回転は内部的に Quaternion で保持する
- 外部操作 API では扱いやすさのため Euler 角 degree 指定を許可する
- Euler 角は pitch(X), yaw(Y), roll(Z) として受け取り、Quaternion に変換する
- 親子関係は GameObjectId で表現し、ポインタ直接参照は禁止する
- 親子関係の変更、循環参照チェック、ワールド行列更新は TransformSystem が担当する
- SetParent / RemoveParent は KeepLocalTransform と KeepWorldTransform の方針を選べるようにする
- Collider は見た目用 Transform と密結合しない。処理は 2D を前提に別管理する

### Transform 更新の扱い

TransformSystem は、位置変更リクエストを後から一括適用する System ではない。

- 各 System が位置を変更する場合は、`TransformSystem::SetLocalPosition` などを通して local 値を即時変更する
- Movement / CollisionResolve / DebugEditor などの処理中に参照する座標は、原則として localPosition を正とする
- worldMatrix / worldPosition / worldRotation / worldScale は描画、カメラ、親子階層反映用のキャッシュとする
- TransformSystem の `UpdateWorldTransforms` は、変更済み local 値から world キャッシュを再計算する役割とする
- world キャッシュを参照する System は、その前に TransformSystem による更新が済んでいる順序で実行する
- バトル処理の移動、接地、壁、押し合い解決は worldMatrix に依存しない

## Camera 設計

Camera も Component と System に分離する。

- CameraComponent は FOV、AspectRatio、Near/Far、View/Projection 行列キャッシュのみを持つ
- カメラの位置と回転は TransformComponent で管理する
- CameraSystem は TransformComponent から View 行列を作成する
- Projection 行列は CameraComponent の投影設定から作成する
- DirectX 左手座標系に合わせ、カメラ前方向は +Z とする
- CameraSystem は入力処理、追従処理、Renderer 所有を担当しない
- バトル中のメインカメラ追従は BattleCameraSystem が担当し、CameraSystem は行列キャッシュ更新だけを担当する
- Scene / World 実装前は Game が仮に CameraComponent と Transform を保持する
- Scene / World 実装後は World が activeCameraId と CameraComponent を管理する

## Renderer と Camera の関係

Renderer は Camera を所有しない。

- Renderer は外部から渡された View / Projection 行列を使って描画する
- CameraComponent の更新は CameraSystem が担当する
- activeCameraId の管理は Game の仮実装を経て、最終的に World が担当する
- デバッグ用立方体描画はカメラ確認用の一時的な最小描画機能とする
- 本格的な 3D モデル描画は、モデル読み込みとシェーダー管理を分けて追加する

## Component 設計

Component は struct で定義し、データ専用とする。

- 処理を持たせない
- メンバ関数によるゲームロジックを実装しない
- System が Component の値を読み書きして処理する

## System 設計

System は固定順序で更新する。ただし、Component にロジックを持たせないことと、System を過度に細分化することは同義ではない。

```text
Input -> State -> Movement -> Collision -> HitResolve -> Spawn/Destroy
```

各 System は World を入力として受け取り、必要な Component / GameObject データを処理する。

### バトル用 System 粒度

バトル中は同一フレーム内の整合性を優先するため、System の更新順を固定する。
一方で、管理が複雑になりすぎるほど細かい System 分割は避ける。

当面のバトル基礎では、次の粒度と順序を基準とする。

```text
InputSystem
PlayerFacingSystem
InputHistorySystem
CommandInputSystem
StateUpdateSystem
PlayerControlSystem
MovementSystem
BattleCameraSystem
EmbedResolveSystem
HitCollisionSystem
HitResolveSystem
HitReactionSystem
BattleResultSystem
BattleHUDSystem
MotionSystem
TransformSystem
CameraSystem
DebugSystem
```

- InputSystem は、キーボードやコントローラー入力を 1 フレーム分の入力状態に変換する
- PlayerFacingSystem は、入力履歴を保存する前に相手との位置関係から `facingDirection` を更新する
- InputHistorySystem は、InputSystem の確定済み入力を格闘ゲーム用入力履歴へ変換し、InputHistoryComponent に保存する
- CommandInputSystem は、InputHistoryComponent を過去フレームまで遡ってコマンド成立を確認し、CommandBufferComponent に保存する
- StateUpdateSystem は、入力履歴、コマンド候補、現在状態、地上/空中、被弾、キャンセル可否を見て今フレームの `PlayerActionState` と `actionFrame` を更新する
- PlayerControlSystem は、確定済みの `PlayerActionState` に応じて歩き、攻撃、被弾などの行動処理を行う
- MovementSystem は、Velocity による移動、重力、ジャンプ、技移動など、めり込み解消前の位置更新を扱う
- BattleCameraSystem は、Movement 後の 2 Player 位置からメインカメラ Transform を更新する
- EmbedResolveSystem は、地面、壁、プレイヤー同士の押し合いなど、移動後の位置めり込み解消を扱う
- HitCollisionSystem は、攻撃判定とやられ判定など、ヒット用の接触情報を収集する
- HitResolveSystem は、ヒット結果、ダメージ、のけぞり State、ヒットストップなどの結果を確定する
- HitReactionSystem は、HitResolveSystem が確定した被弾反応リクエストを読み、ヒットバック、ガードバック、吹き飛び、ダウンを処理する
- BattleResultSystem は、KO とラウンドタイマーのタイムアップを確認し、勝敗結果を確定する
- BattleHUDSystem は、HPバーやラウンドタイマーなどの対戦 HUD 表示状態を更新し、ゲームビューへ描画する
- MotionSystem は、現在の ActionState / actionFrame / MotionData から GameObject ごとのボーン姿勢を更新し、Renderer へ渡すスキニング行列を作る
- TransformSystem は、描画やカメラ用の world キャッシュを更新する
- CameraSystem は、カメラ Transform から View / Projection を更新する
- DebugSystem は Debug ビルドや検証用途に限定し、バトル結果の確定責務を持たせない

StateUpdateSystem は、Player タグと Velocity / State を持つ GameObject を対象にする。
InputHistoryComponent がある場合はテンキー方向、ジャンプを読み、CommandBufferComponent がある場合は攻撃候補を読み、ない場合は中立入力として扱う。
現段階では入力履歴、接地状態、Y 速度を見て、`Idle`、`FrontWalk`、`BackWalk`、`VerticalJumpStartup`、`FrontJumpStartup`、`BackJumpStartup`、`VerticalJump`、`FrontJump`、`BackJump`、`Fall`、`GroundAttack`、`AirAttack`、`LandingRecovery`、`Hitstun`、`Guardstun`、`AirHitstun`、`Down`、`WakeUp` を含む `PlayerActionState` を確定する。
Player の向きは、World に登録された相手 Player の Transform と自分の Transform の X 座標比較で決める。
自分が左、相手が右なら右向き、自分が右、相手が左なら左向きとする。
空中にいる間は対面方向を更新しない。
着地直後は、前フレームの Jump / Fall State が残っていても `isGrounded=true` なら相手方向へ振り向いてよい。
`StateComponent::actionStartFacingDirection` は現在の `PlayerActionState` に入った瞬間の向きを保存し、ジャンプ横速度など行動開始時の向きに固定したい処理で使う。
ジャンプ入力はテンキー方向の `7 / 8 / 9` を使い、向きに応じてバックジャンプ、垂直ジャンプ、前ジャンプへ分ける。
ジャンプ入力直後は 4F のジャンプ移行 State に入り、`actionFrame 0〜3` はまだ地上行動として扱う。
ジャンプ移行中に攻撃候補が成立した場合、地上攻撃でジャンプ移行を上書きする。
ジャンプ移行 State が `actionFrame 4` に到達したフレームで、対応する実ジャンプ State に遷移する。
ジャンプ移行から実ジャンプへ遷移するときは `actionStartFacingDirection` を引き継ぎ、めくりや着地前後の振り向きでジャンプ横方向が反転しないようにする。
今フレームで被弾要求がある場合は最優先で `Hitstun` に遷移し、攻撃前隙中でも攻撃を中止する。
攻撃中や被弾中などのキャンセル不可行動は、終了またはキャンセル可能になるまで新しい行動へ変更しない。
`actionFrame` は `PlayerActionState` に入ってからの経過フレームとして、StateUpdateSystem が更新する。
`actionFrame` は StateUpdateSystem の処理開始時に 1 進み、`PlayerActionState` が切り替わった場合は 0 に戻る。

PlayerControlSystem は、Player タグと Transform / Velocity / State / CharacterParameter を持つ GameObject を対象にする。
PlayerControlSystem は StateComponent の `currentActionState` や `actionFrame` を更新しない。
現段階では確定済み `PlayerActionState::FrontWalk / BackWalk` のときに向きとキャラクターパラメータから Velocity の X 成分を毎フレーム上書きし、各 Jump の `actionFrame == 0` のときだけ Velocity へジャンプ初速を設定する。
ジャンプ移行中は実ジャンプの初速を入れず、Velocity の X/Y を 0 にして地上で滑らないようにする。
実ジャンプの横速度は現在の `facingDirection` ではなく、`actionStartFacingDirection` を基準に設定する。
Velocity はノックバック、技移動、押し出しでも変化するため、Idle / Walk 判定には使わない。
仮接地判定は EmbedResolveSystem 内に置き、`Transform.y <= 0` を接地として `y = 0`、`Velocity.y = 0`、`isGrounded = true` に補正する。

MovementSystem は入力を直接読まない。

- `SetVelocity` は Velocity 全体を上書きする
- `SetVelocityX/Y/Z` は指定軸だけを上書きする
- `AddVelocity` は外力や技移動などの加算用に使う
- `MovementSystem::Update` は空中重力を Velocity に加算し、確定済み Velocity を Transform に反映する
- 上昇中と下降中で重力値を分ける

BattleCameraSystem は MovementSystem の後、EmbedResolveSystem の前に実行する。

- メインカメラの目標 X 座標は常に 2 Player の X 座標の中心とする
- 実際のカメラ X 座標は `BattleCameraFollowComponent::velocityX` を使い、加速と減速で目標へ滑らかに追従する
- X 方向のデッドゾーンは設けない
- カメラ X はステージ左右端でクランプし、画面にステージ外が映らないようにする
- カメラ Y は `CameraYFollowMode::NaturalJump` の Player 高さに応じて少しだけ上げ、補間で滑らかに追従する
- 通常ジャンプ、通常ジャンプ由来の落下、空中攻撃、通常ジャンプ中の通常被弾は `NaturalJump` を維持する
- バーストなどの被弾吹き飛びは `CameraYFollowMode::Ignore` とし、カメラ Y 追従対象にしない
- 着地、ダウン、起き上がり、地上復帰時は `CameraYFollowMode::None` に戻す
- カメラ Z と FOV は現段階では固定値として扱い、ズームはまだ行わない
- CameraSystem は従来通り、更新済み Transform から View / Projection 行列を作る

EmbedResolveSystem は BattleCameraSystem の後、TransformSystem の前に実行する。

- 仮接地判定、壁補正、プレイヤー同士の PushBox めり込み補正を扱う
- 現段階の壁は仮実装として `StageMinX=-30 / StageMaxX=30` の固定値で扱う
- Player がメインカメラ画角外へ後ろに下がろうとした場合、画面端を壁と同じように扱って位置補正する
- 片方だけが相手方向へ歩いてめり込んだ場合、歩いていない側を押す
- 押される側が壁に到達して押し切れない場合、残りのめり込み量は押した側へ戻す
- 両方が押し合っている場合、またはどちらが押したか確定できない場合は、互いに離す形で補正する

HitCollisionSystem、HitResolveSystem、HitReactionSystem は、押し合いや壁補正とは別に攻撃ヒット処理を扱う。

- HitCollisionSystem は `currentAttack.slotId`、`actionFrame`、`CharacterAttackDataComponent` から現在有効な AttackBox を計算する
- AttackBox と相手の HurtBox を 2D AABB で判定し、当たった事実だけを World の一時結果バッファへ保存する
- HitCollisionSystem は `StateComponent` や `currentAttack.hasHit` を直接変更しない
- HitResolveSystem は一時結果バッファを読み、攻撃側の `currentAttack.hasHit` と防御側の `PlayerActionState::Hitstun / Guardstun` を確定する
- HitResolveSystem はヒット/ガード確定後に `HitReactionRequest` を World へ積み、座標や Velocity は直接変更しない
- HitReactionSystem は `HitReactionRequest` を読み、通常ヒットバックやガードバックは 1 フレームの即時座標補正として処理する
- 通常ヒットバックやガードバックで防御側が壁に到達して下がりきれない場合、不足分を攻撃側へ返して 2 Player 間の距離を確保する
- `AttackData.hitReactionType` は `Normal / Down / Burst / HardBurst` を基本とする
- `Normal` は防御側を後ろへずらす地上ヒットバックとして扱う
- `Down` はその場で `PlayerActionState::Down` へ遷移し、一定フレーム後 `WakeUp`、その後 `Idle` へ戻る
- `Burst` と `HardBurst` は防御側に吹き飛び Velocity を設定し、`AirHitstun` として重力で落下させる
- `Burst` と `HardBurst` の防御側が着地した場合は `Down` へ遷移する
- 空中で追撃された場合は、技ごとのタイプより弱めの空中再打ち上げを優先して使う
- `Down / WakeUp` 中、または接地済みの `AirHitstun` は攻撃を受けない
- `AttackData.hitstunFrames` は、ヒットした相手が `PlayerActionState::Hitstun` を維持するフレーム数として扱う
- `AttackData.guardstunFrames` は、ガードした相手が `PlayerActionState::Guardstun` を維持するフレーム数として扱う
- ガード時は本来ダメージの 1/10 を HP へ適用する
- 通常ガードは地上の `Idle / FrontWalk / BackWalk` 中に後ろ入力をしている場合のみ成立し、`Guardstun` 中は入力に関係なく連続ガードとして扱う
- HPバーのダメージ蓄積表示は `Hitstun / Guardstun / AirHitstun / Down / WakeUp` 中に停止し、硬直解除後に現在HPへ追いつく
- 1vs1 前提でも、結果バッファ内では処理対象を明確にするため attacker / defender の GameObjectId を持つ

BattleResultSystem は HitResolveSystem の後に実行し、同一フレームで KO とタイムアップが重なった場合は KO 判定を優先する。
ラウンドタイマーは 60fps 固定の残りフレーム数として保持し、0 になったら残り HP が高い Player を勝者にする。
HP が同値なら Draw とする。

BattleHUDSystem は Player の HealthComponent と StateComponent を読み、HPバー表示用 Component を更新する。
HPバーは現在 HP に即追従するバーと、ヒットスタン中だけ古い長さを維持するダメージバーに分ける。
2D HUD は既存 Renderer の SpriteBatch 描画を使い、TransformComponent の localPosition.xy を画面左上座標、localScale.xy をピクセルサイズとして扱う。
UI GameObject は `GameObjectTag::UI` を付け、3Dモデル描画やデバッグキューブ描画の対象から除外する。

## Input 設計

InputSystem は、各デバイスの入力をフレーム単位の Action 状態へ変換する。

- 入力サンプリングは毎フレーム 1 回だけ行う
- 同一フレーム内のすべての System は、確定済みの同じ入力結果を読む
- 有効な ActionMap はゲーム全体で 1 つだけ持つ
- 2 プレイヤー時も ActionMap はプレイヤー単位ではなくゲーム単位で切り替える
- PlayerInputState は Player ごとに持つ
- InputBinding は actionMap、action、deviceType、playerIndex、具体デバイス入力を持ち、どのデバイスをどの Player へ渡すかを Binding 側で決める
- 現段階の既定 Binding は `Player 0 = Keyboard`、`Player 1 = Gamepad 0`
- Gamepad の Move は左スティックと十字キーの両方から受け取る
- 十字キーとキーボードの上下左右は、左右同時または上下同時なら打ち消し、斜め入力は長さ 1 以下に正規化する
- スティックは radial dead zone を通した後、InputHistorySystem が角度からテンキー方向へ丸める
- キーコンフィグは将来 JSON などの外部ファイルから読み込める構造にする
- 直近で入力されたデバイス種別は Player ごとに保持し、操作説明 UI などで使えるようにする
- デッドゾーンなどの調整値は InputSettings にまとめ、後から調整できるようにする
- 入力履歴やコマンド判定は、必要な Scene や Battle 系 System 側で持つ
- DebugCamera 操作は DebugCameraControlSystem 側に委託し、通常のゲーム入力には含めない

InputHistoryComponent は、バトル系オブジェクトが入力履歴を保存するためのデータ専用 Component とする。

- 生のキーボード入力や PlayerInputState を丸ごと保存しない
- `Move` Action はテンキー表記の `1〜9` に変換して保存する
- InputHistorySystem は World の BattlePlayerId の順番と InputSystem の PlayerInputState 番号を対応させて保存する
- 攻撃、ガードなどのボタンは InputSystem が判定済みの `Trigger / Press / Release` を保存する
- 攻撃ボタンは `AttackA`、`AttackB`、`AttackX`、`AttackY` の 4 種とする
- キーボード既定割り当ては `AttackA=H`、`AttackB=J`、`AttackX=Y`、`AttackY=U`
- ゲームパッド既定割り当ては `AttackA=A`、`AttackB=B`、`AttackX=X`、`AttackY=Y`
- ジャンプは専用ボタンではなく、テンキー方向 `7 / 8 / 9` から `Trigger / Press / Release` を作って保存する
- InputHistoryComponent は過去 30F の ring buffer を持ち、各フレームのテンキー方向、攻撃ボタン mask、入力時点の `facingDirection` を保存する
- 波動コマンドなどの方向コマンドは、各 InputHistoryFrame に保存された `facingDirection` を基準に相対方向へ変換して判定する
- コマンド方向判定は可変長のステップ列で扱い、`236`、`41236`、`236236` など長さの違うコマンドを同じ処理で判定する
- 簡易入力では、テンキー方向を上下左右の成分として扱い、必要成分を含む入力ならそのステップを満たしたものとする
- 同じ方向を押し続けたフレームは 1 ステップ分として扱い、斜め 1 回だけで複数ステップが成立しないようにする
- 選択可能コマンドは `Hadouken(236)`、`Shoryuu(626)`、`Yoga(41236)`、`ReverseYoga(63214)`、`FullRotate` を基本とする
- `FullRotate` は回転方向や開始方向を問わず、入力履歴内に上下左右成分がすべて 1 回以上あれば成立とする
- コマンド候補の優先度は、`FullRotate > Yoga / ReverseYoga > Shoryuu > Hadouken > 通常攻撃` の順に高くする
- 通常攻撃同士の同時入力は `AttackA > AttackB > AttackX > AttackY` の優先度で扱う
- CommandInputSystem は、CharacterAttackDataComponent に割り当て済みの AttackData を読み、必殺技の `commandId` と攻撃ボタンの組み合わせで候補を作る
- CommandBufferComponent は成立済み攻撃候補を保持し、StateUpdateSystem が行動可能なタイミングで消費する
- コマンド先行入力の有効期限は技ごとではなく、CommandInputSystem の共通猶予フレームで一括管理する
- 入力の取得は InputSystem が担当し、入力履歴への保存は InputHistorySystem が担当する
- StateUpdateSystem は保存済みの InputHistoryComponent と CommandBufferComponent を読み、今フレームの PlayerActionState を決める
- PlayerControlSystem は確定済みの PlayerActionState を読み、今フレームの行動処理を行う
- InputHistoryComponent 自身は判定関数や Update を持たない

ボタン入力状態の名称は次の意味で統一する。

- `Trigger`: 前フレーム押されておらず、今フレーム押された
- `Press`: 今フレーム押されている
- `Release`: 前フレーム押されており、今フレーム離された

### Collision の種類

格闘ゲームでは、位置補正用の接触とヒット判定用の接触を分ける。

- 地面、壁、プレイヤー押し合いは EmbedResolveSystem で位置を補正する
- 攻撃判定とやられ判定の接触は HitCollisionSystem で収集する
- ガード成立可否は、収集済み接触結果と防御側の入力/状態を見て HitResolveSystem で確定する
- ダメージ、State 変更、ヒットストップなどの結果は HitResolveSystem で確定する
- HitCollisionSystem は結果を直接確定しない

## Collision / Resolve

ヒット用 Collision は衝突情報の収集のみを行う。

- HitCollisionSystem は結果を直接確定しない
- HitResolveSystem が全体の衝突情報を見て結果を確定する
- 同一フレーム内の整合性を優先する

地面、壁、プレイヤー押し合いなど、移動結果を補正する接触は EmbedResolveSystem の責務とする。

## ダメージ / バトル状態

ダメージやヒット結果は、個別の CollisionSystem 内で直接反映しない。

- HP などの値は HealthComponent または BattleStatusComponent として保持する
- ダメージ適用、のけぞり、ヒットストップ、無敵などの結果確定は HitResolveSystem が担当する
- KO、タイムアップ、残り HP 比較による勝敗確定は BattleResultSystem が担当する
- StateComponent は現在の PlayerActionState と actionFrame を保持する
- actionFrame は、PlayerActionState に入ってからの経過フレームとして扱う

## 生成 / 削除

生成・削除はリクエストとして蓄積する。

- SpawnRequest に生成要求を積む
- DestroyRequest に削除要求を積む
- フレーム最後に一括反映する
- Update 中に直接追加・削除してはいけない

現行実装では、World が SpawnRequest / DestroyRequest を保持する。

- System や Debug UI は `World::RequestSpawn` / `World::RequestDestroy` を呼ぶ
- GameObject 配列の実変更は SpawnDestroySystem だけが行う
- SpawnDestroySystem は TransformSystem / CameraSystem の直前で実行する
- System 更新中に発生した生成・削除は、同一フレームの判定処理には参加しない
- SpawnDestroySystem 反映後に TransformSystem を実行するため、生成されたオブジェクトは同一フレームの描画には反映される
- 親 GameObject を削除する場合は、TransformComponent の childIds を辿り、子も再帰的に削除する
- 存在しない GameObject への削除要求や重複した削除要求は無視できる実装にする

生成内容は SpawnType で分岐する。

- `SpawnType::DebugCube` は TransformComponent を持つ GameObject を生成する
- `SpawnType::Debugman` は TransformComponent と ModelComponent を持つ GameObject を生成する
- `SpawnType::DebugPlayer` は Player タグを持ち、TransformComponent、ModelComponent、VelocityComponent、StateComponent、InputHistoryComponent、CommandBufferComponent、CharacterParameterComponent、CharacterAttackDataComponent、HitBoxComponent を持つ GameObject を生成する
- `SpawnType::DebugPlayer2` は Player タグを持ち、DebugPlayer と同じバトル用 Component を持つ
- SpawnRequest は type、name、position、rotationDegrees を指定できる

## HitBox / Battle Collision

格闘ゲーム用の判定は 2D AABB を基準にする。

- 判定は見た目の 3D Model とは分離し、X / Y 平面上の 2D 矩形として扱う
- Component 数を増やしすぎないため、Player は `HitBoxComponent` 1 つを持つ
- `HitBoxComponent` の内部で `pushBox`、`hurtBox`、`currentAttack` を分ける
- `pushBox` はプレイヤー同士、壁、押し合い、めり込み解消に使う
- `hurtBox` は攻撃を受ける側の被弾判定に使う
- `currentAttack` は現在実行中の攻撃スロットと、この攻撃が既にヒットしたかを保持する
- 1vs1 前提では、同じ攻撃中の多段ヒット防止は相手 ID ではなく `hasHit` の bool で管理する
- 通常攻撃の AttackBox は GameObject として生成せず、`CharacterAttackDataComponent` と `actionFrame` から有効フレームだけ一時的に計算する
- 通常攻撃の確認用 slotId は `AttackA`、`AttackB`、`AttackX`、`AttackY` とする
- 通常攻撃スロットは地上用 `groundAttackSlots` と空中用 `airAttackSlots` に分ける
- 地上通常攻撃スロットには `usableState: Ground` の AttackData、空中通常攻撃スロットには `usableState: Air` の AttackData だけを割り当てる
- 同じ `AttackA / AttackB / AttackX / AttackY` ボタンでも、現在の地上/空中状態に合う通常攻撃だけを CommandInputSystem が候補化する
- 必殺技の確認用 slotId は `SpecialA`、`SpecialB` などとし、スロットに設定されたボタンと AttackData の `commandId` で発動する
- 例として `SpecialA` は `Hadouken + AttackA` で `debug_special_attack`、`SpecialB` は `Shoryuu + AttackB` で `debug_special_upper` を実行する
- 必殺技スロットが未設定、または地上/空中条件やコマンド条件を満たさない場合は、同じボタンの通常攻撃を通常通り実行できる
- 空中攻撃中に接地した場合、攻撃発生前なら硬直なしで `Idle` に戻し、発生中または後隙中なら `LandingRecovery` に遷移する
- `LandingRecovery` は現段階では固定 5F の着地硬直として扱い、終了後に通常行動へ戻る
- 飛び道具、設置技、独立して移動する攻撃は、必要になった段階で GameObject として Spawn する
- Debug ビルドではゲームビュー上に PushBox を白、HurtBox を緑、AttackBox を赤の半透明表示にする
- Scene View は自由カメラ確認用に使い、HitBox 可視化はゲームビュー側で確認する
- HitBox の可視化は ImGui から一括で表示/非表示を切り替えられるようにする

## CharacterData / AttackData

キャラクターと技は、保存場所と責務を分ける。

- CharacterData は `assets/CharacterData/<CharacterId>/Parameter.json` と `AttackList.json` で管理する
- AttackData は `assets/AttackData/<AttackDataId>.json` に技 1 つ単位で保存する
- AttackData は特定キャラクターのフォルダ内に置かない
- Character は、基本パラメータと AttackData ID のスロット割り当てで定義する
- `Parameter.json` は `characterName` と、前歩き速度、後ろ歩き速度、ジャンプ初速、前後ジャンプ横速度、上昇/下降重力などの固定パラメータを持つ
- カスタマイズシーンのキャラクター作成では、現段階では `characterName` と技スロットだけを編集し、基本パラメータはゲーム側の固定 JSON として扱う
- `AttackList.json` は `groundAttackSlots`、`airAttackSlots`、`specialAttackSlots` を分け、slotId、button、使用する AttackData ID の対応だけを持つ
- 通常攻撃はキャラクター側の地上/空中それぞれの `AttackA / AttackB / AttackX / AttackY` スロットに割り当てる
- 地上通常攻撃スロットと空中通常攻撃スロットは全て必須とし、未設定のまま保存しない
- 必殺技はキャラクター側の `AttackA / AttackB / AttackX / AttackY` に対応する任意スロットに割り当て、AttackData 側の `commandId` とスロットの `button` の組み合わせで発動する
- 必殺技スロットは任意のため、未設定なら同じボタンの通常攻撃を通常通り出せる
- `AttackList.json` の `attackDataId` は `assets/AttackData` からの相対 ID を基本とする。例: `debug_punch`、`Ground/slot_00`
- Loader は手動編集しやすいよう、`Ground/slot_00.json` や `assets/AttackData/Ground/slot_00.json` のような書き方も読み込み時に吸収する
- ファイル名だけで指定して複数候補が見つかる場合は曖昧な指定として扱うため、カテゴリ付き ID を推奨する
- AttackData は `attackKind`、必殺技用の `commandId`、発動可能状態を示す `usableState`、`hitstunFrames`、`guardstunFrames` を持つ
- AttackData は `hitReactionType` を持ち、技ごとの被弾反応を `Normal / Down / Burst / HardBurst` から選ぶ
- 対戦開始または Spawn 時に JSON を読み込み、`CharacterParameterComponent` と `CharacterAttackDataComponent` にコピーする
- 対戦中の System は JSON を直接参照せず、Component にコピー済みの値だけを参照する
- `CharacterParameterComponent` は、その GameObject が使うキャラクター基本パラメータを保持する
- `CharacterAttackDataComponent` は、slotId と読み込み済み AttackData の組み合わせを保持する
- 将来の技調整モードでは、メモリ上の AttackData / CharacterData を編集し、保存時に JSON へ書き戻す
- `CharacterDataSaver` は `characterName`、`groundAttackSlots`、`airAttackSlots`、`specialAttackSlots` を保存し、旧命名へ戻さない
- JSON の項目追加時は、未指定項目にデフォルト値を使えるようにし、既存データの読み込みを壊さない

## 3D Model / Resource

3D モデルは GameObject に直接持たせず、Resource と Component を分離する。

- `ModelResource` は FBX などから読み込んだ共有データを持つ
- `ModelResource` は Mesh、頂点、インデックス、Bone、AnimationClip の受け皿を持つ
- `ModelComponent` は GameObject が参照する `resourceKey` のみを持つ
- 同じモデルを複数 GameObject が使う場合でも、モデル本体は共有する
- FBX と同階層に置いた diffuse texture は Material 情報から読み込み、Mesh ごとに適用する
- FBX 側に diffuse texture 参照がない場合は、同階層の `*diffuse*.png` を fallback として探す
- 頂点には将来のスケルタルアニメーション用に bone index / bone weight を持たせる
- 現段階ではアニメーション再生は行わず、static pose として描画する
- AnimationClip は bone ごとの position / rotation / scale keyframe を保持できる構造にする
- ゲーム内で作成するキーフレームアニメーションも、同じ AnimationClip / Channel / Keyframe 構造に保存する
- MotionSystem を追加する場合は、再生状態を別 Component に持たせ、ModelResource や MotionData の AnimationClip / Pose を参照する
- 2D UI は ModelComponent ではなく、UI タグと用途別 Component を追加して表現する
- 現段階の HUD は通常の TransformComponent を使い、画面座標用の Transform として扱う

## Animation / Motion

自作モーション機能は、見た目の姿勢制御として扱い、攻撃性能を持つ AttackData と分離する。

- MotionData は AttackData とは別 JSON / 別リソースとして管理する
- AttackData は参照用の `motionDataId` だけを持ち、キーフレーム姿勢そのものは持たない
- 攻撃判定、ダメージ、硬直、キャンセル、ガード、リアクションは AttackData / HitBox / State 系で扱い、MotionData へ混ぜない
- MotionData は `motionDataId`、表示名、総フレーム、ループ有無、キーフレーム一覧を持つ
- キーフレームはフレーム番号と、その時点の全ボーンまたは編集対象ボーンのローカル姿勢を保存する
- 補完は位置を線形補間、回転を Quaternion Slerp、スケールを線形補間で扱う
- 初期実装では MotionSystem がキーフレーム間を補間し、必要になった最適化段階で 1F ごとの姿勢キャッシュへ移行する
- 姿勢キャッシュ導入後は、対戦中に毎フレーム補間計算せず、`actionFrame` からキャッシュ済み姿勢を参照する
- スキニングは GPU スキニングを基本方針とし、Renderer はボーン行列配列を HLSL へ渡す
- HLSL はスキニング実装以降、外部 `.hlsl` ファイルを優先して管理する
- `ModelResource` は共有モデルデータ、初期ボーン階層、bind pose、頂点ウェイト、FBX 由来 AnimationClip を持つ
- `ModelResource` に GameObject ごとの現在姿勢や再生状態を持たせない
- GameObject ごとの現在姿勢は `SkeletonPoseComponent` に保持する
- 再生中モーション ID、再生フレーム、ループ有無などの再生状態は `MotionPlayerComponent` に持たせる
- MotionSystem は `ModelComponent`、`SkeletonPoseComponent`、`MotionPlayerComponent` を読み、描画用スキニング行列を更新する
- 最初は FK で、各ボーンのローカル回転・位置・スケールを直接指定して姿勢を作る
- IK は FK キーフレーム再生が安定してから追加する
- IK はまず腕・脚向けの 2 ボーン IK を優先し、首、腰、背中などは FK 操作を基本とする
- IK 追加時は関節の可動域制限を持たせ、肘や膝が逆に曲がる、関節が破綻するなどの姿勢を防ぐ
- モーション編集画面は最初 ImGui 数値編集で作り、MotionData ID、対象ボーン名、現在プレビューフレームへの回転キー追加、保存から段階的に追加する
- 3D ギズモ、姿勢プリセット、IK 操作、可動域編集は、スキニング描画と FK キーフレーム保存が安定した後に追加する

## オブジェクト参照

GameObject 間のポインタ直接参照は禁止する。

関係性は次の方法で表現する。

- ID
- Event
- Request

## Scene 管理

SceneManager が Scene を管理する。

- Scene 切替時は旧 Scene を破棄する
- 旧 Scene の全 GameObject を削除する
- Scene 単位でメモリ解放を行う
- Scene はゲームロジック本体を直接持たない
- Scene は `Update` ではなく `RunSystems` を持ち、その Scene で使う System 群を固定順で実行する入口とする
- SceneManager は即時切り替えではなく、切り替え要求を保持して安全なタイミングで反映する

### 非バトル Scene の扱い

バトル部分以外の Scene は、同一フレーム内の全体整合性がバトルほど厳しくないため、必要に応じて Scene 内で直接制御してよい。

- Title / Result / Customize などは、BattleScene と同じ System 群を無理に持たせない
- InputSystem と ActionMap は Scene をまたいで使い回す
- Scene が World を持つ構造は維持し、必要になった段階で UI 用 GameObject やプレビュー用 GameObject を追加する
- 非バトル Scene でも GameObject / Component にゲームロジックを持たせる方針にはしない

CustomizeScene は技調整・キャラクター調整用の作業 Scene とする。

- 現段階ではタイトル画面の `P` キーから入る仮導線とする
- 技調整のパラメータウィンドウは、まず ImGui で実装する
- 地上技、空中技、必殺技の各スロット数は定数で管理し、実行中には変更しない
- AttackData 編集中はメモリ上の draft を変更し、Save 操作時だけ JSON へ書き戻す
- AttackData の保存先は `assets/AttackData/<Category>/slot_XX.json` を基準とする
- AttackDataSaver は CharacterDataLoader が読み込める JSON 形式を維持する
- スロット一覧は保存済み AttackData の表示名を見せ、手動保存した JSON も選びやすくする
- AttackData Editor は複数 AttackBox と単数の CancelSetting を編集できるが、発生タイミングは `frame.startup / active / recovery` を正とし、AttackBox 側にはフレーム情報を持たせない
- `frame.startup` は前隙フレーム数ではなく、攻撃ボタンを押したフレームを 1F とした時に何フレーム目から攻撃判定が出るかを表す
- `frame.startup` の最小値は 2F とする。1F は内部 `actionFrame=0` から攻撃判定が出るため、このプロジェクトでは使用しない
- `frame.active` は攻撃判定が出ているフレーム数、`frame.recovery` は攻撃判定が消えた後の硬直フレーム数を表す
- キャンセル設定は `canAttackCancel` で有効/無効を切り替える。有効フラグを OFF にしても編集中 draft の `cancelSetting` は消さず、再度 ON にした時に直前編集内容を復元する
- キャンセル開始/終了フレームは内部データと JSON では 0 始まりを維持し、CustomizeScene の表示と入力だけプレビューに合わせて 1 始まりに変換する
- `AttackUsableState` は `Ground / Air` のみとし、通常技はカテゴリで固定、必殺技だけ CustomizeScene で選択可能にする
- キャンセル開始/終了フレームは、保存前と編集中の補正で技の総フレーム範囲内に収める
- 技プレビューは段階的に実装し、現段階では静止モデル、現在フレーム操作、active フレーム中の AttackBox 表示だけを扱う
- プレビューの Play / Stop / 1F 操作は、表示上だけ `0F = 攻撃ボタンを押す前の Idle`、`1F = 攻撃ボタンを押したフレーム` として扱う
- AttackData とバトル内部処理は 0 始まりを維持し、プレビュー表示フレームから 1 を引いた値を内部 actionFrame 相当として使う
- 例として `startup=4 / active=3 / recovery=7` の場合、内部は `0〜2 Startup / 3〜5 Active / 6〜12 Recovery`、プレビュー表示は `0 Idle / 1〜3 Startup / 4〜6 Active / 7〜13 Recovery` とする
- 本格的なモーション再生やキーフレームアニメーション編集は、保存形式とプレビュー導線が安定した後に追加する
- 将来の専用 UI 化やプレビュー再生を追加しても、保存形式と Loader 互換性を壊さない

## 禁止事項

- GameObject 間の直接参照
- GameObject がロジックを持つこと
- Component に処理を実装すること
- Update 中の GameObject 追加・削除
- State 内で直接結果を確定すること
- CollisionSystem で衝突結果を直接確定すること

## 最重要原則

状態はローカル、結果はグローバルで決定する。

同一フレームの整合性を最優先とする。

## 作業項目

1. ゲームループ確立（固定 60fps）
2. DirectX 初期化 + Render 最低限
3. ImGui 導入（デバッグ基盤）
4. Scene / World / GameObject 構造
5. System 基盤（Update 順固定）
6. Movement / Transform 最低限
7. Spawn / Destroy 機構
8. Collider / Collision（めり込み解消）
