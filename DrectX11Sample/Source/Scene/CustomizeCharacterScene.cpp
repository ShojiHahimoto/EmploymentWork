#include "Scene/CustomizeCharacterScene.h"

#include "Input/InputSystem.h"
#include "Manager/SceneManager.h"
#include "Manager/SceneLoadRequest.h"
#include "Manager/LoadWorker.h"
#include "Manager/SaveWorker.h"
#include "Scene/CustomizeSelectScene.h"

#include <memory>
#include <chrono>

/// <summary>編集対象と選択画面へ戻る際の画面サイズを保持する。</summary>
/// <param name="initialWidth">初期画面幅。</param>
/// <param name="initialHeight">初期画面高さ。</param>
/// <param name="slotIndex">編集するキャラクタースロット番号。</param>
CustomizeCharacterScene::CustomizeCharacterScene(int initialWidth, int initialHeight, int slotIndex)
	: width(initialWidth), height(initialHeight), slotIndex(slotIndex)
{
}

/// <summary>選択されたキャラクターを編集用の下書きとして開く。</summary>
void CustomizeCharacterScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
}

/// <summary>キャラクター編集の下書きを遷移前に準備する。</summary>
/// <param name="request">ワーカーへ渡すロード要求。</param>
void CustomizeCharacterScene::DescribeLoad(SceneLoadRequest &request) const
{
	request.prepareScene = [index = slotIndex]
	{
		CustomizeCharacterEditorController draft;
		auto status = draft.SelectSlot(index);
		draft.PrepareAttackPicker();

		PreparedSceneLoad result;
		result.apply = [draft = std::move(draft), status = std::move(status)](Scene &scene) mutable
		{
			auto &editorScene = static_cast<CustomizeCharacterScene &>(scene);
			editorScene.editor = std::move(draft);
			editorScene.statusMessage = std::move(status);
		};

		return result;
	};
}

void CustomizeCharacterScene::Exit()
{
	world.Clear();
}

/// <summary>キャンセル入力で技選択を閉じるか、キャラクター一覧へ戻る。</summary>
void CustomizeCharacterScene::Update()
{
	if (pendingSave.valid() && pendingSave.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		try
		{
			statusMessage = pendingSave.get();
		}
		catch (const std::exception &)
		{
			statusMessage = "Character save failed.";
		}
	}

	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const auto &cancel = Input::InputSystem::GetActionState(playerIndex, Input::InputActionId::Cancel);

		if (!cancel.trigger)
		{
			continue;
		}

		if (mode == CustomizeMode::AttackPicker)
		{
			mode = CustomizeMode::CharacterEditor;
		}
		else
		{
			RequestSlotSelection();
		}

		return;
	}
}

/// <summary>既存の編集UIまたは技選択UIを描画し、戻る操作を振り分ける。</summary>
/// <param name="renderer">シーン共通のレンダラー。ImGuiで描画するため直接は使用しない。</param>
void CustomizeCharacterScene::Draw([[maybe_unused]] Renderer &renderer)
{
	if (mode == CustomizeMode::AttackPicker)
	{
		mode = editor.DrawAttackPicker(statusMessage);

		return;
	}

	const auto nextMode = editor.DrawEditor(statusMessage);
	if (editor.ConsumeSaveRequest() && saveWorker && !pendingSave.valid())
	{
		statusMessage = "Saving character...";
		pendingSave = saveWorker->Enqueue([draft = editor]() mutable
		{
			return draft.SaveDraft();
		});
	}

	if (nextMode == CustomizeMode::CharacterSlotSelect)
	{
		RequestSlotSelection();

		return;
	}

	mode = nextMode;
}

void CustomizeCharacterScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

World &CustomizeCharacterScene::GetWorld()
{
	return world;
}

const World &CustomizeCharacterScene::GetWorld() const
{
	return world;
}

/// <summary>キャラクタースロット一覧への遷移を予約する。</summary>
void CustomizeCharacterScene::RequestSlotSelection()
{
	if (pendingSave.valid())
	{
		return;
	}

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeSelectScene>(width, height, CustomizeMode::CharacterSlotSelect));
}
