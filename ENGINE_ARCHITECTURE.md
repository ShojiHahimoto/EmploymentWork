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
- GameObject ごとの Component 群は `std::vector<std::unique_ptr<Component>>` として保持する
- `TransformComponent`、`CameraComponent`、`VelocityComponent`、`StateComponent` などは共通基底 `Component` を継承する
- `Component` の継承は型管理のためだけに使い、ゲームロジックは持たせない
- Component の追加、取得、存在確認は World が提供する
- System は World の GameObject 群を走査し、必要な Component を参照・変更する
- GameObject 自身に Update やゲームロジックを持たせない

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

当面のバトル基礎では、次の粒度を基準とする。

```text
InputSystem
CharacterControlSystem
MovementResolveSystem
HitCollisionSystem
HitResolveSystem
TransformSystem
CameraSystem
Debug 系 System
```

- InputSystem は、キーボードやコントローラー入力を 1 フレーム分の入力状態に変換する
- CharacterControlSystem は、入力、State、Velocity を見て、歩き、ジャンプ、落下などを決める
- MovementResolveSystem は、Velocity による移動、仮地面、壁、プレイヤー同士の押し合い、めり込み解消までを扱う
- HitCollisionSystem は、攻撃判定とやられ判定など、ヒット用の接触情報を収集する
- HitResolveSystem は、ヒット結果、ダメージ、のけぞり State、ヒットストップなどの結果を確定する
- TransformSystem は、描画やカメラ用の world キャッシュを更新する
- CameraSystem は、カメラ Transform から View / Projection を更新する
- Debug 系 System は Debug ビルドや検証用途に限定し、バトル結果の確定責務を持たせない

### Collision の種類

格闘ゲームでは、位置補正用の接触とヒット判定用の接触を分ける。

- 地面、壁、プレイヤー押し合いは MovementResolveSystem で位置を補正する
- 攻撃判定、やられ判定、ガード判定は HitCollisionSystem で収集する
- ダメージ、State 変更、ヒットストップなどの結果は HitResolveSystem で確定する
- HitCollisionSystem は結果を直接確定しない

## Collision / Resolve

ヒット用 Collision は衝突情報の収集のみを行う。

- HitCollisionSystem は結果を直接確定しない
- HitResolveSystem が全体の衝突情報を見て結果を確定する
- 同一フレーム内の整合性を優先する

地面、壁、プレイヤー押し合いなど、移動結果を補正する接触は MovementResolveSystem の責務とする。

## ダメージ / バトル状態

ダメージやヒット結果は、個別の CollisionSystem 内で直接反映しない。

- HP などの値は HealthComponent または BattleStatusComponent として保持する
- ダメージ適用、のけぞり、ヒットストップ、無敵などの結果確定は HitResolveSystem が担当する
- StateComponent は現在状態と stateFrame を保持する
- stateFrame は、状態に入ってからの経過フレームとして扱う

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
- SpawnRequest は type、name、position、rotationDegrees を指定できる

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
