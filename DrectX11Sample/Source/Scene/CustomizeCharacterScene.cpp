#include "Scene/CustomizeCharacterScene.h"

#include "Manager/SceneLoadRequest.h"

#include <unordered_set>

/// <summary>編集対象と初期画面サイズを保持する。</summary>
/// <param name="initialWidth">初期画面幅。</param>
/// <param name="initialHeight">初期画面高さ。</param>
/// <param name="slotIndex">編集するキャラクタースロット番号。</param>
CustomizeCharacterScene::CustomizeCharacterScene(int initialWidth, int initialHeight, int slotIndex)
	: flow(initialWidth, initialHeight), slotIndex(slotIndex)
{
}

/// <summary>準備済みデータで編集UIとプレビューを開始する。</summary>
void CustomizeCharacterScene::Enter()
{
	flow.Enter(world, saveWorker);
}

/// <summary>編集下書き、作成済み技とその再生素材を遷移前に準備する。</summary>
/// <param name="request">ロードワーカーへ渡す要求。</param>
void CustomizeCharacterScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.bgmId = "BGM/BGM_CustomizeEditor_01_maou";
	flow.DescribeLoad(request);
	request.prepareScene = [index = slotIndex]
	{
		CustomizeCharacterEditorController draft;
		draft.SelectSlot(index);
		draft.PrepareAttackPicker();

		PreparedSceneLoad result;
		std::unordered_set<std::string> motions;
		for (const auto &attack : draft.GetAttackPickerItems())
		{
			if (!attack.motionDataId.empty() && motions.insert(attack.motionDataId).second)
			{
				result.motionIds.push_back(attack.motionDataId);
			}
		}

		result.apply = [draft = std::move(draft)](Scene &scene) mutable
		{
			static_cast<CustomizeCharacterScene &>(scene).flow.ApplyPrepared(std::move(draft));
		};

		return result;
	};
}

void CustomizeCharacterScene::Exit()
{
	flow.Exit();
	world.Clear();
}

void CustomizeCharacterScene::Update()
{
	flow.Update(world);
}

void CustomizeCharacterScene::Draw(Renderer &renderer)
{
	flow.Draw(world, renderer);
}

void CustomizeCharacterScene::OnResize(int newWidth, int newHeight)
{
	flow.Resize(newWidth, newHeight);
}

World &CustomizeCharacterScene::GetWorld()
{
	return world;
}

const World &CustomizeCharacterScene::GetWorld() const
{
	return world;
}
