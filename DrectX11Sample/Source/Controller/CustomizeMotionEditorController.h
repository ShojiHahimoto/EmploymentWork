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
	static constexpr int MotionNameBufferSize = 128;
	static constexpr int MotionEditorBoneCount = MotionBodyPartCount;

	/// <summary>
	/// MotionData 編集 UI と、キーフレーム編集操作を描画/実行する。
	/// </summary>
	/// <param name="attackData">攻撃モーション時に移動キーを編集する AttackData。</param>
	/// <param name="previewController">現在フレームとタイムライン操作に使うプレビュー制御。</param>
	/// <param name="editingCommonMotion">汎用モーション編集中なら true。</param>
	/// <param name="totalFrames">0F Idle を含まない総フレーム数。</param>
	/// <param name="statusMessage">画面に表示する処理結果メッセージ。</param>
	/// <param name="attackMovementKeyOffset">攻撃移動キーの編集値。</param>
	/// <returns>Save MotionData が押された場合は true。</returns>
	bool DrawEditor(
		AttackData& attackData,
		CustomizePreviewController& previewController,
		bool editingCommonMotion,
		int totalFrames,
		std::string& statusMessage,
		DirectX::SimpleMath::Vector2& attackMovementKeyOffset);

	/// <summary>
	/// 攻撃モーション編集用に、MotionData 下書きと編集バッファを初期化する。
	/// </summary>
	void ResetForAttackMotion();

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
	/// 現在の下書き内容を ImGui 入力用バッファへコピーする。
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
	/// <param name="keyFrame">更新対象の内部 actionFrame。</param>
	/// <param name="totalFrames">MotionData の総フレーム数。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string SetSelectedRotationKey(int keyFrame, int totalFrames);

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
	/// 現在フレームへ T ポーズ回転を適用する。
	/// </summary>
	/// <param name="keyFrame">適用先の内部 actionFrame。</param>
	/// <returns>ユーザー表示用の処理結果メッセージ。</returns>
	std::string ApplyTPosePreset(int keyFrame);

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
	/// <returns>ImGui InputText に渡す固定長バッファ。</returns>
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

private:
	/// <summary>
	/// 保存前に MotionData のフレーム範囲とキー順を整える。
	/// </summary>
	void NormalizeDraftForSave();

	MotionData draft;
	bool hasDraft = false;
	std::array<char, MotionNameBufferSize> displayNameBuffer = {};
	int selectedBoneIndex = 0;
	DirectX::SimpleMath::Vector3 rotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 rootOffsetKey = DirectX::SimpleMath::Vector3::Zero;
	std::array<DirectX::SimpleMath::Vector3, MotionEditorBoneCount> copiedPoseRotations = {};
	bool hasCopiedPose = false;
};
