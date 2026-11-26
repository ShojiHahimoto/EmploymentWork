#include "Controller/BattleSetupControllers.h"

#include "Data/CharacterDataLoader.h"
#include "Input/InputSystem.h"
#include "Scene/BattleScene.h"
#include "Manager/SceneManager.h"
#include "Manager/SaveWorker.h"
#include "Scene/TitleScene.h"
#include "Input/UIInputSystem.h"
#include "UI/UIRenderer.h"
#include "Core/Application.h"

#pragma execution_character_set("utf-8")

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>

namespace
{
	using Control = BattleSetupControl;
	using Keyboard = Input::KeyboardKey;
	using Pad = Input::GamepadButton;
	constexpr const char* TabImages[] = {"battle_tab_left", "battle_tab_right"};
	constexpr const char* PlayerLabels[] = {"1P", "2P"};
	const DirectX::SimpleMath::Color BattleText(0.91f, 0.94f, 0.96f, 1.0f);

	Input::NamedInputConfig FixedPreset(BattleSetup::InputDeviceKind kind, int slot)
	{
		Input::NamedInputConfig preset;
		preset.deviceKind = kind;
		if (slot == 0)
		{
			preset.configId = kind == BattleSetup::InputDeviceKind::Keyboard ? "default_keyboard" : "default_gamepad";
			preset.configName = "デフォルト";
		}
		else
		{
			preset.configId = "battle_preset_" + std::to_string(slot);
			preset.configName = "カスタム" + std::to_string(slot);
		}
		return preset;
	}

	bool Pressed(const BattleSetupControls& controls, Control control)
	{
		return controls[static_cast<size_t>(control)];
	}

	bool DeviceConnected(int device)
	{
		return device == 0 || Input::InputSystem::IsGamepadConnected(device - 1);
	}

	int DeviceIndex(const BattleSetup::InputDeviceAssignment& device)
	{
		return device.kind == BattleSetup::InputDeviceKind::Keyboard ? 0 : device.gamepadIndex + 1;
	}

	BattleSetupControls ReadControls(int device)
	{
		const auto state = Input::InputSystem::ReadDeviceUIActions(device - 1);
		const auto& move = state.actions[static_cast<size_t>(Input::InputActionId::Move)].value.axis;
		const auto& submit = state.actions[static_cast<size_t>(Input::InputActionId::Submit)].value;
		const auto& cancel = state.actions[static_cast<size_t>(Input::InputActionId::Cancel)].value;
		const float threshold = Input::InputSystem::GetSettings().buttonThreshold;
		return {move.x < -0.5f, move.x > 0.5f, move.y > 0.5f, move.y < -0.5f,
			submit.IsActive(threshold), cancel.IsActive(threshold)};
	}

	void AddPlayerTab(GameUI::Root& root, int side, const GameUI::Rect& card)
	{
		const int tabWidth = std::min(66, (card.right - card.left) / 2);
		const int x = side == 0 ? card.left : card.right - tabWidth;
		const GameUI::Rect tab{x, card.top - 9, x + tabWidth, card.top + 16};
		root.AddImage({}, tab, TabImages[side]);
		root.AddLabel({}, tab, PlayerLabels[side], true, 15, true, true);
	}

	GameUI::Rect SidePanel(int width, int height, int side)
	{
		const int panelWidth = width * 34 / 100;
		const int left = side == 0 ? 12 : width - panelWidth - 12;
		return {left, 86, left + panelWidth, height - 18};
	}

	std::string SideAction(int side, const char* action)
	{
		return "player/" + std::to_string(side) + "/" + action;
	}

	GameUI::Rect ConfigPanel(int width, int height, int side)
	{
		auto panel = SidePanel(width, height, side).Inset(12, 8);
		const int available = panel.bottom - panel.top;
		panel.top += std::max(0, std::min(available * 43 / 100, available - 250));
		return panel;
	}
}

/// <summary>全機器の入力を独立して反映し、決定済みの側は他の機器に取らせない。</summary>
/// <param name="controls">キーボード、続いて各パッドの押下エッジ。</param>
void BattleSetupDeviceAssignmentController::Update(const std::vector<BattleSetupControls>& controls)
{
	titleRequested = false;
	deviceSides.resize(controls.size(), 0);
	for (auto& device : lockedDevices)
	{
		if (device >= 0 && !DeviceConnected(device))
		{
			device = -1;
		}
	}

	// 未決定の機器からの取消は、同時に届いた別機器の決定より優先する。
	for (int device = 0; device < static_cast<int>(controls.size()); ++device)
	{
		if (DeviceConnected(device) && Pressed(controls[device], Control::Cancel) &&
			lockedDevices[0] != device && lockedDevices[1] != device)
		{
			titleRequested = true;
			return;
		}
	}

	for (int device = 0; device < static_cast<int>(controls.size()); ++device)
	{
		if (!DeviceConnected(device))
		{
			continue;
		}

		const auto& input = controls[device];
		const int lockedSide = lockedDevices[0] == device ? 0 : lockedDevices[1] == device ? 1 : -1;
		if (lockedSide >= 0)
		{
			if (Pressed(input, Control::Cancel))
			{
				lockedDevices[lockedSide] = -1;
			}
			else if (Pressed(input, Control::Confirm) && lockedDevices[1 - lockedSide] < 0)
			{
				// 機器は決定した側に残し、空いている側だけをCPUにする。
				session.cpuSide = 1 - lockedSide;
				setupData.players[session.cpuSide].cpuControlled = true;
				session.activeSide = lockedSide;
				return;
			}
			continue;
		}

		int& side = deviceSides[device];
		if (Pressed(input, Control::Left))
		{
			side = 0;
		}
		if (Pressed(input, Control::Right))
		{
			side = 1;
		}
		if (lockedDevices[side] >= 0)
		{
			side = 1 - side;
		}

		if (Pressed(input, Control::Confirm) && lockedDevices[side] < 0)
		{
			lockedDevices[side] = device;
			setupData.players[side].device = {device == 0 ? BattleSetup::InputDeviceKind::Keyboard :
				BattleSetup::InputDeviceKind::Gamepad, device - 1};
			if (IsComplete())
			{
				return;
			}
			for (int other = 0; other < static_cast<int>(deviceSides.size()); ++other)
			{
				if (other != device)
				{
					deviceSides[other] = 1 - side;
				}
			}
		}
	}
}

bool BattleSetupDeviceAssignmentController::IsComplete() const
{
	return session.cpuSide >= 0 || (lockedDevices[0] >= 0 && lockedDevices[1] >= 0);
}

/// <summary>未決定の機器からタイトルへ戻る要求があったか返す。</summary>
/// <returns>今フレームに戻る要求があればtrue。</returns>
bool BattleSetupDeviceAssignmentController::WantsTitle() const
{
	return titleRequested;
}

void BattleSetupDeviceAssignmentController::Reset()
{
	lockedDevices.fill(-1);
	deviceSides.clear();
	titleRequested = false;
}

/// <summary>機器の決定状態に応じて中央または左上からアイコンを配置する。</summary>
void BattleSetupDeviceAssignmentController::BuildUI(GameUI::Root& root) const
{
	const int panelWidth = session.width * 34 / 100;
	const int panelHeight = session.height * 40 / 100;
	const int gap = session.width * 3 / 100;
	const int top = (session.height - panelHeight) / 2 + 28;

	for (int side = 0; side < 2; ++side)
	{
		const int left = (session.width - 2 * panelWidth - gap) / 2 + side * (panelWidth + gap);
		const GameUI::Rect panel{left, top, left + panelWidth, top + panelHeight};
		root.AddPanel(panel, "battle_panel");
		root.AddNineSliceImage({}, panel, "battle_surface");
		AddPlayerTab(root, side, panel);

		int count = 0;
		for (int device = 0; device < static_cast<int>(deviceSides.size()); ++device)
		{
			const bool locked = lockedDevices[side] == device;
			if (!DeviceConnected(device) || (!locked &&
				(lockedDevices[side] >= 0 || deviceSides[device] != side || lockedDevices[1 - side] == device)))
			{
				continue;
			}

			const int size = std::max(1, std::min(panelWidth / (locked ? 3 : 4),
				(panelHeight - 44) / (locked ? 1 : 2)));
			const int columns = std::max(1, (panelWidth - 20) / (size + 6));
			const int x = locked ? left + (panelWidth - size) / 2 : left + 10 + (count % columns) * (size + 6);
			const int y = locked ? top + (panelHeight - size) / 2 : top + 22 + (count / columns) * (size + 6);
			const GameUI::Rect tile{x, y, x + size, y + size};
			root.AddPanel(tile, "button");
			root.AddImage({}, tile.Inset(size / 10, size / 5), device == 0 ? "battle_keyboard" : "battle_gamepad");
			++count;
		}
	}
}

void BattleSetupCharacterSelectionController::SelectCharacter(int playerIndex, int slotIndex)
{
	if (!HasSavedCharacterSlot(slotIndex))
	{
		return;
	}

	auto& player = setupData.players[playerIndex];
	const auto& summary = characterSlotSummaries[slotIndex];
	player.characterSlotIndex = slotIndex;
	player.characterId = summary.characterId;
	player.characterFolderPath = summary.characterFolderPath;
}

void BattleSetupCharacterSelectionController::SelectFirst()
{
	for (int slot = 0; slot < BattleSetup::CharacterSlotCount; ++slot)
	{
		if (HasSavedCharacterSlot(slot))
		{
			SelectCharacter(0, slot);
			SelectCharacter(1, slot);
			return;
		}
	}
}

void BattleSetupCharacterSelectionController::MoveSelection(int playerIndex, int horizontal, int vertical)
{
	std::vector<int> slots;
	for (int slot = 0; slot < BattleSetup::CharacterSlotCount; ++slot)
	{
		if (HasSavedCharacterSlot(slot))
		{
			slots.push_back(slot);
		}
	}
	if (slots.empty())
	{
		return;
	}

	const auto it = std::find(slots.begin(), slots.end(), setupData.players[playerIndex].characterSlotIndex);
	const int index = it == slots.end() ? 0 : static_cast<int>(it - slots.begin());
	const int last = static_cast<int>(slots.size()) - 1;
	const int next = vertical != 0 ? std::clamp(index + vertical * 2, 0, last)
		: std::clamp(index / 2 * 2 + std::clamp(index % 2 + horizontal, 0, 1), 0, last);
	SelectCharacter(playerIndex, slots[next]);
}

void BattleSetupCharacterSelectionController::BuildRoster(GameUI::Root& root, const GameUI::Rect& bounds, bool enabled) const
{
	const int count = static_cast<int>(std::count_if(characterSlotSummaries.begin(), characterSlotSummaries.end(),
		[](const auto& item) { return item.hasSavedData; }));
	if (count == 0)
	{
		root.AddLabel({}, bounds, "作成済みキャラクターなし", true, 18, true);
		return;
	}

	const int cellWidth = (bounds.right - bounds.left - 10) / 2;
	const int rowHeight = std::min(100, (bounds.bottom - bounds.top) / ((count + 1) / 2));
	const int rosterTop = bounds.top + (bounds.bottom - bounds.top - rowHeight * ((count + 1) / 2) + 14) / 2;
	int index = 0;
	for (int slot = 0; slot < BattleSetup::CharacterSlotCount; ++slot)
	{
		if (!HasSavedCharacterSlot(slot))
		{
			continue;
		}

		const int x = (count % 2 != 0 && index == count - 1)
			? (bounds.left + bounds.right - cellWidth) / 2
			: bounds.left + (index % 2) * (cellWidth + 10);
		const int y = rosterTop + index / 2 * rowHeight;
		const GameUI::Rect card{x, y, x + cellWidth, y + rowHeight - 14};
		root.AddButton("slot/" + std::to_string(slot), card, enabled, "battle_roster_button");
		root.AddNineSliceImage({}, card, "battle_surface", 0.7f);
		root.AddLabel({}, card.Inset(8, 6), characterSlotSummaries[slot].characterName,
			true, 19, true, true, BattleText);
		for (int side = 0; side < 2; ++side)
		{
			if (setupData.players[side].characterSlotIndex == slot)
			{
				AddPlayerTab(root, side, card);
			}
		}
		++index;
	}
}

void BattleSetupCharacterSelectionController::BuildSummary(GameUI::Root& root, int playerIndex, const GameUI::Rect& bounds)
{
	const int slot = setupData.players[playerIndex].characterSlotIndex;
	if (!HasSavedCharacterSlot(slot))
	{
		return;
	}

	const auto& summary = characterSlotSummaries[slot];
	root.AddLabel({}, {bounds.left, bounds.top, bounds.right, bounds.top + 42}, summary.characterName,
		true, 26, true, true, BattleText);
	constexpr const char* categories[] = {"地上技", "空中技", "必殺技"};
	constexpr const char* badges[] = {"badge_a", "badge_b", "badge_x", "badge_y"};
	const int categoryWidth = std::min(96, (bounds.right - bounds.left) / 3);
	const int rowHeight = 27;
	const int groupHeight = rowHeight * 4 + 6;
	const int listWidth = bounds.right - bounds.left - 10;
	root.BeginScrollArea(SideAction(playerIndex, "attacks"), {bounds.left, bounds.top + 44, bounds.right, bounds.bottom},
		groupHeight * 3, attackScroll[playerIndex]);
	for (int group = 0; group < 3; ++group)
	{
		const int top = group * groupHeight;
		root.AddRule({0, top, listWidth, top + 1});
		root.AddLabel({}, {0, top, categoryWidth, top + groupHeight}, categories[group], true, 15, true, true, BattleText);
		for (int key = 0; key < 4; ++key)
		{
			const int y = top + key * rowHeight + 2;
			root.AddImage({}, {categoryWidth + 4, y + 4, categoryWidth + 23, y + 23}, badges[key]);
			const auto& name = summary.attackNames[group][key];
			root.AddLabel({}, {categoryWidth + 29, y, listWidth, y + rowHeight},
				name.empty() ? "空" : name, true, 15, false, false, BattleText);
		}
	}
	root.EndScrollArea();
}


namespace
{
	constexpr Input::KeyboardKey ConfigurableKeyboardKeys[] = {
		Input::KeyboardKey::A,
		Input::KeyboardKey::B,
		Input::KeyboardKey::C,
		Input::KeyboardKey::D,
		Input::KeyboardKey::E,
		Input::KeyboardKey::F,
		Input::KeyboardKey::G,
		Input::KeyboardKey::H,
		Input::KeyboardKey::I,
		Input::KeyboardKey::J,
		Input::KeyboardKey::K,
		Input::KeyboardKey::L,
		Input::KeyboardKey::M,
		Input::KeyboardKey::N,
		Input::KeyboardKey::O,
		Input::KeyboardKey::P,
		Input::KeyboardKey::Q,
		Input::KeyboardKey::R,
		Input::KeyboardKey::S,
		Input::KeyboardKey::T,
		Input::KeyboardKey::U,
		Input::KeyboardKey::V,
		Input::KeyboardKey::W,
		Input::KeyboardKey::X,
		Input::KeyboardKey::Y,
		Input::KeyboardKey::Z,
		Input::KeyboardKey::Space,
		Input::KeyboardKey::Up,
		Input::KeyboardKey::Down,
		Input::KeyboardKey::Left,
		Input::KeyboardKey::Right
	};

	constexpr Input::GamepadButton ConfigurableGamepadButtons[] = {
		Input::GamepadButton::DPadUp,
		Input::GamepadButton::DPadDown,
		Input::GamepadButton::DPadLeft,
		Input::GamepadButton::DPadRight,
		Input::GamepadButton::A,
		Input::GamepadButton::B,
		Input::GamepadButton::X,
		Input::GamepadButton::Y,
		Input::GamepadButton::LeftShoulder,
		Input::GamepadButton::RightShoulder,
		Input::GamepadButton::LeftTrigger,
		Input::GamepadButton::RightTrigger
	};

	/// <summary>
	/// KeyboardKey を 表示用の短い文字列に変換する。
	/// </summary>
	/// <param name="key">表示する KeyboardKey。</param>
	/// <returns>キー名文字列。</returns>
	const char* ToKeyboardKeyText(Input::KeyboardKey key)
	{
		switch (key)
		{
		case Input::KeyboardKey::A: return "A";
		case Input::KeyboardKey::B: return "B";
		case Input::KeyboardKey::C: return "C";
		case Input::KeyboardKey::D: return "D";
		case Input::KeyboardKey::E: return "E";
		case Input::KeyboardKey::F: return "F";
		case Input::KeyboardKey::G: return "G";
		case Input::KeyboardKey::H: return "H";
		case Input::KeyboardKey::I: return "I";
		case Input::KeyboardKey::J: return "J";
		case Input::KeyboardKey::K: return "K";
		case Input::KeyboardKey::L: return "L";
		case Input::KeyboardKey::M: return "M";
		case Input::KeyboardKey::N: return "N";
		case Input::KeyboardKey::O: return "O";
		case Input::KeyboardKey::P: return "P";
		case Input::KeyboardKey::Q: return "Q";
		case Input::KeyboardKey::R: return "R";
		case Input::KeyboardKey::S: return "S";
		case Input::KeyboardKey::T: return "T";
		case Input::KeyboardKey::U: return "U";
		case Input::KeyboardKey::V: return "V";
		case Input::KeyboardKey::W: return "W";
		case Input::KeyboardKey::X: return "X";
		case Input::KeyboardKey::Y: return "Y";
		case Input::KeyboardKey::Z: return "Z";
		case Input::KeyboardKey::Space: return "Space";
		case Input::KeyboardKey::Up: return "Up";
		case Input::KeyboardKey::Down: return "Down";
		case Input::KeyboardKey::Left: return "Left";
		case Input::KeyboardKey::Right: return "Right";
		case Input::KeyboardKey::Enter: return "Enter";
		case Input::KeyboardKey::Escape: return "Escape";
		case Input::KeyboardKey::None:
		default:
			return "None";
		}
	}

	/// <summary>
	/// GamepadButton を 表示用の短い文字列に変換する。
	/// </summary>
	/// <param name="button">表示する GamepadButton。</param>
	/// <returns>ボタン名文字列。</returns>
	const char* ToGamepadButtonText(Input::GamepadButton button)
	{
		switch (button)
		{
		case Input::GamepadButton::DPadUp: return "DPad Up";
		case Input::GamepadButton::DPadDown: return "DPad Down";
		case Input::GamepadButton::DPadLeft: return "DPad Left";
		case Input::GamepadButton::DPadRight: return "DPad Right";
		case Input::GamepadButton::LeftShoulder: return "LB";
		case Input::GamepadButton::RightShoulder: return "RB";
		case Input::GamepadButton::LeftTrigger: return "LT";
		case Input::GamepadButton::RightTrigger: return "RT";
		case Input::GamepadButton::A: return "A";
		case Input::GamepadButton::B: return "B";
		case Input::GamepadButton::X: return "X";
		case Input::GamepadButton::Y: return "Y";
		case Input::GamepadButton::Start: return "Start";
		case Input::GamepadButton::Back: return "Back";
		case Input::GamepadButton::LeftThumb: return "Left Thumb";
		case Input::GamepadButton::RightThumb: return "Right Thumb";
		case Input::GamepadButton::None:
		default:
			return "None";
		}
	}


}

/// <summary>
/// 共有するセットアップ状態への参照を各コントローラーに結び付ける。
/// </summary>
/// <param name="sharedSession">各担当Controllerが共有するセットアップ状態。</param>
BattleSetupControllerContext::BattleSetupControllerContext(BattleSetupSession& sharedSession)
	: session(sharedSession)
{
}

/// <summary>
/// デバイス割り当てを編集する共有セットアップ状態を受け取る。
/// </summary>
/// <param name="sharedSession">デバイス設定を保持する共有状態。</param>
BattleSetupDeviceAssignmentController::BattleSetupDeviceAssignmentController(BattleSetupSession& sharedSession)
	: BattleSetupControllerContext(sharedSession)
	, setupData(sharedSession.setupData)
{
}

/// <summary>
/// キャラクター選択を編集する共有セットアップ状態を受け取る。
/// </summary>
/// <param name="sharedSession">キャラクタースロット選択を保持する共有状態。</param>
BattleSetupCharacterSelectionController::BattleSetupCharacterSelectionController(BattleSetupSession& sharedSession)
	: BattleSetupControllerContext(sharedSession)
	, setupData(sharedSession.setupData)
	, characterSlotSummaries(sharedSession.characterSlotSummaries)
{
}

/// <summary>
/// キーコン編集と入力キャプチャに使う共有セットアップ状態を受け取る。
/// </summary>
/// <param name="sharedSession">キーコン設定と編集状態を保持する共有状態。</param>
BattleSetupInputConfigController::BattleSetupInputConfigController(BattleSetupSession& sharedSession)
	: BattleSetupControllerContext(sharedSession)
	, setupData(sharedSession.setupData)
	, keyboardInputConfigs(sharedSession.keyboardInputConfigs)
	, gamepadInputConfigs(sharedSession.gamepadInputConfigs)
	, statusMessage(sharedSession.statusMessage)
	, capturingPlayerIndex(sharedSession.capturingPlayerIndex)
	, captureWaitFrames(sharedSession.captureWaitFrames)
	, creatingInputConfig(sharedSession.creatingInputConfig)
	, editingInputConfigPlayerIndex(sharedSession.editingInputConfigPlayerIndex)
	, editingInputConfigDeviceKind(sharedSession.editingInputConfigDeviceKind)
	, capturingDevice(sharedSession.capturingDevice)
	, capturingTarget(sharedSession.capturingTarget)
	, draftKeyboardConfig(sharedSession.draftKeyboardConfig)
	, draftGamepadConfig(sharedSession.draftGamepadConfig)
{
}

/// <summary>
/// 各セットアップ担当Controllerと画面配置の初期サイズを設定する。
/// </summary>
/// <param name="sharedSession">セットアップ全体で共有するデータ。</param>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
BattleSetupFlowController::BattleSetupFlowController(
	BattleSetupSession& sharedSession,
	int initialWidth,
	int initialHeight)
	: BattleSetupControllerContext(sharedSession)
	, setupData(sharedSession.setupData)
	, statusMessage(sharedSession.statusMessage)
	, width(sharedSession.width)
	, height(sharedSession.height)
	, deviceAssignmentController(sharedSession)
	, characterSelectionController(sharedSession)
	, inputConfigController(sharedSession)
{
	width = initialWidth;
	height = initialHeight;
}

/// <summary>
/// UI 入力マップへ切り替え、準備済み一覧を使う。
/// </summary>
/// <param name="writer">入力設定の保存先ワーカー。</param>
void BattleSetupFlowController::Enter(SaveWorker *writer)
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	inputConfigController.SetSaveWorker(writer);
	statusMessage.clear();
}

/// <summary>
/// セットアップ専用の終了時処理を行う。World の解放は Scene が担当する。
/// </summary>
void BattleSetupFlowController::Exit()
{
}

/// <summary>
/// キーコンフィグの入力待ちがあれば、次に押されたキーまたはボタンを取り込む。
/// </summary>
void BattleSetupFlowController::Update()
{
	inputConfigController.ApplyInputConfigSave();
	UpdateSetupUI();
}

/// <summary>
/// セットアップ画面の各担当コントローラーへ描画を委譲する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。</param>
void BattleSetupFlowController::Draw(Renderer& renderer)
{
	GameUI::DrawCommand background;
	background.bounds = {0, 0, width, height};
	background.color = DirectX::SimpleMath::Color(0.19f, 0.22f, 0.24f, 1.0f);
	root.Draw(renderer, &background, inputConfigController.Owner() >= 0);
}

/// <summary>
/// ウィンドウサイズ変更後の保持サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void BattleSetupFlowController::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// CharacterData 配下の固定スロットを確認し、選択 UI に表示する名前を更新する。
/// </summary>
void BattleSetupCharacterSelectionController::RefreshCharacterSlotSummaries()
{
	for (int slotIndex = 0; slotIndex < BattleSetup::CharacterSlotCount; ++slotIndex)
	{
		BattleSetupCharacterSlotSummary& summary = characterSlotSummaries[static_cast<size_t>(slotIndex)];
		summary = BattleSetupCharacterSlotSummary{};
		summary.characterId = BattleSetup::BuildCharacterSlotId(slotIndex);
		summary.characterFolderPath = BattleSetup::BuildCharacterFolderPath(slotIndex);

		const std::filesystem::path characterFolder(summary.characterFolderPath);
		const bool hasParameter = std::filesystem::exists(characterFolder / "Parameter.json");
		const bool hasAttackList = std::filesystem::exists(characterFolder / "AttackList.json");
		summary.hasSavedData = hasParameter && hasAttackList;
		if (!summary.hasSavedData)
		{
			summary.characterName = "Empty";
			continue;
		}

		CharacterData characterData;
		CharacterDataLoader::LoadCharacterData(summary.characterFolderPath, characterData);
		summary.characterName = characterData.parameter.characterName.empty()
			? summary.characterId
			: characterData.parameter.characterName;

		for (const auto& assigned : characterData.attacks)
		{
			const int button = static_cast<int>(assigned.button) - static_cast<int>(AttackButtonId::AttackA);
			const int group = assigned.slotType == AttackSlotType::Special ? 2 :
				assigned.slotUsableState == AttackUsableState::Air ? 1 : 0;
			if (button >= 0 && button < 4)
			{
				summary.attackNames[group][button] = assigned.attack.displayName;
			}
		}
	}
}

/// <summary>
/// 保存済みキーコンフィグ一覧を Keyboard / Gamepad ごとに読み直す。
/// </summary>
void BattleSetupInputConfigController::RefreshInputConfigSummaries()
{
	keyboardInputConfigs = Input::InputConfigLoader::LoadNamedInputConfigs(BattleSetup::InputDeviceKind::Keyboard);
	gamepadInputConfigs = Input::InputConfigLoader::LoadNamedInputConfigs(BattleSetup::InputDeviceKind::Gamepad);
}

/// <summary>
/// 選択対象スロットに読み込み可能なキャラクターデータがあるか確認する。
/// </summary>
/// <param name="slotIndex">確認するキャラクタースロット番号。</param>
/// <returns>保存済みデータが揃っていれば true。</returns>
bool BattleSetupCharacterSelectionController::HasSavedCharacterSlot(int slotIndex) const
{
	if (slotIndex < 0 || slotIndex >= BattleSetup::CharacterSlotCount)
	{
		return false;
	}

	return characterSlotSummaries[static_cast<size_t>(slotIndex)].hasSavedData;
}

/// <summary>
/// 現在選択中のカスタム枠を編集用コピーへ取り込む。
/// </summary>
/// <param name="playerIndex">作成対象の Player 番号。</param>
void BattleSetupInputConfigController::BeginPresetEdit(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= Input::MaxPlayers)
	{
		return;
	}

	const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
	creatingInputConfig = true;
	editingInputConfigPlayerIndex = playerIndex;
	editingInputConfigDeviceKind = player.device.kind;
	draftKeyboardConfig = player.keyboardConfig;
	draftGamepadConfig = player.gamepadConfig;
	capturingTarget = BattleSetupCaptureTarget::None;
	capturingPlayerIndex = -1;

	statusMessage.clear();
}

/// <summary>
/// 編集用コピーを破棄し、選択中のプリセットは維持する。
/// </summary>
void BattleSetupInputConfigController::CancelPresetEdit()
{
	creatingInputConfig = false;
	editingInputConfigPlayerIndex = -1;
	capturingTarget = BattleSetupCaptureTarget::None;
	capturingPlayerIndex = -1;
	statusMessage.clear();
}

/// <summary>
/// 編集中のキーコンフィグで、同じ機器種別の固定枠を上書きする。
/// </summary>
void BattleSetupInputConfigController::SaveDraftInputConfig()
{
	if (!saveWorker || IsSaving() || !creatingInputConfig || editingInputConfigPlayerIndex < 0)
	{
		return;
	}

	Input::NamedInputConfig config = FixedPreset(editingInputConfigDeviceKind, PresetIndex(editingInputConfigPlayerIndex));
	config.keyboardConfig = draftKeyboardConfig;
	config.gamepadConfig = draftGamepadConfig;

	savingPlayerIndex = editingInputConfigPlayerIndex;
	statusMessage = "保存中…";
	pendingSave = saveWorker->Enqueue([config = std::move(config)]() mutable -> std::optional<Input::NamedInputConfig>
	{
		config.filePath = Input::InputConfigLoader::BuildConfigFilePath(config.deviceKind, config.configId);
		if (!Input::InputConfigLoader::SaveNamedInputConfig(config))
		{
			return std::nullopt;
		}

		return config;
	});
}

/// <summary>キーコンフィグの保存に使うワーカーを借りる。</summary>
/// <param name="writer">ゲームが所有する保存ワーカー。</param>
void BattleSetupInputConfigController::SetSaveWorker(SaveWorker *writer)
{
	saveWorker = writer;
}

/// <summary>キーコンフィグの保存結果を受け取っていないか確認する。</summary>
/// <returns>未受け取りならtrue。</returns>
bool BattleSetupInputConfigController::IsSaving() const
{
	return pendingSave.valid();
}

/// <summary>保存済み設定を、ファイルを読み直さず一覧と対象プレイヤーに反映する。</summary>
void BattleSetupInputConfigController::ApplyInputConfigSave()
{
	if (!pendingSave.valid() || pendingSave.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}

	std::optional<Input::NamedInputConfig> config;
	try
	{
		config = pendingSave.get();
	}
	catch (const std::exception &)
	{
		statusMessage = "キーコンフィグを保存できませんでした。";

		return;
	}

	if (!config)
	{
		statusMessage = "キーコンフィグを保存できませんでした。";

		return;
	}

	auto &configs = config->deviceKind == BattleSetup::InputDeviceKind::Keyboard ? keyboardInputConfigs : gamepadInputConfigs;
	const auto existing = std::find_if(configs.begin(), configs.end(),
		[&config](const auto& entry) { return entry.configId == config->configId; });
	if (existing == configs.end())
	{
		configs.push_back(*config);
	}
	else
	{
		*existing = *config;
	}
	for (int side = 0; side < Input::MaxPlayers; ++side)
	{
		const auto& player = setupData.players[side];
		const auto& selectedId = config->deviceKind == BattleSetup::InputDeviceKind::Keyboard
			? player.keyboardConfigId : player.gamepadConfigId;
		if (side == savingPlayerIndex ||
			(!player.cpuControlled && player.device.kind == config->deviceKind && selectedId == config->configId))
		{
			ApplyNamedInputConfig(side, *config);
		}
	}
	owner = -1;
	creatingInputConfig = false;
	editingInputConfigPlayerIndex = -1;
	capturingTarget = BattleSetupCaptureTarget::None;
	capturingPlayerIndex = -1;
	statusMessage = "保存しました: " + config->configName;
}

/// <summary>
/// 保存済みキーコンフィグを指定プレイヤーの現在設定へ反映する。
/// </summary>
/// <param name="playerIndex">適用先 Player 番号。</param>
/// <param name="config">適用する保存済みキーコンフィグ。</param>
void BattleSetupInputConfigController::ApplyNamedInputConfig(int playerIndex, const Input::NamedInputConfig& config)
{
	if (playerIndex < 0 || playerIndex >= Input::MaxPlayers)
	{
		return;
	}

	BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
	if (config.deviceKind == BattleSetup::InputDeviceKind::Keyboard)
	{
		player.keyboardConfigId = config.configId;
		player.keyboardConfigName = config.configName;
		player.keyboardConfig = config.keyboardConfig;
	}
	else
	{
		player.gamepadConfigId = config.configId;
		player.gamepadConfigName = config.configName;
		player.gamepadConfig = config.gamepadConfig;
	}
}

/// <summary>
/// 指定項目のキーコンフィグ入力待ちを開始する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
/// <param name="device">入力を待つデバイス種別。</param>
/// <param name="target">変更対象の入力項目。</param>
void BattleSetupInputConfigController::BeginBindingCapture(int playerIndex, BattleSetupCaptureDevice device, BattleSetupCaptureTarget target)
{
	capturingPlayerIndex = playerIndex;
	capturingDevice = device;
	capturingTarget = target;
	captureWaitFrames = 1;
	statusMessage.clear();
}

/// <summary>
/// キーコンフィグ入力待ち中なら、押されたキーまたはボタンを対象項目へ反映する。
/// </summary>
void BattleSetupInputConfigController::UpdateBindingCapture()
{
	if (capturingTarget == BattleSetupCaptureTarget::None || capturingPlayerIndex < 0)
	{
		return;
	}

	const auto& player = setupData.players[capturingPlayerIndex];
	if (Input::InputSystem::IsKeyboardKeyDown(Keyboard::Escape) ||
		(capturingDevice == BattleSetupCaptureDevice::Gamepad &&
			Input::InputSystem::IsGamepadButtonDown(player.device.gamepadIndex, Pad::Back)))
	{
		capturingTarget = BattleSetupCaptureTarget::None;
		statusMessage.clear();
		return;
	}

	if (capturingDevice == BattleSetupCaptureDevice::Keyboard)
	{
		for (Input::KeyboardKey key : ConfigurableKeyboardKeys)
		{
			if (Input::InputSystem::IsKeyboardKeyDown(key))
			{
				if (captureWaitFrames == 0)
				{
					ApplyCapturedKeyboardKey(key);
				}
				return;
			}
		}
		captureWaitFrames = 0;
		return;
	}

	if (player.device.kind != BattleSetup::InputDeviceKind::Gamepad)
	{
		capturingTarget = BattleSetupCaptureTarget::None;
		statusMessage = "Selected player is not using a gamepad.";
		return;
	}

	for (Input::GamepadButton button : ConfigurableGamepadButtons)
	{
		if (Input::InputSystem::IsGamepadButtonDown(player.device.gamepadIndex, button))
		{
			if (captureWaitFrames == 0)
			{
				ApplyCapturedGamepadButton(button);
			}
			return;
		}
	}
	captureWaitFrames = 0;
}

namespace
{
	template<class Config, class Value>
	void ExchangeBinding(Config& config, BattleSetupCaptureTarget target, Value value)
	{
		constexpr Value Config::* members[] = {&Config::moveLeft, &Config::moveRight, &Config::moveDown,
			&Config::moveUp, &Config::attackA, &Config::attackB, &Config::attackX, &Config::attackY};
		const size_t index = static_cast<size_t>(target) - 1;
		if (index >= std::size(members))
		{
			return;
		}

		auto& current = config.*members[index];
		for (auto member : members)
		{
			auto& assigned = config.*member;
			if (assigned == value)
			{
				std::swap(current, assigned);
				return;
			}
		}
		current = value;
	}
}

/// <summary>キーを割り当て、使用済みなら元の項目と交換する。</summary>
/// <param name="key">取得したキー。</param>
void BattleSetupInputConfigController::ApplyCapturedKeyboardKey(Input::KeyboardKey key)
{
	ExchangeBinding(draftKeyboardConfig, capturingTarget, key);
	capturingTarget = BattleSetupCaptureTarget::None;
}

/// <summary>パッドのボタンを割り当て、使用済みなら元の項目と交換する。</summary>
/// <param name="button">取得したボタン。</param>
void BattleSetupInputConfigController::ApplyCapturedGamepadButton(Input::GamepadButton button)
{
	ExchangeBinding(draftGamepadConfig, capturingTarget, button);
	capturingTarget = BattleSetupCaptureTarget::None;
}

/// <summary>
/// 現在のバトル開始設定が実行可能か検証する。
/// </summary>
/// <param name="outMessage">検証結果の表示メッセージ。</param>
/// <returns>バトル開始可能なら true。</returns>
bool BattleSetupFlowController::ValidateSetup(std::string& outMessage) const
{
	if (!BattleSetup::AreDevicesUnique(setupData))
	{
		outMessage = "1Pと2Pには別々の機器を割り当ててください。";
		return false;
	}

	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
		if (!characterSelectionController.HasSavedCharacterSlot(player.characterSlotIndex))
		{
			outMessage = std::to_string(playerIndex + 1) + "Pのキャラクターを選択してください。";
			return false;
		}
	}

	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
		if (!player.cpuControlled && player.device.kind == BattleSetup::InputDeviceKind::Gamepad
			&& !Input::InputSystem::IsGamepadConnected(player.device.gamepadIndex))
		{
			outMessage = std::to_string(playerIndex + 1) + "Pのゲームパッドが接続されていません。";
			return false;
		}
	}

	outMessage.clear();
	return true;
}

/// <summary>
/// 現在の設定を BattleScene へ渡して、バトル開始を予約する。
/// </summary>
void BattleSetupFlowController::StartBattle()
{
	if (inputConfigController.IsSaving())
	{
		return;
	}

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<BattleScene>(width, height, setupData));
}


int BattleSetupInputConfigController::Owner() const
{
	return owner;
}

int BattleSetupInputConfigController::PresetIndex(int playerIndex) const
{
	const auto& player = setupData.players[playerIndex];
	const auto& id = player.device.kind == BattleSetup::InputDeviceKind::Keyboard
		? player.keyboardConfigId : player.gamepadConfigId;
	for (int slot = 1; slot <= 3; ++slot)
	{
		if (id == FixedPreset(player.device.kind, slot).configId)
		{
			return slot;
		}
	}
	return 0;
}

std::string BattleSetupInputConfigController::PresetLabel(int playerIndex) const
{
	return FixedPreset(setupData.players[playerIndex].device.kind, PresetIndex(playerIndex)).configName;
}

/// <summary>選択した機器種別の固定プリセットを循環し、その設定を適用する。</summary>
void BattleSetupInputConfigController::CyclePreset(int playerIndex, int direction)
{
	const int slot = (PresetIndex(playerIndex) + direction + 4) % 4;
	auto config = FixedPreset(setupData.players[playerIndex].device.kind, slot);
	const auto& configs = config.deviceKind == BattleSetup::InputDeviceKind::Keyboard
		? keyboardInputConfigs : gamepadInputConfigs;
	const auto saved = std::find_if(configs.begin(), configs.end(),
		[&config](const auto& entry) { return entry.configId == config.configId; });
	if (slot != 0 && saved != configs.end())
	{
		config.keyboardConfig = saved->keyboardConfig;
		config.gamepadConfig = saved->gamepadConfig;
	}
	ApplyNamedInputConfig(playerIndex, config);
}

void BattleSetupInputConfigController::Open(int playerIndex)
{
	if (!setupData.players[playerIndex].cpuControlled && PresetIndex(playerIndex) != 0)
	{
		owner = playerIndex;
		editorFocus = 0;
		BeginPresetEdit(playerIndex);
	}
}

void BattleSetupInputConfigController::Close()
{
	if (!IsSaving())
	{
		CancelPresetEdit();
		owner = -1;
	}
}

void BattleSetupInputConfigController::BuildUI(GameUI::Root& root, const GameUI::Rect& bounds)
{
	root.AddPanel(bounds, "battle_panel");
	BuildConfigEditor(root, bounds.Inset(12, 8));
}

namespace
{
	using Target = BattleSetupCaptureTarget;
	constexpr Target BindingTargets[] = {Target::MoveUp, Target::MoveDown, Target::MoveLeft, Target::MoveRight,
		Target::AttackA, Target::AttackB, Target::AttackX, Target::AttackY};
	constexpr const char* BindingLabels[] = {"移動 上", "移動 下", "移動 左", "移動 右",
		"攻撃 A", "攻撃 B", "攻撃 X", "攻撃 Y"};
	constexpr std::array<std::string_view, 10> EditorButtonIds = {
		"config/bind/0", "config/bind/1", "config/bind/2", "config/bind/3", "config/bind/4",
		"config/bind/5", "config/bind/6", "config/bind/7", "config/save", "config/cancel"};
	constexpr int LastBindingRow = static_cast<int>(std::size(BindingTargets)) - 1;
	constexpr int SaveButton = LastBindingRow + 1;
	constexpr int CancelButton = SaveButton + 1;
}

/// <summary>縦の割り当て欄と横並びの保存・キャンセルを画面配置どおりに移動する。</summary>
/// <param name="horizontal">左右の移動方向。</param>
/// <param name="vertical">上下の移動方向。下が正。</param>
void BattleSetupInputConfigController::MoveEditorFocus(int horizontal, int vertical)
{
	if (editorFocus <= LastBindingRow)
	{
		editorFocus = std::clamp(editorFocus + vertical, 0, SaveButton);
	}
	else if (vertical < 0)
	{
		editorFocus = LastBindingRow;
	}
	else if (vertical == 0)
	{
		editorFocus = std::clamp(editorFocus + horizontal, SaveButton, CancelButton);
	}
}

void BattleSetupInputConfigController::BuildConfigEditor(GameUI::Root& root, const GameUI::Rect& bounds)
{
	const int center = (bounds.left + bounds.right) / 2;
	const int rowHeight = std::clamp((bounds.bottom - bounds.top) / 11, 1, 38);
	const bool enabled = !IsSaving() && capturingTarget == Target::None;
	const bool keyboard = editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard;
	root.AddLabel({}, {bounds.left, bounds.top, bounds.right, bounds.top + rowHeight},
		PresetLabel(owner), true, 17, true, true, BattleText);

	const auto& k = draftKeyboardConfig;
	const auto& p = draftGamepadConfig;
	const Keyboard keys[] = {k.moveUp, k.moveDown, k.moveLeft, k.moveRight, k.attackA, k.attackB, k.attackX, k.attackY};
	const Pad buttons[] = {p.moveUp, p.moveDown, p.moveLeft, p.moveRight, p.attackA, p.attackB, p.attackX, p.attackY};
	for (int i = 0; i < 8; ++i)
	{
		const int y = bounds.top + (i + 1) * rowHeight;
		root.AddLabel({}, {bounds.left, y, center, y + rowHeight}, BindingLabels[i], true, 15, false, false, BattleText);
		const char* keyText = keyboard ? ToKeyboardKeyText(keys[i]) : ToGamepadButtonText(buttons[i]);
		root.AddButton(EditorButtonIds[i], {center, y, bounds.right, y + rowHeight - 2},
			capturingTarget == BindingTargets[i] ? "入力待ち…" : keyText, enabled, 15);
	}

	const int y = bounds.top + 9 * rowHeight;
	root.AddPrimaryButton("config/save", {bounds.left, y, center - 3, y + rowHeight}, "保存", enabled);
	root.AddButton("config/cancel", {center + 3, y, bounds.right, y + rowHeight}, "キャンセル", !IsSaving(), 15);
	root.AddLabel({}, {bounds.left, y + rowHeight, bounds.right, y + 2 * rowHeight}, statusMessage, true, 13, false, false, BattleText);
	if (enabled)
	{
		root.FocusButton(EditorButtonIds[editorFocus]);
	}
}

/// <summary>割り当て設定のボタン操作を処理し、ポインターで選んだ位置も保持する。</summary>
void BattleSetupInputConfigController::HandleUI(const GameUI::Root& root)
{
	const auto focused = std::find(EditorButtonIds.begin(), EditorButtonIds.end(), root.GetFocusedId());
	if (focused != EditorButtonIds.end())
	{
		editorFocus = static_cast<int>(focused - EditorButtonIds.begin());
	}

	if (root.WasActivated("config/cancel"))
	{
		Close();
		return;
	}

	if (root.WasActivated("config/save"))
	{
		SaveDraftInputConfig();
	}
	for (int i = 0; i < 8; ++i)
	{
		if (root.WasActivated(EditorButtonIds[i]))
		{
			BeginBindingCapture(owner, editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard
				? BattleSetupCaptureDevice::Keyboard : BattleSetupCaptureDevice::Gamepad, BindingTargets[i]);
			break;
		}
	}
}

/// <summary>キーコン操作とは分離して、機器ごとの押下エッジを処理する。</summary>
void BattleSetupFlowController::UpdateSetupUI()
{
	const bool capturing = session.capturingTarget != Target::None;
	const int deviceCount = Input::InputSystem::GetMaxGamepadCount() + 1;
	previousControls.resize(deviceCount);
	std::vector<BattleSetupControls> edges(deviceCount);
	auto frame = GameUI::UIInputSystem::GetFrame();
	const bool foreground = GetForegroundWindow() == Application::GetWindow() && !frame.focusLost;
	for (int device = 0; device < deviceCount; ++device)
	{
		const auto current = ReadControls(device);
		for (size_t key = 0; key < current.size(); ++key)
		{
			edges[device][key] = foreground && current[key] && !previousControls[device][key];
		}
		previousControls[device] = current;
	}
	if (foreground && frame.rightPointerPressed)
	{
		edges[0][static_cast<size_t>(Control::Cancel)] = true;
	}

	frame.focusMove = 0;
	frame.activatePressed = false;
	frame.cancelPressed = false;
	const int configOwner = inputConfigController.Owner();
	frame.keysPressed[static_cast<size_t>(GameUI::Key::Enter)] = false;
	frame.keysPressed[static_cast<size_t>(GameUI::Key::Escape)] = false;
	if (configOwner >= 0)
	{
		const auto& access = edges[DeviceIndex(setupData.players[configOwner].device)];
		if (capturing)
		{
			if (foreground && frame.rightPointerPressed && configOwner >= 0 &&
				setupData.players[configOwner].device.kind == BattleSetup::InputDeviceKind::Keyboard)
			{
				session.capturingTarget = Target::None;
			}
			else if (foreground)
			{
				inputConfigController.UpdateBindingCapture();
			}
			frame.pointerPressed = false;
			frame.pointerReleased = false;
		}
		else if (!inputConfigController.IsSaving())
		{
			if (Pressed(access, Control::Cancel))
			{
				inputConfigController.Close();
			}
			else if (!frame.composing)
			{
				const int horizontal = static_cast<int>(Pressed(access, Control::Right)) -
					static_cast<int>(Pressed(access, Control::Left));
				const int vertical = static_cast<int>(Pressed(access, Control::Down)) -
					static_cast<int>(Pressed(access, Control::Up));
				inputConfigController.MoveEditorFocus(horizontal, vertical);
				frame.activatePressed = Pressed(access, Control::Confirm);
			}
		}
	}
	else if (selectingCharacters)
	{
		UpdateCharacters(edges);
	}
	else
	{
		deviceAssignmentController.Update(edges);
		if (deviceAssignmentController.WantsTitle())
		{
			RequestTitleScene();
			return;
		}
		if (deviceAssignmentController.IsComplete())
		{
			selectingCharacters = true;
			characterSelectionController.SelectFirst();
		}
	}

	BuildUI(frame);
	if (!capturing && configOwner >= 0 && inputConfigController.Owner() == configOwner)
	{
		inputConfigController.HandleUI(root);
	}
}

void BattleSetupFlowController::UpdateCharacters(const std::vector<BattleSetupControls>& controls)
{
	if (session.cpuSide >= 0)
	{
		const int humanSide = 1 - session.cpuSide;
		const int side = session.ready[humanSide] ? session.cpuSide : humanSide;
		HandleCharacterInput(side, controls[DeviceIndex(setupData.players[humanSide].device)]);
		return;
	}

	for (int side = 0; side < 2; ++side)
	{
		HandleCharacterInput(side, controls[DeviceIndex(setupData.players[side].device)]);
		if (inputConfigController.Owner() >= 0 || !selectingCharacters)
		{
			break;
		}
	}
}

/// <summary>キャラクターの選択、決定、準備完了を進め、準備完了後は取消だけを受ける。</summary>
void BattleSetupFlowController::HandleCharacterInput(int side, const BattleSetupControls& controls)
{
	const bool cancel = Pressed(controls, Control::Cancel);
	const bool confirm = Pressed(controls, Control::Confirm);
	const int horizontal = static_cast<int>(Pressed(controls, Control::Right)) - static_cast<int>(Pressed(controls, Control::Left));
	const int vertical = static_cast<int>(Pressed(controls, Control::Down)) - static_cast<int>(Pressed(controls, Control::Up));
	if (!cancel && !confirm && !horizontal && !vertical)
	{
		return;
	}

	session.activeSide = side;
	if (session.ready[side])
	{
		if (cancel)
		{
			session.ready[side] = false;
		}
		return;
	}

	if (cancel)
	{
		if (session.characterConfirmed[side])
		{
			session.characterConfirmed[side] = false;
		}
		else if (side == session.cpuSide)
		{
			const int humanSide = 1 - session.cpuSide;
			session.ready[humanSide] = false;
			session.activeSide = humanSide;
		}
		else if (session.cpuSide >= 0 ||
			(side == 0 && !session.characterConfirmed[1] && !session.ready[1]))
		{
			GoBack();
		}
		return;
	}

	if (!session.characterConfirmed[side])
	{
		if (horizontal || vertical)
		{
			characterSelectionController.MoveSelection(side, horizontal, vertical);
		}
		if (confirm && characterSelectionController.HasSavedCharacterSlot(setupData.players[side].characterSlotIndex))
		{
			session.characterConfirmed[side] = true;
			readyFocused[side] = true;
		}
		return;
	}

	if (side != session.cpuSide)
	{
		if (vertical != 0)
		{
			readyFocused[side] = vertical > 0;
		}
		if (horizontal != 0 && !readyFocused[side])
		{
			inputConfigController.CyclePreset(side, horizontal);
		}
	}
	if (confirm)
	{
		if (readyFocused[side])
		{
			MarkReady(side);
		}
		else if (inputConfigController.Owner() < 0)
		{
			inputConfigController.Open(side);
		}
	}
}

void BattleSetupFlowController::MarkReady(int playerIndex)
{
	session.ready[playerIndex] = true;
	if (session.cpuSide >= 0 && playerIndex != session.cpuSide)
	{
		session.activeSide = session.cpuSide;
	}
	if (session.ready[0] && session.ready[1])
	{
		if (ValidateSetup(statusMessage))
		{
			StartBattle();
		}
		else
		{
			session.ready[playerIndex] = false;
		}
	}
}

/// <summary>現在の画面を共通UIへ登録し、マウス操作を担当処理へ渡す。</summary>
void BattleSetupFlowController::BuildUI(GameUI::InputFrame frame)
{
	const int owner = inputConfigController.Owner();
	root.BeginFrame({0, 0, width, height}, frame);
	root.AddRule({0, 0, width, 68}, DirectX::SimpleMath::Color(0.13f, 0.17f, 0.20f, 1.0f));
	root.AddRule({0, 66, width, 68}, DirectX::SimpleMath::Color(0.34f, 0.41f, 0.45f, 1.0f));
	root.AddImage({}, {82, 0, 119, 67}, "header_accent");
	const GameUI::Rect backBounds{13, 15, 66, 52};
	root.AddPanel(backBounds, "button");
	root.AddImageButton("back", backBounds.Inset(7, 5), "back", "戻る", owner < 0);
	root.AddLabel({}, {132, 0, width - 12, 68}, "バトルセットアップ", true, 30, false, true, BattleText);

	if (!selectingCharacters)
	{
		const int titleY = std::max(74, height * 30 / 100 - 60);
		root.AddLabel({}, {0, titleY, width, titleY + 58}, "プレイヤー選択", true, 44, true, true, BattleText);
		deviceAssignmentController.BuildUI(root);
	}
	else
	{
		characterSelectionController.BuildRoster(root, {width * 35 / 100 + 12, 104, width * 65 / 100 - 12, height - 18},
			owner < 0 && !session.characterConfirmed[session.activeSide]);
		for (int side = 0; side < 2; ++side)
		{
			const auto panel = SidePanel(width, height, side);
			root.AddPanel(panel, "battle_panel");
			root.AddNineSliceImage({}, panel, "battle_surface");
			AddPlayerTab(root, side, panel);
			auto summary = panel.Inset(12, 16);
			const int y = std::min(panel.top + 402, panel.bottom - 96);
			if (owner == side)
			{
				summary.bottom = ConfigPanel(width, height, side).top - 4;
			}
			else if (session.characterConfirmed[side])
			{
				summary.bottom = y - 6;
			}
			characterSelectionController.BuildSummary(root, side, summary);
			if (!session.characterConfirmed[side])
			{
				continue;
			}

			if (owner == side)
			{
				continue;
			}
			const bool enabled = owner < 0 && !session.ready[side] &&
				(session.cpuSide < 0 || side == session.activeSide);
			if (side != session.cpuSide)
			{
				const auto label = "キーコンフィグ  " + inputConfigController.PresetLabel(side);
				const GameUI::Rect row{panel.left + 49, y, panel.right - 49, y + 38};
				root.AddButton(SideAction(side, "previous"), {panel.left + 12, y, row.left - 3, y + 38}, "‹", enabled);
				root.AddButton(SideAction(side, "next"), {row.right + 3, y, panel.right - 12, y + 38}, "›", enabled);
				if (!readyFocused[side] && enabled)
				{
					root.AddPrimaryButton(SideAction(side, "config"), row, label);
				}
				else
				{
					root.AddButton(SideAction(side, "config"), row, label, enabled);
				}
			}
			const int readyY = side == session.cpuSide ? y : y + 46;
			const GameUI::Rect row{panel.left + 12, readyY, panel.right - 12, readyY + 38};
			if ((readyFocused[side] && enabled) || session.ready[side])
			{
				root.AddPrimaryButton(SideAction(side, "ready"), row, "準備完了", enabled);
			}
			else
			{
				root.AddButton(SideAction(side, "ready"), row, "準備完了", enabled);
			}
		}

		if (owner >= 0)
		{
			inputConfigController.BuildUI(root, ConfigPanel(width, height, owner));
		}
		else if (!statusMessage.empty())
		{
			const GameUI::Rect message{width * 23 / 100, height - 48, width * 77 / 100, height - 14};
			root.AddPanel(message);
			root.AddLabel({}, message, statusMessage, true, 15, true);
		}
	}

	root.EndFrame();
	if (owner >= 0)
	{
		return;
	}
	if (root.WasActivated("back"))
	{
		GoBack();
		return;
	}
	if (!selectingCharacters)
	{
		return;
	}

	for (int side = 0; side < 2; ++side)
	{
		const bool previous = root.WasActivated(SideAction(side, "previous"));
		const bool next = root.WasActivated(SideAction(side, "next"));
		if (previous || next)
		{
			readyFocused[side] = false;
			inputConfigController.CyclePreset(side, next ? 1 : -1);
			return;
		}
		if (root.WasActivated(SideAction(side, "config")))
		{
			readyFocused[side] = false;
			inputConfigController.Open(side);
			return;
		}
		if (root.WasActivated(SideAction(side, "ready")))
		{
			MarkReady(side);
			return;
		}
	}
	for (int slot = 0; slot < BattleSetup::CharacterSlotCount; ++slot)
	{
		if (root.WasActivated("slot/" + std::to_string(slot)))
		{
			const int side = session.activeSide;
			characterSelectionController.SelectCharacter(side, slot);
			session.characterConfirmed[side] = true;
			readyFocused[side] = true;
			return;
		}
	}
}

/// <summary>キャラ選択から入力機器選択へ戻し、機器選択からはタイトルへ戻す。</summary>
void BattleSetupFlowController::GoBack()
{
	if (!selectingCharacters)
	{
		RequestTitleScene();
		return;
	}

	selectingCharacters = false;
	session.cpuSide = -1;
	session.activeSide = 0;
	session.characterConfirmed.fill(false);
	session.ready.fill(false);
	setupData = BattleSetup::CreateDefaultBattleSetupData();
	deviceAssignmentController.Reset();
	statusMessage.clear();
}

/// <summary>
/// TitleScene への切り替えを予約する。
/// </summary>
void BattleSetupFlowController::RequestTitleScene()
{
	if (inputConfigController.IsSaving())
	{
		return;
	}

	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(width, height));
}
