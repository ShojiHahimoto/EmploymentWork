#include "Scene/CustomizeAttackScene.h"

#include <utility>
#include "Manager/SceneLoadRequest.h"

CustomizeAttackScene::CustomizeAttackScene(int initialWidth, int initialHeight, CustomizeAttackTarget target)
	: flowController(initialWidth, initialHeight, std::move(target))
{
}

/// <summary>準備済みの下書きを使って編集画面を初期化し、画面内の読み込み・保存に使うワーカーを渡す。</summary>
void CustomizeAttackScene::Enter()
{
	flowController.Enter(loadWorker, saveWorker);
}

/// <summary>編集プレビュー用の共通プレイヤーモデルを要求する。</summary>
/// <param name="request">遷移前に準備する素材一覧。</param>
void CustomizeAttackScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.uiImageIds = {"panel_flat_white", "button_primary_clean", "panel", "button", "button_primary",
						  "number_field", "checkbox_off", "checkbox_on", "label_background", "back", "reset",
						  "play", "pause", "previous", "next", "camera", "keyframe", "header", "playback_panel",
						  "playhead", "pencil", "up", "down", "motion", "header_accent", "phase_startup",
						  "phase_active", "phase_recovery", "section_accent", "undo", "redo", "rotate",
						  "translate", "add", "delete", "copy", "paste", "mirror", "preset_save",
						  "preset_load", "select_all", "deselect_all", "keyframe_movement"};
	request.motionIds = {"Common/Idle", "Common/JumpLoop"};
	request.prepareScene = [target = flowController.GetTarget()]
	{
		auto draft = CustomizeAttackFlowController::PrepareDraft(target);
		PreparedSceneLoad result;
		result.apply = [draft = std::move(draft)](Scene &scene) mutable
		{
			static_cast<CustomizeAttackScene &>(scene).flowController.ApplyPreparedDraft(std::move(draft));
		};

		return result;
	};
}

void CustomizeAttackScene::Exit()
{
	flowController.Exit();
	world.Clear();
}

void CustomizeAttackScene::Update()
{
	flowController.Update();
}

void CustomizeAttackScene::Draw(Renderer &renderer)
{
	flowController.DrawBackdrop(renderer);
	flowController.DrawPreview(renderer);
	flowController.DrawInterface(renderer);
}

void CustomizeAttackScene::OnResize(int newWidth, int newHeight)
{
	flowController.OnResize(newWidth, newHeight);
}

World &CustomizeAttackScene::GetWorld()
{
	return world;
}

const World &CustomizeAttackScene::GetWorld() const
{
	return world;
}
