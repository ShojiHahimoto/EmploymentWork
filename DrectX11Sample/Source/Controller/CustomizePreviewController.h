#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Data/AttackData.h"
#include "Data/MotionData.h"
#include "Data/MotionSkeletonDefinition.h"
#include "Data/SkeletonPose.h"
#include "System/Renderer.h"

#include <SimpleMath.h>

#include <array>
#include <string>

/// <summary>
/// カスタマイズ画面の3Dプレビュー状態と描画を管理する。
/// </summary>
class CustomizePreviewController
{
public:
	static constexpr int PreviewTextureWidth = 640;
	static constexpr int PreviewTextureHeight = 360;

	/// <summary>
	/// プレビュー用モデル、カメラ、RenderTexture を初期化する。
	/// </summary>
	void Initialize();

	/// <summary>
	/// プレビュー用 RenderTexture を解放する。
	/// </summary>
	void Release();

	/// <summary>
	/// Play 中なら表示フレームを進める。
	/// </summary>
	/// <param name="enabled">現在の画面がプレビュー再生対象なら true。</param>
	/// <param name="looping">終端到達時に 1F へ戻して再生継続するなら true。</param>
	/// <param name="totalFrames">0F Idle を含まない内部総フレーム数。</param>
	/// <returns>フレームが進んだ場合は true。</returns>
	bool UpdatePlayback(bool enabled, bool looping, int totalFrames);

	/// <summary>
	/// プレビュー再生を開始する。
	/// </summary>
	/// <param name="totalFrames">0F Idle を含まない内部総フレーム数。</param>
	void Play(int totalFrames);

	/// <summary>
	/// プレビュー再生を停止する。
	/// </summary>
	void Stop();

	/// <summary>
	/// 再生を止めて、指定フレーム数だけ手動で移動する。
	/// </summary>
	/// <param name="frameDelta">進めるフレーム数。負数なら戻す。</param>
	/// <param name="totalFrames">0F Idle を含まない内部総フレーム数。</param>
	void StepFrame(int frameDelta, int totalFrames);

	/// <summary>
	/// 現在フレームをプレビュー表示範囲へ収める。
	/// </summary>
	/// <param name="totalFrames">0F Idle を含まない内部総フレーム数。</param>
	void ClampCurrentFrame(int totalFrames);

	/// <summary>
	/// プレビュー用カメラでモデルを描画する。
	/// </summary>
	/// <param name="renderer">描画に使う Renderer。</param>
	/// <param name="region">本体ウィンドウの描画矩形。nullptr の場合は RenderTexture へ描画する。</param>
	/// <param name="attackData">技編集時のプレビューに使う AttackData。</param>
	/// <param name="motionData">モーション編集時のプレビューに使う MotionData。</param>
	/// <param name="editingCommonMotion">汎用モーション編集中なら true。</param>
	/// <param name="hasDraftMotion">motionData が有効な下書きなら true。</param>
	/// <param name="editingMotionDataId">現在編集している MotionData ID。</param>
	void Render(
		Renderer& renderer,
		const RECT* region,
		const AttackData& attackData,
		const MotionData& motionData,
		bool editingCommonMotion,
		bool hasDraftMotion,
		const std::string& editingMotionDataId,
		int selectedBodyPartIndex);

	/// <summary>
	/// プレビュー上の関節クリックで選択された部位を取り出す。
	/// </summary>
	/// <returns>クリック選択された部位番号。未選択なら -1。</returns>
	int ConsumePickedBodyPartIndex();

	/// <summary>
	/// 現在のプレビュー表示フレームを取得する。
	/// </summary>
	/// <returns>0F Idle を含むプレビュー表示フレーム。</returns>
	int GetCurrentFrame() const;

	/// <summary>
	/// 現在のプレビュー表示フレームを設定する。
	/// </summary>
	/// <param name="frame">0F Idle を含むプレビュー表示フレーム。</param>
	void SetCurrentFrame(int frame);

	/// <summary>
	/// 現在の表示フレームを内部 actionFrame へ変換する。
	/// </summary>
	/// <returns>0F Idle は -1、1F 以降は 0 始まりの内部 actionFrame。</returns>
	int GetActionFrame() const;

	/// <summary>
	/// プレビューが再生中か取得する。
	/// </summary>
	/// <returns>再生中なら true。</returns>
	bool IsPlaying() const;

	/// <summary>
	/// プレビュー用 RenderTexture を取得する。
	/// </summary>
	/// <returns>RenderTexture 参照。</returns>
	const Renderer::RenderTexture& GetRenderTexture() const;

	/// <summary>
	/// プレビューカメラの yaw 角度を編集する参照を取得する。
	/// </summary>
	/// <returns>yaw 角度の参照。</returns>
	float& GetCameraYawDegrees();

	/// <summary>
	/// プレビューカメラの pitch 角度を編集する参照を取得する。
	/// </summary>
	/// <returns>pitch 角度の参照。</returns>
	float& GetCameraPitchDegrees();

	/// <summary>
	/// プレビューカメラ距離を編集する参照を取得する。
	/// </summary>
	/// <returns>距離の参照。</returns>
	float& GetCameraDistance();

private:
	/// <summary>
	/// プレビューキャラを中心に回り込むオービットカメラの Transform を更新する。
	/// </summary>
	void UpdateCameraTransform();

	/// <summary>
	/// 現在フレームが active 範囲なら技の AttackBox を描画する。
	/// </summary>
	/// <param name="renderer">DebugBox 描画に使う Renderer。</param>
	/// <param name="attackData">表示する技データ。</param>
	/// <param name="editingCommonMotion">汎用モーション編集中なら true。</param>
	void DrawAttackBoxes(Renderer& renderer, const AttackData& attackData, bool editingCommonMotion);

	/// <summary>
	/// 関節マーカーを画面へ重ね、クリックされた部位を記録する。
	/// </summary>
	/// <param name="region">プレビュー描画矩形。</param>
	/// <param name="model">部位名から実ボーンを解決するモデル。</param>
	/// <param name="selectedBodyPartIndex">現在選択中の部位番号。</param>
	void DrawJointMarkers(
		const RECT& region,
		const ModelResource& model,
		int selectedBodyPartIndex);

	Renderer::RenderTexture renderTexture;
	CameraComponent camera;
	TransformComponent cameraTransform;
	TransformComponent playerTransform;
	SkeletonPose skeletonPose;
	int currentFrame = 0;
	bool playing = false;
	int pickedBodyPartIndex = -1;
	float cameraYawDegrees = 0.0f;
	float cameraPitchDegrees = -2.5f;
	float cameraDistance = 14.0f;
};
