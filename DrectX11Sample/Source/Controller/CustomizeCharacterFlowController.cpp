#include "Controller/CustomizeCharacterFlowController.h"

#pragma execution_character_set("utf-8")
#include "Component/CharacterAttackDataComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MotionPlayerComponent.h"
#include "Component/SkeletonPoseComponent.h"
#include "Core/Debugger.h"
#include "Core/World.h"
#include "Input/InputSystem.h"
#include "Input/UIInputSystem.h"
#include "Manager/SaveWorker.h"
#include "Manager/SceneLoadRequest.h"
#include "Manager/SceneManager.h"
#include "Resource/ModelResource.h"
#include "Scene/CustomizeSelectScene.h"
#include "System/CommandInputSystem.h"
#include "System/EmbedResolveSystem.h"
#include "System/MotionSystem.h"
#include "System/MovementSystem.h"
#include "System/PlayerControlSystem.h"
#include "System/SpawnDestroySystem.h"
#include "System/StateUpdateSystem.h"
#include "System/TransformSystem.h"
#include "UI/CustomizeEditorUI.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace
{
using namespace DirectX::SimpleMath;
constexpr const char *PreviewAttackSlot = "PreviewAttack";
constexpr int AirAttackDelayFrames = 5;
constexpr size_t HistoryLimit = 100;
constexpr int HeaderHeight = 72;
constexpr int RowHeight = 53;
constexpr int SectionHeight = 42;
constexpr const char *GroupNames[] = {"地上技", "空中技", "必殺技"};
constexpr const char *Badges[] = {"badge_a", "badge_b", "badge_x", "badge_y"};

/// <summary>技情報の矩形を、フレーム三列と任意のコマンド列に分ける。</summary>
/// <param name="bounds">フレーム情報全体の矩形。</param>
/// <param name="command">コマンド列が必要なら true。</param>
/// <returns>発生、持続、硬直、コマンドの順の矩形。</returns>
std::array<GameUI::Rect, 4> AttackColumns(const GameUI::Rect &bounds, bool command)
{
	const int right = command ? bounds.left + (bounds.right - bounds.left) * 60 / 100 : bounds.right;
	const int width = right - bounds.left;
	std::array<GameUI::Rect, 4> result;
	for (int i = 0; i < 3; ++i)
	{
		result[i] = {bounds.left + width * i / 3, bounds.top, bounds.left + width * (i + 1) / 3, bounds.bottom};
	}

	result[3] = {right, bounds.top, bounds.right, bounds.bottom};

	return result;
}

/// <summary>数値と同じ列にフレーム情報の見出しを登録する。</summary>
/// <param name="root">登録先のUI。</param>
/// <param name="id">見出しの一意なID。</param>
/// <param name="bounds">フレーム情報の矩形。</param>
/// <param name="command">コマンド列が必要なら true。</param>
void AddFrameHeaders(GameUI::Root &root, const std::string &id, const GameUI::Rect &bounds, bool command)
{
	const auto columns = AttackColumns(bounds, command);
	const char *labels[] = {"発生", "持続", "硬直"};
	for (int i = 0; i < 3; ++i)
	{
		const float available = static_cast<float>(columns[i].right - columns[i].left - 8);
		const float fontSize = std::clamp(available / 2.0f, 8.0f, 14.0f);
		root.AddLabel(id + "/" + std::to_string(i), columns[i], labels[i], true, fontSize, true);
	}
}

/// <summary>必殺技の右向き基準の方向列を表示用の矢印にする。</summary>
/// <param name="command">表示対象のコマンド。</param>
/// <returns>表示用の矢印列。</returns>
std::string CommandArrows(AttackCommandId command)
{
	if (command == AttackCommandId::FullRotate)
	{
		return "↻";
	}

	const char *arrows[] = {"", "↙", "↓", "↘", "←", "", "→", "↖", "↑", "↗"};
	std::string result;
	for (const auto &step : CommandInputSystem::GetDirectionalCommand(command))
	{
		result += arrows[step.direction];
	}

	return result;
}
}

/// <summary>初期画面サイズと編集対象を保持する。</summary>
/// <param name="width">初期画面幅。</param>
/// <param name="height">初期画面高さ。</param>
CustomizeCharacterFlowController::CustomizeCharacterFlowController(int width, int height)
	: width(width), height(height)
{
}

/// <summary>共通エディター素材とプレビューのモデルおよび汎用モーションを要求する。</summary>
/// <param name="request">シーン遷移時のロード要求。</param>
void CustomizeCharacterFlowController::DescribeLoad(SceneLoadRequest &request) const
{
	request.uiImageIds = {"header", "header_accent", "panel", "button", "button_primary_clean",
		"button_primary", "number_field", "back", "undo", "redo", "play", "section_accent",
		"badge_a", "badge_b", "badge_x", "badge_y", "slot_focused"};
	request.motionIds = {"Common/Idle", "Common/JumpStart", "Common/JumpLoop"};
}

/// <summary>準備済みの下書きを受け取る。</summary>
/// <param name="prepared">ロードワーカーが準備した下書きと技一覧。</param>
void CustomizeCharacterFlowController::ApplyPrepared(CustomizeCharacterEditorController prepared)
{
	editor = std::move(prepared);
	name = editor.GetCharacterNameBuffer().data();
}

/// <summary>入力と待機プレビューを開始する。</summary>
/// <param name="world">プレビュー用 World。</param>
/// <param name="writer">キャラクター保存ワーカー。</param>
void CustomizeCharacterFlowController::Enter(World &world, SaveWorker *writer)
{
	saveWorker = writer;
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	ResetPreview(world, nullptr);
}

/// <summary>プレビュー用の描画リソースを解放する。</summary>
void CustomizeCharacterFlowController::Exit()
{
	Renderer::ReleaseRenderTexture(preview);
}

/// <summary>更新先の画面サイズを設定する。</summary>
/// <param name="width">新しい画面幅。</param>
/// <param name="height">新しい画面高さ。</param>
void CustomizeCharacterFlowController::Resize(int width, int height)
{
	if (width > 0 && height > 0)
	{
		this->width = width;
		this->height = height;
	}
}

/// <summary>入力を処理し、プレビューを一フレーム進める。</summary>
/// <param name="world">プレビュー用 World。</param>
void CustomizeCharacterFlowController::Update(World &world)
{
	PollSave();
	const auto before = Capture();
	const bool wasEditingText = root.WantsTextInput();
	BuildUI();
	ResizePreviewTexture();
	const bool editingName = root.GetEditInteraction().id == "text/character/name" &&
		root.GetEditInteraction().ongoing;
	if (before.name != name && !nameEdit)
	{
		nameEdit = before;
	}

	if (!editingName)
	{
		CommitName();
	}

	ApplyUI(world, wasEditingText);
	UpdatePreview(world);
}

/// <summary>プレビューと操作パネルを順に描画する。</summary>
/// <param name="world">プレビュー用 World。</param>
/// <param name="renderer">専用UIの描画先。</param>
void CustomizeCharacterFlowController::Draw(World &world, Renderer &renderer) const
{
	Renderer::DrawScreenRect({0, 0, width, height}, Color(.94f, .95f, .97f, 1));
	root.Draw(renderer);
	DrawPreview(world);
}

/// <summary>名前と全割り当てを履歴用に取得する。</summary>
/// <returns>現在の編集状態。</returns>
CustomizeCharacterFlowController::Snapshot CustomizeCharacterFlowController::Capture() const
{
	Snapshot result;
	result.name = name;
	for (int group = 0; group < 3; ++group)
	{
		result.groups[group] = editor.GetAttackSlotDrafts(static_cast<CustomizeCharacterAttackSlotGroup>(group));
	}

	return result;
}

/// <summary>履歴の名前と割り当てを下書きへ戻す。</summary>
/// <param name="snapshot">適用する編集状態。</param>
void CustomizeCharacterFlowController::Restore(const Snapshot &snapshot)
{
	name = snapshot.name;
	for (int group = 0; group < 3; ++group)
	{
		editor.GetAttackSlotDrafts(static_cast<CustomizeCharacterAttackSlotGroup>(group)) = snapshot.groups[group];
	}
}

/// <summary>名前欄の連続入力を一操作として確定する。</summary>
void CustomizeCharacterFlowController::CommitName()
{
	if (!nameEdit)
	{
		return;
	}

	if (nameEdit->name != name)
	{
		Record(std::move(*nameEdit));
	}

	nameEdit.reset();
}

/// <summary>変更前の状態を履歴に積み、古い分岐を破棄する。</summary>
/// <param name="before">変更前の編集状態。</param>
void CustomizeCharacterFlowController::Record(Snapshot before)
{
	if (undo.size() >= HistoryLimit)
	{
		undo.erase(undo.begin());
	}

	undo.push_back(std::move(before));
	redo.clear();
}

/// <summary>最後に確定した編集を取り消す。</summary>
void CustomizeCharacterFlowController::Undo()
{
	CommitName();
	if (!undo.empty())
	{
		redo.push_back(Capture());
		Restore(undo.back());
		undo.pop_back();
	}
}

/// <summary>取り消した編集をやり直す。</summary>
void CustomizeCharacterFlowController::Redo()
{
	CommitName();
	if (!redo.empty())
	{
		undo.push_back(Capture());
		Restore(redo.back());
		redo.pop_back();
	}
}

/// <summary>共通ヘッダーと二つのリストを登録して入力を処理する。</summary>
void CustomizeCharacterFlowController::BuildUI()
{
	auto frame = GameUI::UIInputSystem::GetFrame();
	GameUI::UIInputSystem::AddKeyboardSubmit(frame, root.WantsTextInput());
	frame.cancelPressed |= frame.rightPointerPressed && !frame.focusLost &&
		!previewRegion.Contains(frame.rightPressPosition);
	root.BeginFrame({0, 0, width, height}, frame);
	const int titleRight = width * 36 / 100;
	const float titleSize = std::clamp(static_cast<float>(titleRight - 146) / 10.0f, 15.0f, 34.0f);
	CustomizeEditorUI::AddHeader(root, width, HeaderHeight, titleRight, "キャラクターの編集", titleSize);
	root.AddLabel("header/name", {width * 36 / 100, 0, width * 47 / 100, HeaderHeight},
		"キャラクター名", true, 19);
	root.AddTextInput("character/name", {width * 47 / 100, 10, width * 77 / 100, HeaderHeight - 10},
		name, CustomizeCharacterEditorController::CharacterNameBufferSize - 1, true, true);
	CustomizeEditorUI::AddHistorySaveButtons(root, width, HeaderHeight, !undo.empty() || nameEdit.has_value(),
		!redo.empty(), saveWorker && !pendingSave.valid());

	constexpr int gap = 5;
	const int bottom = std::max(HeaderHeight + 60, height - gap);
	const int top = HeaderHeight + gap;
	const int pickerLeft = width - gap - width * 34 / 100;
	const int assignmentRight = pickerOpen ? pickerLeft - gap : width - gap;
	const GameUI::Rect assignments = {assignmentRight - width * 36 / 100, top, assignmentRight, bottom};
	const GameUI::Rect previewPanel = {gap, top, assignments.left - gap, bottom};
	root.AddPanel(previewPanel);
	previewRegion = previewPanel.Inset(13, 12);

	root.AddPanel(assignments);
	root.AddLabel("status", {assignments.left + 12, bottom - 34, assignments.right - 12, bottom - 5}, status);
	BuildSlots({assignments.left + 15, assignments.top + 14, assignments.right - 15, bottom - 39});

	pickerRows.clear();
	pickerRegion = {};
	if (pickerOpen)
	{
		pickerRegion = {pickerLeft, top, width - gap, bottom};
		root.AddPanel(pickerRegion);
		BuildPicker(pickerRegion.Inset(15, 14));
	}

	root.EndFrame();
}

/// <summary>編集対象の技スロットをカテゴリ別のスクロール領域に並べる。</summary>
/// <param name="bounds">割り当てパネル内の表示領域。</param>
void CustomizeCharacterFlowController::BuildSlots(const GameUI::Rect &bounds)
{
	const int listWidth = bounds.right - bounds.left - 8;
	const int pickRight = listWidth - 55;
	const auto groupSelected = editor.GetPickingSlotGroup();
	const int indexSelected = editor.GetPickingSlotIndex();
	root.BeginScrollArea("assignments", bounds, 3 * (SectionHeight + 4 * RowHeight + 8), slotScroll);

	int y = 0;
	for (int group = 0; group < 3; ++group)
	{
		const auto slotGroup = static_cast<CustomizeCharacterAttackSlotGroup>(group);
		const bool special = group == 2;
		const int fieldsWidth = std::min(special ? 222 : 156, pickRight * 54 / 100);
		const int namesEnd = pickRight - 4 - fieldsWidth;
		root.AddSectionHeader("group/" + std::to_string(group), {0, y, listWidth, y + SectionHeight},
			GroupNames[group], 24);
		AddFrameHeaders(root, "group/frames/" + std::to_string(group),
			{namesEnd, y, pickRight - 4, y + SectionHeight}, special);
		y += SectionHeight;

		const auto &drafts = editor.GetAttackSlotDrafts(slotGroup);
		for (int button = 0; button < 4; ++button)
		{
			const auto &draft = drafts[button];
			const auto *attack = editor.FindAttack(draft.attackDataId);
			const std::string id = "slot/" + std::to_string(group * 4 + button);
			const bool selected = pickerOpen && groupSelected == slotGroup && indexSelected == button;
			root.AddButton(id, {0, y, pickRight, y + 46}, true, selected ? "slot_focused" : "button");
			root.AddImage(id + "/badge", {9, y + 8, 39, y + 38}, Badges[button]);
			root.AddLabel(id + "/name", {46, y, namesEnd, y + 46},
				draft.attackDataId.empty() ? "空" : draft.attackDisplayName, true, 18, false, true);
			if (attack)
			{
				AddAttackFields(id, *attack, {namesEnd, y, pickRight - 4, y + 46}, special);
			}

			root.AddIconButton(id + "/play", {pickRight + 5, y, listWidth, y + 46}, "", "play",
				attack && !attack->motionDataId.empty(), false);
			y += RowHeight;
		}

		y += 8;
	}

	root.EndScrollArea();
}

/// <summary>選択中の割り当て枠に対応する作成済み技だけを右に表示する。</summary>
/// <param name="bounds">右パネルの内側矩形。</param>
void CustomizeCharacterFlowController::BuildPicker(const GameUI::Rect &bounds)
{
	const auto group = editor.GetPickingSlotGroup();
	const auto &items = editor.GetAttackPickerItems();
	const auto &current = editor.GetAttackSlotDrafts(group)[editor.GetPickingSlotIndex()];
	for (size_t index = 0; index < items.size(); ++index)
	{
		if (editor.IsAttackCompatibleWithSlotGroup(group, items[index]))
		{
			pickerRows.push_back(index);
		}
	}

	root.AddSectionHeader("picker/header", {bounds.left, bounds.top, bounds.right, bounds.top + 46},
		std::string(GroupNames[static_cast<int>(group)]) + "を選択", 23);
	const int listWidth = bounds.right - bounds.left - 8;
	const bool special = group == CustomizeCharacterAttackSlotGroup::Special;
	const int split = listWidth * (special ? 38 : 48) / 100;
	AddFrameHeaders(root, "picker/columns", {bounds.left + split, bounds.top + 48,
		bounds.left + listWidth - 5, bounds.top + 78}, special);
	root.BeginScrollArea("picker/scroll", {bounds.left, bounds.top + 80, bounds.right, bounds.bottom},
		static_cast<int>(pickerRows.size() + 1) * RowHeight, pickerScroll);
	root.AddButton("picker/clear", {0, 0, listWidth, 46}, "割り当てを外す", !current.attackDataId.empty());
	for (size_t row = 0; row < pickerRows.size(); ++row)
	{
		const auto &attack = items[pickerRows[row]];
		const std::string id = "picker/" + std::to_string(row);
		const int y = (static_cast<int>(row) + 1) * RowHeight;
		const bool selected = current.attackDataId == attack.attackDataId;
		root.AddButton(id, {0, y, listWidth, y + 46}, true, selected ? "slot_focused" : "button");
		root.AddLabel(id + "/name", {10, y, split, y + 46}, attack.displayName, true, 18, false, true);
		AddAttackFields(id, attack, {split, y, listWidth - 5, y + 46}, special);
	}

	root.EndScrollArea();
}

/// <summary>技のフレーム情報と必要な場合だけコマンドを描画する。</summary>
/// <param name="id">行の入力ID。</param>
/// <param name="attack">値を表示する技。</param>
/// <param name="bounds">フレーム情報とコマンドの領域。</param>
/// <param name="command">必殺技の方向列も表示するか。</param>
void CustomizeCharacterFlowController::AddAttackFields(std::string_view id, const AttackData &attack,
	const GameUI::Rect &bounds, bool command)
{
	const auto columns = AttackColumns(bounds, command);
	const auto &frame = attack.frame;
	const int values[] = {frame.startup, frame.active, frame.recovery};
	for (int i = 0; i < 3; ++i)
	{
		root.AddLabel(std::string(id) + "/frames/" + std::to_string(i), columns[i],
			std::to_string(values[i]), true, 16, true);
	}

	if (command)
	{
		root.AddLabel(std::string(id) + "/command", columns[3],
			CommandArrows(attack.commandId), true, 16, true, true);
	}
}

/// <summary>ボタンとキャンセル入力を編集・保存・プレビューへ振り分ける。</summary>
/// <param name="world">プレビュー用 World。</param>
/// <param name="wasEditingText">入力処理の前に名前欄を編集中だったか。</param>
void CustomizeCharacterFlowController::ApplyUI(World &world, bool wasEditingText)
{
	const auto &input = GameUI::UIInputSystem::GetFrame();
	bool cancel = root.WasActivated("back");
	if (!wasEditingText && !input.composing && !input.focusLost)
	{
		cancel |= input.rightPointerPressed && !previewRegion.Contains(input.rightPressPosition);
		for (int player = 0; player < Input::MaxPlayers; ++player)
		{
			cancel |= Input::InputSystem::GetActionState(player, Input::InputActionId::Cancel).trigger;
		}
	}

	if (cancel)
	{
		if (pickerOpen)
		{
			pickerOpen = false;
		}
		else
		{
			NavigateBack();
		}

		return;
	}

	const auto &focused = root.GetFocusedId();
	// Reflow after release so the layout cannot move under a captured button.
	if (pickerOpen && input.pointerReleased && !pickerRegion.Contains(input.pointerPosition) &&
		!focused.starts_with("slot/"))
	{
		pickerOpen = false;
	}

	if (root.WasActivated("undo"))
	{
		Undo();

		return;
	}

	if (root.WasActivated("redo"))
	{
		Redo();

		return;
	}

	if (root.WasActivated("save"))
	{
		Save();

		return;
	}

	for (int i = 0; i < 12; ++i)
	{
		const auto group = static_cast<CustomizeCharacterAttackSlotGroup>(i / 4);
		const int button = i % 4;
		const auto id = "slot/" + std::to_string(i);
		if (root.WasActivated(id + "/play"))
		{
			const auto &draft = editor.GetAttackSlotDrafts(group)[button];
			const auto *attack = editor.FindAttack(draft.attackDataId);
			if (attack && MotionDataManager::GetMotionData(attack->motionDataId))
			{
				ResetPreview(world, attack);
			}
			else
			{
				status = "再生できるモーションがありません";
			}

			return;
		}

		if (root.WasActivated(id))
		{
			const bool sameSlot = pickerOpen && editor.GetPickingSlotGroup() == group &&
				editor.GetPickingSlotIndex() == button;
			if (!sameSlot)
			{
				pickerScroll = 0;
			}

			editor.SetPickingSlotGroup(group);
			editor.SetPickingSlotIndex(button);
			pickerOpen = true;

			return;
		}
	}

	if (!pickerOpen)
	{
		return;
	}

	const AttackData *chosen = nullptr;
	for (size_t row = 0; row < pickerRows.size(); ++row)
	{
		if (root.WasActivated("picker/" + std::to_string(row)))
		{
			chosen = &editor.GetAttackPickerItems()[pickerRows[row]];

			break;
		}
	}

	const bool clear = root.WasActivated("picker/clear");
	if (chosen || clear)
	{
		const auto group = editor.GetPickingSlotGroup();
		const auto &current = editor.GetAttackSlotDrafts(group)[editor.GetPickingSlotIndex()];
		const std::string id = chosen ? chosen->attackDataId : "";
		if (id != current.attackDataId)
		{
			CommitName();
			auto before = Capture();
			editor.AssignAttackToCurrentSlot(id, chosen ? *chosen : AttackData{});
			Record(std::move(before));
		}

		pickerOpen = false;
	}
}

/// <summary>保存中でなければキャラクタースロット一覧に戻る。</summary>
void CustomizeCharacterFlowController::NavigateBack()
{
	if (!pendingSave.valid())
	{
		SceneManager::GetInstance().RequestChangeScene(
			std::make_unique<CustomizeSelectScene>(width, height, CustomizeMode::CharacterSlotSelect));
	}
}

/// <summary>クリック時点の下書きを共通保存ワーカーに渡す。</summary>
void CustomizeCharacterFlowController::Save()
{
	if (!saveWorker || pendingSave.valid())
	{
		return;
	}

	CommitName();
	auto &buffer = editor.GetCharacterNameBuffer();
	std::snprintf(buffer.data(), buffer.size(), "%s", name.c_str());
	status = "保存中…";
	pendingSave = saveWorker->Enqueue([draft = editor]() mutable
	{
		return draft.SaveDraft();
	});
}

/// <summary>保存完了を待機せずに取得し、結果を表示する。</summary>
void CustomizeCharacterFlowController::PollSave()
{
	if (!pendingSave.valid() || pendingSave.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}

	try
	{
		const auto result = pendingSave.get();
		if (result.starts_with("Saved:"))
		{
			status = "保存しました";
		}
		else if (result == "Character name is required.")
		{
			status = "キャラクター名を入力してください";
		}
		else if (result.starts_with("Required slot is empty:"))
		{
			status = "地上技と空中技をすべて割り当ててください";
		}
		else
		{
			status = "保存できませんでした";
		}
	}
	catch (const std::exception &error)
	{
		DebugLog("[Customize] Character save failed: ", error.what());
		status = "保存できませんでした";
	}
}

/// <summary>位置・速度・状態・モーションを初期化し、指定の技を最初から再生する。</summary>
/// <param name="world">プレビュー専用 World。</param>
/// <param name="attack">再生する技。nullptr の場合は待機から開始。</param>
void CustomizeCharacterFlowController::ResetPreview(World &world, const AttackData *attack)
{
	world.Clear();
	pendingAirAttack = false;
	initialAction = true;
	airborneFrames = 0;

	auto character = std::make_shared<CharacterData>();
	character->parameter = editor.GetDraftParameter();
	if (attack)
	{
		CharacterAssignedAttackData assigned;
		assigned.slotId = PreviewAttackSlot;
		assigned.attack = *attack;
		assigned.slotType = attack->attackKind == AttackKind::Special ? AttackSlotType::Special : AttackSlotType::Normal;
		assigned.slotUsableState = attack->usableState;
		character->attacks.push_back(std::move(assigned));
	}

	world.RequestSpawn(SpawnType::DebugPlayer, "CharacterPreview", Vector3::Zero, Vector3::Zero, "",
		std::move(character));
	SpawnDestroySystem::Update(world);
	if (world.GetGameObjects().empty())
	{
		return;
	}

	const auto id = world.GetGameObjects().front().id;

	if (auto *motion = world.GetComponent<MotionPlayerComponent>(id))
	{
		motion->forceIdleLoop = true;
	}

	if (!attack)
	{
		return;
	}

	if (attack->usableState == AttackUsableState::Air)
	{
		pendingAirAttack = true;
		StateUpdateSystem::StartAction(world, id, {PlayerActionState::VerticalJumpStartup, true});
	}
	else
	{
		StateUpdateSystem::StartAction(world, id, {PlayerActionState::GroundAttack, true, PreviewAttackSlot});
	}
}

/// <summary>対戦と同じ順で一体分の状態、重力、実移動、着地と姿勢を更新する。</summary>
/// <param name="world">プレビュー専用 World。</param>
void CustomizeCharacterFlowController::UpdatePreview(World &world)
{
	if (world.GetGameObjects().empty())
	{
		return;
	}

	const auto id = world.GetGameObjects().front().id;
	const auto *state = world.GetComponent<StateComponent>(id);
	bool forcedAction = initialAction;
	initialAction = false;
	if (pendingAirAttack && state)
	{
		if (!state->isGrounded && ++airborneFrames >= AirAttackDelayFrames)
		{
			StateUpdateSystem::StartAction(world, id, {PlayerActionState::AirAttack, true, PreviewAttackSlot});
			pendingAirAttack = false;
			forcedAction = true;
		}
		else if (state->isGrounded && airborneFrames > 0)
		{
			pendingAirAttack = false;
		}
	}

	if (!forcedAction)
	{
		StateUpdateSystem::Update(world);
	}

	PlayerControlSystem::Update(world);
	MovementSystem::Update(world);
	EmbedResolveSystem::ResolveTemporaryGround(world);
	MotionSystem::Update(world);
	TransformSystem::UpdateWorldTransforms(world.GetGameObjects());
}

/// <summary>表示領域以上の解像度でプレビューの描画先を確保する。</summary>
void CustomizeCharacterFlowController::ResizePreviewTexture()
{
	if (previewRegion.IsEmpty())
	{
		return;
	}

	const int targetWidth = std::max(500, previewRegion.right - previewRegion.left);
	const int targetHeight = std::max(840, previewRegion.bottom - previewRegion.top);
	if (preview.width == targetWidth && preview.height == targetHeight)
	{
		return;
	}

	Renderer::RenderTexture replacement;
	if (FAILED(Renderer::CreateRenderTexture(replacement, targetWidth, targetHeight)))
	{
		DebugLog("[Customize] Character preview render texture unavailable.");

		return;
	}

	Renderer::ReleaseRenderTexture(preview);
	preview = replacement;
}

/// <summary>キャラを追うカメラでプレビューを描画する。</summary>
/// <param name="world">プレビュー用 World。</param>
void CustomizeCharacterFlowController::DrawPreview(World &world) const
{
	if (!preview.renderTargetView || previewRegion.IsEmpty() || world.GetGameObjects().empty())
	{
		return;
	}

	const auto id = world.GetGameObjects().front().id;
	const auto *transform = world.GetTransform(id);
	const auto *component = world.GetComponent<ModelComponent>(id);
	const auto *model = component ? ModelResourceManager::GetModel(component->resourceKey) : nullptr;
	if (!transform || !model)
	{
		return;
	}

	const float clear[] = {.94f, .945f, .95f, 1.0f};
	Renderer::BeginRenderTexture(preview, clear);
	const auto position = TransformSystem::GetLocalPosition(*transform);
	const auto scale = TransformSystem::GetLocalScale(*transform);
	const float framing = std::max(.5f, scale.y / .05f);
	const Vector3 cameraOffset(3.5f, 1.8f, -16.0f);
	Vector3 screenRight = Vector3::Up.Cross(-cameraOffset);
	screenRight.Normalize();
	const Vector3 target = position + (Vector3(0, 4.5f, 0) + screenRight * .90f) * framing;
	const Vector3 eye = target + cameraOffset * framing;
	const auto &bounds = previewRegion;
	const float aspect = static_cast<float>(bounds.right - bounds.left) / (bounds.bottom - bounds.top);
	const Matrix view = DirectX::XMMatrixLookAtLH(eye, target, Vector3::Up);
	const Matrix projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(43.0f),
		std::max(.1f, aspect), .1f, 200.0f);
	Renderer::SetViewProjection(view, projection);

	Matrix modelWorld = TransformSystem::GetWorldMatrix(*transform);
	const auto *motion = world.GetComponent<MotionPlayerComponent>(id);
	if (motion)
	{
		modelWorld *= Matrix::CreateTranslation(motion->visualRootOffset);
	}

	const auto *pose = world.GetComponent<SkeletonPoseComponent>(id);
	const auto *bones = pose && pose->initialized ? &pose->skinningMatrices : nullptr;
	const Renderer::ShadowCaster caster{model, modelWorld, bones};
	Renderer::DrawShadowMap(std::span(&caster, 1), position, 108.0f);
	Renderer::DrawTrainingRoomBackground(-55, 55, -35, 48, 48, 4, true);
	Renderer::DrawModel(*model, modelWorld, bones);
	Renderer::RestoreBackBuffer();
	Renderer::DrawTextureRegion(preview.shaderResourceView, bounds.ToNativeRect(),
		{0, 0, preview.width, preview.height}, Color(1, 1, 1, 1));
}
