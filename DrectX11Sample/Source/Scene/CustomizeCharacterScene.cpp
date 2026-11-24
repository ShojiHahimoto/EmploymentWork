#include "Scene/CustomizeCharacterScene.h"

#include "Input/InputSystem.h"
#include "Manager/SceneManager.h"
#include "Scene/CustomizeSelectScene.h"

#include <memory>

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
	statusMessage = editor.SelectSlot(slotIndex);
}

void CustomizeCharacterScene::Exit()
{
	world.Clear();
}

/// <summary>キャンセル入力で技選択を閉じるか、キャラクター一覧へ戻る。</summary>
void CustomizeCharacterScene::Update()
{
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
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<CustomizeSelectScene>(width, height, CustomizeMode::CharacterSlotSelect));
}
