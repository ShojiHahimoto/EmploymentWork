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

System は固定順序で更新する。

```text
Input -> State -> Movement -> Collision -> HitResolve -> Spawn/Destroy
```

各 System は World を入力として受け取り、必要な Component / GameObject データを処理する。

## Collision / Resolve

Collision は衝突情報の収集のみを行う。

- CollisionSystem は結果を直接確定しない
- HitResolveSystem が全体の衝突情報を見て結果を確定する
- 同一フレーム内の整合性を優先する

## 生成 / 削除

生成・削除はリクエストとして蓄積する。

- SpawnRequest に生成要求を積む
- DestroyRequest に削除要求を積む
- フレーム最後に一括反映する
- Update 中に直接追加・削除してはいけない

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
