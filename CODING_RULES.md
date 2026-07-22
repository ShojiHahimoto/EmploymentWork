# コーディングルール

このファイルは、実装時に守る技術的なルールをまとめる。
エンジン設計思想は `ENGINE_ARCHITECTURE.md` を基準にする。

## 文字コード

プロジェクト内のテキスト系ファイルは、UTF-8 BOM 付き + CRLF で保存する。

対象例:

- `.cpp`
- `.h`
- `.hpp`
- `.sln`
- `.vcxproj`
- `.filters`
- `.config`
- `.md`

理由:

- MSVC が既定コードページで UTF-8 BOM なしの日本語コメントを読むと、`warning C4819` が出ることがある
- 場合によってはコメントの文字化けが原因で、後続コードまで壊れて解釈される
- 日本語コメントを安全に残すため、MSVC が Unicode と認識できる形式に統一する
- 改行コードを CRLF に統一し、Windows / Visual Studio 上での不要な差分を減らす

外部パッケージ、ビルド成果物、`.git` 配下は対象外とする。

## コメント方針

コメントは、処理内容を逐語的に説明するのではなく、設計意図と注意点を中心に書く。

優先してコメントする内容:

- 新しい System、Component、行動処理を追加する場合は、関数の前に「何をする処理か」「何をしない処理か」が分かるコメントを書く
- なぜその順序で処理するのか
- 状態遷移、入力解釈、Velocity 代入など、ゲーム挙動の分岐条件と理由
- 親子階層、ID参照、フレーム終端反映など、設計ルールに関わる箇所
- 後から変更すると壊れやすい前提
- DirectX / SimpleMath など、API の使い方に癖がある箇所

コメント量は最小限に寄せすぎず、プロジェクト所有者が処理の流れを追える量を優先する。
ただし、実装とズレたコメントは残さず、処理変更時に必ず更新する。

避けるコメント:

- コードを読めば分かるだけの説明
- 古い実装経緯だけを残すコメント
- 実装とズレたまま更新されていないコメント

## ビルド確認

コード変更後は、可能な限り `Debug|x64` でビルド確認する。

基準:

- 0 errors
- 可能なら 0 warnings

文字コード警告 `C4819` が出た場合は、該当ファイルを UTF-8 BOM 付きで保存し直す。

改行コードだけの差分が大量に出た場合は、作業前に対象ファイルの文字コードと改行コードを確認する。

## C++ 言語標準

このプロジェクトは C++20 を使用する。

理由:

- GameObject が異なる型の Component を共通基底 `Component` 経由で保持する
- World が型付き Component 取得 API を提供し、System が必要な Component を参照する
- Visual Studio の全構成で `stdcpp20` を指定し、構成差によるビルド差異を避ける

## 関数引数

関数引数は、問題がない場合は `const T&` で渡すことを基本方針にする。

対象例:

- `std::string`
- `std::vector` などのコンテナ
- `DirectX::SimpleMath::Vector3`
- `DirectX::SimpleMath::Quaternion`
- `DirectX::SimpleMath::Matrix`
- `CameraComponent`
- `TransformComponent`
- `ModelResource`
- その他、コピーコストがある構造体やクラス

例:

```cpp
void SetLocalPosition(TransformComponent& transform, const DirectX::SimpleMath::Vector3& position);
bool LoadModel(const std::string& key, const std::string& path, ID3D11Device* device);
```

例外:

- `int` / `float` / `bool` / enum / `GameObjectId` など、小さくコピーが安い値
- 関数内で値を変更して呼び出し元へ反映したい場合の非 const 参照
- `std::unique_ptr` など、所有権を移動するために値渡しする場合
- ポインタで所有しない外部リソースを表す場合
- 値コピーしたほうが意図が明確、または寿命管理が安全な場合

`const` や参照は無理に付けない。
意図が「読むだけ」でコピー不要な引数は `const T&`、意図が「変更する」なら `T&`、意図が「所有権を受け取る」なら値渡しを使う。

## Windows.h

`NOMINMAX` は Visual Studio プロジェクトのプリプロセッサ定義に入れる。

対象:

- Debug|Win32
- Release|Win32
- Debug|x64
- Release|x64

各ヘッダーで個別に `#define NOMINMAX` は書かない。

理由:

- `Windows.h` は既定で `min` / `max` マクロを定義する
- `std::min` / `std::max` や自前関数名と衝突して、分かりにくいコンパイルエラーになる
- このプロジェクトでは Windows の `min` / `max` マクロを使わない方針にする
- プロジェクト設定で一括定義すると、ファイルごとの書き忘れを防げる

## 設計との関係

実装判断で迷った場合は、次の順で確認する。

1. `ENGINE_ARCHITECTURE.md`
2. この `CODING_RULES.md`
3. 既存コードの実装パターン

特に、GameObject / Component / System の責務分離は `ENGINE_ARCHITECTURE.md` を優先する。
