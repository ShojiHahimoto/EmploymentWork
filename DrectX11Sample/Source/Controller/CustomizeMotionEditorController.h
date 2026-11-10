#pragma once

#include "Controller/CustomizePreviewController.h"
#include "Data/AttackData.h"
#include "Data/MotionData.h"
#include "Data/MotionSkeletonDefinition.h"

#include <SimpleMath.h>

#include <array>
#include <string>

/// <summary>
/// カスタマイズ画面の MotionData 下書き、姿勢キー、汎用モーション用オフセットキーを管理する。
/// </summary>
class CustomizeMotionEditorController
{
public:
	/// <summary>UIに依存せず攻撃移動キーを追加する。既存の補間位置を初期値にする。</summary>
	/// <param name="attackData">編集対象。</param>
	/// <param name="frame">内部フレーム。</param>
	/// <param name="offset">追加した位置の出力先。</param>
	void AddAttackMovementKeyframe(AttackData& attackData, int frame, DirectX::SimpleMath::Vector2& offset);
	/// <summary>指定フレームの攻撃移動キーを削除し、編集値を再取得する。</summary>
	/// <param name="attackData">編集対象。</param>
	/// <param name="frame">内部フレーム。</param>
	/// <param name="offset">削除後の補間位置。</param>
	void DeleteAttackMovementKeyframe(AttackData& attackData, int frame, DirectX::SimpleMath::Vector2& offset);
	/// <summary>既存の攻撃移動キーだけを更新する。姿勢キーとは独立して扱う。</summary>
	/// <param name="attackData">編集対象。</param>
	/// <param name="frame">内部フレーム。</param>
	/// <param name="offset">前後・上下の移動量。</param>
	void UpdateAttackMovementKeyframe(AttackData& attackData, int frame, const DirectX::SimpleMath::Vector2& offset);
	/// <summary>指定フレームに攻撃移動キーがあるか取得する。</summary>
	/// <param name="attackData">参照対象。</param>
	/// <param name="frame">内部フレーム。</param>
	/// <returns>キーがあればtrue。</returns>
	bool HasAttackMovementKeyframe(const AttackData& attackData, int frame) const;
	/// <summary>選択部位を更新し、以前のIK操作を終了して編集値を同期する。</summary>
	/// <param name="preview">プレビュー制御。</param>
	/// <param name="part">部位番号。-1は選択解除。</param>
	/// <param name="frame">内部フレーム。</param>
	void SelectBodyPart(CustomizePreviewController& preview, int part, int frame);
	/// <summary>プリセット等の対象に使う選択部位を取得する。Gizmoのアクティブ部位とは分ける。</summary>
	/// <returns>選択された部位だけtrue。</returns>
	std::array<bool, MotionBodyPartCount> GetSelectedBodyParts() const;
	/// <summary>複数選択モードを取得する。</summary>
	/// <returns>複数選択中ならtrue。</returns>
	bool IsMultipleSelectionEnabled() const { return multipleSelectionEnabled; }
	/// <summary>モード切替時はアクティブ部位を基準に選択を同期する。</summary>
	/// <param name="enabled">複数選択を使う場合true。</param>
	void SetMultipleSelectionEnabled(bool enabled);
	/// <summary>全ての編集部位を選択する。一括のGizmo変形は行わない。</summary>
	void SelectAllBodyParts();
	/// <summary>UI・ギズモ共通のIK編集開始。位置は一時目標であり保存しない。</summary>
	/// <param name="preview">姿勢・IK計算を提供するプレビュー。</param>
	/// <param name="frame">姿勢キーのある内部フレーム。</param>
	/// <returns>編集セッションを開始できた場合true。</returns>
	bool BeginIkEdit(CustomizePreviewController& preview, int frame);
	/// <summary>ワールド位置の要求をIKで解き、回転キーへ反映する。</summary>
	/// <param name="preview">IK計算を提供するプレビュー。</param>
	/// <param name="frame">開始時と同じ内部フレーム。</param>
	/// <param name="target">ワールド基準の目標位置。</param>
	/// <returns>操作結果。</returns>
	std::string UpdateIkEdit(CustomizePreviewController& preview, int frame, const DirectX::SimpleMath::Vector3& target);
	/// <summary>IKの開始姿勢を解放し、次の操作へ持ち越さない。</summary>
	/// <param name="preview">終了するプレビュー。</param>
	void EndIkEdit(CustomizePreviewController& preview);
	/// <summary>旧ギズモの確定済みIK結果を回転キーへ取り込む。描画APIには依存しない。</summary>
	/// <param name="preview">操作結果を持つプレビュー。</param>
	/// <param name="frame">反映先の内部フレーム。</param>
	void ConsumeTranslationEdit(CustomizePreviewController& preview, int frame);
	static constexpr int MotionNameBufferSize = 128;
	static constexpr int MotionEditorBoneCount = MotionBodyPartCount;

	/// <summary>
	/// 攻撃モーション編集用に、MotionData 下書きと編集バッファを初期化する。
	/// </summary>
	void ResetForAttackMotion();
	/// <summary>履歴から下書きを復元し、表示用入力値を同期する。コピー済み姿勢や選択部位は維持する。</summary>
	/// <param name="restored">復元するモーション。</param>
	/// <param name="actionFrame">表示する内部フレーム。</param>
	void RestoreDraft(const MotionData& restored, int actionFrame);

	/// <summary>
	/// MotionData ID から編集用下書きを読み込み、存在しなければ新規下書きを作る。
	/// </summary>
	/// <param name="motionDataId">assets/MotionData 配下の拡張子なし ID。</param>
	/// <param name="fallbackDisplayName">新規下書き時の表示名。</param>
	/// <param name="totalFrames">編集時に使う総フレーム数。</param>
	/// <param name="looping">ループモーションとして扱う場合は true。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string LoadDraft(
		const std::string& motionDataId,
		const std::string& fallbackDisplayName,
		int totalFrames,
		bool looping);

	/// <summary>
	/// 編集中の MotionData 下書きを保存する。
	/// </summary>
	/// <param name="motionDataId">保存先 MotionData ID。</param>
	/// <param name="totalFrames">保存時に確定する総フレーム数。</param>
	/// <param name="looping">ループモーションとして保存する場合は true。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SaveDraft(const std::string& motionDataId, int totalFrames, bool looping);

	/// <summary>
	/// 現在の下書き内容を UI 共通の入力用バッファへコピーする。
	/// </summary>
	/// <param name="actionFrame">現在の内部 actionFrame。0F Idle の場合は -1。</param>
	void CopyEditorBuffers(int actionFrame);

	/// <summary>
	/// 選択部位やフレームが変わった時、現在フレームの編集値をバッファへ反映する。
	/// </summary>
	/// <param name="actionFrame">現在の内部 actionFrame。0F Idle の場合は -1。</param>
	void RefreshFrameEditValues(int actionFrame);

	/// <summary>
	/// AttackData から指定フレームの攻撃移動量を補間取得する。
	/// </summary>
	/// <param name="attackData">参照する AttackData。</param>
	/// <param name="frame">参照する内部 actionFrame。</param>
	/// <returns>補間済みの前後/上下移動量。</returns>
	DirectX::SimpleMath::Vector2 GetAttackMovementOffsetAtFrame(const AttackData& attackData, int frame) const;

	/// <summary>
	/// 現在フレームへ全身姿勢キーフレームを追加する。
	/// </summary>
	/// <param name="keyFrame">追加先の内部 actionFrame。</param>
	/// <param name="totalFrames">MotionData の総フレーム数。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string AddWholeBodyKeyframe(int keyFrame, int totalFrames);

	/// <summary>
	/// 現在フレームの全身姿勢キーフレームを削除する。
	/// </summary>
	/// <param name="keyFrame">削除対象の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string DeleteWholeBodyKeyframe(int keyFrame);

	/// <summary>
	/// 選択部位の回転キーを現在の編集値で更新する。
	/// </summary>
	/// <param name="previewController">実FBXの基準回転を取得するプレビュー。</param>
	/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
	/// <param name="totalFrames">MotionData の総フレーム数。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SetSelectedRotationKey(CustomizePreviewController& previewController, int keyFrame, int totalFrames);

	/// <summary>
	/// ギズモ操作で得た内部ローカル回転を、既存の数値入力と同じルールで現在フレームへ保存する。
	/// </summary>
	/// <param name="bodyPartIndex">編集された部位番号。</param>
	/// <param name="axisIndex">操作された軸番号。X=0, Y=1, Z=2。範囲外なら保存しない。</param>
	/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
	/// <param name="totalFrames">MotionData の総フレーム数。</param>
	/// <param name="localRotation">保存するローカル Quaternion。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string ApplyGizmoRotationEdit(
		int bodyPartIndex,
		int axisIndex,
		int keyFrame,
		int totalFrames,
		const DirectX::SimpleMath::Quaternion& localRotation);

	/// <summary>
	/// 現在フレームに姿勢キーフレームがあるか確認する。
	/// </summary>
	/// <param name="keyFrame">確認対象の内部 actionFrame。</param>
	/// <returns>少なくとも 1 部位のキーがあれば true。</returns>
	bool HasMotionKeyframe(int keyFrame) const;

	/// <summary>
	/// 現在フレームに汎用モーション用の見た目オフセットキーを追加する。
	/// </summary>
	/// <param name="keyFrame">追加先の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string AddRootOffsetKeyframe(int keyFrame);

	/// <summary>
	/// 現在フレームの汎用モーション用見た目オフセットキーを削除する。
	/// </summary>
	/// <param name="keyFrame">削除対象の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string DeleteRootOffsetKeyframe(int keyFrame);

	/// <summary>
	/// 現在フレームの汎用モーション用見た目オフセットキーを編集値で更新する。
	/// </summary>
	/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SetRootOffsetKeyframe(int keyFrame);

	/// <summary>
	/// 現在フレームに汎用モーション用見た目オフセットキーがあるか確認する。
	/// </summary>
	/// <param name="keyFrame">確認対象の内部 actionFrame。</param>
	/// <returns>rootOffsetKeys にキーがあれば true。</returns>
	bool HasRootOffsetKeyframe(int keyFrame) const;

	/// <summary>
	/// 現在フレームの全身姿勢をコピーする。
	/// </summary>
	/// <param name="keyFrame">コピー元の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string CopyWholeBodyPose(int keyFrame);

	/// <summary>
	/// コピー済み全身姿勢を現在フレームへ貼り付ける。
	/// </summary>
	/// <param name="keyFrame">貼り付け先の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string PasteWholeBodyPose(int keyFrame);

	/// <summary>
	/// 現在フレームへ指定プリセットの選択部位だけを適用する。
	/// </summary>
	/// <param name="presetId">読み込む姿勢プリセット ID。</param>
	/// <param name="keyFrame">適用先の内部 actionFrame。</param>
	/// <param name="applyMask">適用する部位だけ true の配列。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string ApplyPosePreset(
		const std::string& presetId,
		int keyFrame,
		const std::array<bool, MotionEditorBoneCount>& applyMask);

	/// <summary>
	/// MotionData 下書きを取得する。
	/// </summary>
	/// <returns>編集可能な MotionData 参照。</returns>
	MotionData& GetDraft();

	/// <summary>
	/// MotionData 下書きを取得する。
	/// </summary>
	/// <returns>読み取り専用の MotionData 参照。</returns>
	const MotionData& GetDraft() const;

	/// <summary>
	/// MotionData 下書きが有効か取得する。
	/// </summary>
	/// <returns>読み込みまたは新規作成済みなら true。</returns>
	bool HasDraft() const;

	/// <summary>
	/// 表示名入力用バッファを取得する。
	/// </summary>
	/// <returns>UI の文字入力に渡す UTF-8 固定長バッファ。</returns>
	std::array<char, MotionNameBufferSize>& GetDisplayNameBuffer();

	/// <summary>
	/// 選択中の編集部位番号を取得する。
	/// </summary>
	/// <returns>編集可能な部位番号参照。</returns>
	int& GetSelectedBoneIndex();

	/// <summary>
	/// 選択部位の回転編集値を取得する。
	/// </summary>
	/// <returns>編集可能なオイラー角参照。</returns>
	DirectX::SimpleMath::Vector3& GetRotationEulerDegrees();

	/// <summary>
	/// 汎用モーション用の見た目オフセット編集値を取得する。
	/// </summary>
	/// <returns>編集可能なオフセット参照。</returns>
	DirectX::SimpleMath::Vector3& GetRootOffsetKey();

	/// <summary>
	/// コピー済み姿勢が存在するか取得する。
	/// </summary>
	/// <returns>Copy Pose 済みなら true。</returns>
	bool HasCopiedPose() const;

	/// <summary>現在姿勢を指定名のプリセットとして保存する。UIの入力バッファには依存しない。</summary>
	/// <param name="keyFrame">保存元の内部フレーム。</param>
	/// <param name="name">重複しないプリセット名。</param>
	/// <returns>保存結果。</returns>
	std::string SavePosePreset(int keyFrame, const std::string& name);

private:
	/// <summary>
	/// 選択中 IK 対象関節のワールド位置を、現在のプレビュー姿勢から一時ターゲット入力へ読み込む。
	/// </summary>
	/// <param name="previewController">現在表示中のプレビュー姿勢を持つ Controller。</param>
	/// <param name="keyFrame">読み込んだターゲットが属する内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string CaptureCurrentIkTarget(CustomizePreviewController& previewController, int keyFrame);

	/// <summary>
	/// 選択中 IK 対象を解き、必要な親側関節の回転だけを現在フレームの MotionData キーへ保存する。
	/// </summary>
	/// <param name="previewController">現在表示中のプレビュー姿勢と IK 計算に使う Controller。</param>
	/// <param name="keyFrame">保存先の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string ApplySelectedIkTarget(CustomizePreviewController& previewController, int keyFrame);

	/// <summary>
	/// 保存前に MotionData のフレーム範囲とキー順を整える。
	/// </summary>
	void NormalizeDraftForSave();

	MotionData draft;
	bool hasDraft = false;
	std::array<char, MotionNameBufferSize> displayNameBuffer = {};
	int selectedBoneIndex = 0;
	bool multipleSelectionEnabled = false;
	std::array<bool, MotionBodyPartCount> selectedBodyParts{};
	DirectX::SimpleMath::Vector3 rotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rootOffsetKey = DirectX::SimpleMath::Vector3::Zero;
	std::array<DirectX::SimpleMath::Quaternion, MotionEditorBoneCount> copiedPoseRotations = {};
	bool hasCopiedPose = false;
	DirectX::SimpleMath::Vector3 ikTargetWorldPosition = DirectX::SimpleMath::Vector3::Zero;
	bool hasIkTargetWorldPosition = false;
	bool ikTargetEditing = false; // アクティブな位置入力だけが開始姿勢を保持する。
	int ikTargetKeyFrame = -1;
	int ikTargetBodyPartIndex = -1;
};
