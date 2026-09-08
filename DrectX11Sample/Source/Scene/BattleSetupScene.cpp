#include "Scene/BattleSetupScene.h"

#include "Data/CharacterDataLoader.h"
#include "Input/InputSystem.h"
#include "Scene/BattleScene.h"
#include "Scene/SceneManager.h"
#include "Scene/TitleScene.h"
#include "System/imgui-docking/imgui.h"

#include <cstdio>
#include <filesystem>
#include <memory>

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
	/// KeyboardKey を ImGui 表示用の短い文字列に変換する。
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
	/// GamepadButton を ImGui 表示用の短い文字列に変換する。
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

	/// <summary>
	/// 入力デバイス割り当てを ImGui 表示用文字列に変換する。
	/// </summary>
	/// <param name="device">表示する入力デバイス割り当て。</param>
	/// <returns>デバイス名文字列。</returns>
	std::string ToInputDeviceText(const BattleSetup::InputDeviceAssignment& device)
	{
		if (device.kind == BattleSetup::InputDeviceKind::Keyboard)
		{
			return "Keyboard";
		}

		return "XInput Pad " + std::to_string(device.gamepadIndex);
	}
}

/// <summary>
/// BattleSetupScene を現在の描画サイズで初期化する。
/// </summary>
/// <param name="initialWidth">初期ウィンドウ幅。</param>
/// <param name="initialHeight">初期ウィンドウ高さ。</param>
BattleSetupScene::BattleSetupScene(int initialWidth, int initialHeight)
	: setupData(BattleSetup::CreateDefaultBattleSetupData())
	, width(initialWidth)
	, height(initialHeight)
{
}

/// <summary>
/// UI 入力マップへ切り替え、キャラクタースロット一覧を読み込む。
/// </summary>
void BattleSetupScene::Enter()
{
	Input::InputSystem::SetActionMap(Input::InputActionMapId::UI);
	RefreshCharacterSlotSummaries();
	RefreshInputConfigSummaries();
	statusMessage = "Battle setup ready.";
}

/// <summary>
/// BattleSetupScene が保持する一時 World を破棄する。
/// </summary>
void BattleSetupScene::Exit()
{
	world.Clear();
}

/// <summary>
/// キーコンフィグの入力待ちがあれば、次に押されたキーまたはボタンを取り込む。
/// </summary>
void BattleSetupScene::RunSystems()
{
	UpdateBindingCapture();
}

/// <summary>
/// BattleSetupScene の ImGui 仮 UI を描画する。
/// </summary>
/// <param name="renderer">描画に使用する Renderer。現段階では ImGui のみなので未使用。</param>
void BattleSetupScene::Draw(Renderer& renderer)
{
	(void)renderer;
	DrawSetupWindow();
}

/// <summary>
/// ウィンドウサイズ変更後の保持サイズを更新する。
/// </summary>
/// <param name="newWidth">新しい幅。</param>
/// <param name="newHeight">新しい高さ。</param>
void BattleSetupScene::OnResize(int newWidth, int newHeight)
{
	if (newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	width = newWidth;
	height = newHeight;
}

/// <summary>
/// BattleSetupScene が保持する World を取得する。
/// </summary>
/// <returns>変更可能な World。</returns>
World& BattleSetupScene::GetWorld()
{
	return world;
}

/// <summary>
/// BattleSetupScene が保持する World を読み取り専用で取得する。
/// </summary>
/// <returns>読み取り専用の World。</returns>
const World& BattleSetupScene::GetWorld() const
{
	return world;
}

/// <summary>
/// CharacterData 配下の固定スロットを確認し、選択 UI に表示する名前を更新する。
/// </summary>
void BattleSetupScene::RefreshCharacterSlotSummaries()
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
	}
}

/// <summary>
/// 保存済みキーコンフィグ一覧を Keyboard / Gamepad ごとに読み直す。
/// </summary>
void BattleSetupScene::RefreshInputConfigSummaries()
{
	keyboardInputConfigs = Input::InputConfigLoader::LoadNamedInputConfigs(BattleSetup::InputDeviceKind::Keyboard);
	gamepadInputConfigs = Input::InputConfigLoader::LoadNamedInputConfigs(BattleSetup::InputDeviceKind::Gamepad);
}

/// <summary>
/// バトル開始前の設定を編集する ImGui ウィンドウを描画する。
/// </summary>
void BattleSetupScene::DrawSetupWindow()
{
	ImGui::SetNextWindowSize(ImVec2(620.0f, 720.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Battle Setup"))
	{
		ImGui::End();
		return;
	}

	DrawPlayerSetup(0);
	ImGui::Separator();
	DrawPlayerSetup(1);
	ImGui::Separator();

	std::string validationMessage;
	const bool canStartBattle = ValidateSetup(validationMessage);
	if (!validationMessage.empty())
	{
		ImGui::TextColored(canStartBattle ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", validationMessage.c_str());
	}
	if (!statusMessage.empty())
	{
		ImGui::TextUnformatted(statusMessage.c_str());
	}

	ImGui::BeginDisabled(!canStartBattle);
	if (ImGui::Button("Start Battle", ImVec2(150.0f, 0.0f)))
	{
		StartBattle();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Back To Title", ImVec2(150.0f, 0.0f)))
	{
		RequestTitleScene();
	}

	ImGui::End();
}

/// <summary>
/// 指定プレイヤーのデバイス、キャラクター、キーコンフィグ UI を描画する。
/// </summary>
/// <param name="playerIndex">描画する Player 番号。</param>
void BattleSetupScene::DrawPlayerSetup(int playerIndex)
{
	ImGui::PushID(playerIndex);
	ImGui::Text("%dP Setup", playerIndex + 1);
	DrawDeviceSelector(playerIndex);
	DrawCharacterSelector(playerIndex);
	DrawKeyConfig(playerIndex);
	ImGui::PopID();
}

/// <summary>
/// 指定プレイヤーの使用デバイス選択 Combo を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawDeviceSelector(int playerIndex)
{
	BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
	const std::string currentDeviceText = ToInputDeviceText(player.device);

	ImGui::BeginDisabled(IsCreatingInputConfigFor(playerIndex));
	if (ImGui::BeginCombo("Input Device", currentDeviceText.c_str()))
	{
		const bool keyboardSelected = player.device.kind == BattleSetup::InputDeviceKind::Keyboard;
		if (ImGui::Selectable("Keyboard", keyboardSelected))
		{
			player.device.kind = BattleSetup::InputDeviceKind::Keyboard;
			player.device.gamepadIndex = -1;
		}

		for (int gamepadIndex = 0; gamepadIndex < Input::InputSystem::GetMaxGamepadCount(); ++gamepadIndex)
		{
			const bool connected = Input::InputSystem::IsGamepadConnected(gamepadIndex);
			const bool selected =
				player.device.kind == BattleSetup::InputDeviceKind::Gamepad
				&& player.device.gamepadIndex == gamepadIndex;
			const std::string label = connected
				? "XInput Pad " + std::to_string(gamepadIndex)
				: "XInput Pad " + std::to_string(gamepadIndex) + " (Disconnected)";

			ImGui::BeginDisabled(!connected);
			if (ImGui::Selectable(label.c_str(), selected))
			{
				player.device.kind = BattleSetup::InputDeviceKind::Gamepad;
				player.device.gamepadIndex = gamepadIndex;
			}
			ImGui::EndDisabled();
		}

		ImGui::EndCombo();
	}
	ImGui::EndDisabled();
}

/// <summary>
/// 指定プレイヤーのキャラクタースロット選択 Combo を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawCharacterSelector(int playerIndex)
{
	BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
	const BattleSetupCharacterSlotSummary& currentSummary =
		characterSlotSummaries[static_cast<size_t>(player.characterSlotIndex)];
	const std::string currentText = currentSummary.characterId + ": " + currentSummary.characterName;

	if (ImGui::BeginCombo("Character Slot", currentText.c_str()))
	{
		for (int slotIndex = 0; slotIndex < BattleSetup::CharacterSlotCount; ++slotIndex)
		{
			const BattleSetupCharacterSlotSummary& summary = characterSlotSummaries[static_cast<size_t>(slotIndex)];
			const bool selected = player.characterSlotIndex == slotIndex;
			const std::string label = summary.characterId + ": " + summary.characterName;

			ImGui::BeginDisabled(!summary.hasSavedData);
			if (ImGui::Selectable(label.c_str(), selected))
			{
				player.characterSlotIndex = slotIndex;
				player.characterId = summary.characterId;
				player.characterFolderPath = summary.characterFolderPath;
			}
			ImGui::EndDisabled();
		}

		ImGui::EndCombo();
	}
}

/// <summary>
/// 選択中デバイスに合わせて、指定プレイヤーのキーコンフィグ UI を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawKeyConfig(int playerIndex)
{
	if (IsCreatingInputConfigFor(playerIndex))
	{
		DrawNewInputConfigEditor(playerIndex);
	}
	else
	{
		DrawInputConfigSelector(playerIndex);
	}
}

/// <summary>
/// 選択中デバイスに合う保存済みキーコンフィグ一覧を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawInputConfigSelector(int playerIndex)
{
	BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
	const bool usesKeyboard = player.device.kind == BattleSetup::InputDeviceKind::Keyboard;
	const std::vector<Input::NamedInputConfig>& configs = usesKeyboard ? keyboardInputConfigs : gamepadInputConfigs;
	const std::string& currentConfigId = usesKeyboard ? player.keyboardConfigId : player.gamepadConfigId;
	const std::string& currentConfigName = usesKeyboard ? player.keyboardConfigName : player.gamepadConfigName;
	const char* defaultConfigId = usesKeyboard ? "default_keyboard" : "default_gamepad";
	const char* defaultConfigName = usesKeyboard ? "Default Keyboard" : "Default Gamepad";

	if (ImGui::BeginCombo("Saved Key Config", currentConfigName.c_str()))
	{
		if (ImGui::Selectable(defaultConfigName, currentConfigId == defaultConfigId))
		{
			if (usesKeyboard)
			{
				player.keyboardConfigId = defaultConfigId;
				player.keyboardConfigName = defaultConfigName;
				player.keyboardConfig = BattleSetup::KeyboardKeyConfig{};
			}
			else
			{
				player.gamepadConfigId = defaultConfigId;
				player.gamepadConfigName = defaultConfigName;
				player.gamepadConfig = BattleSetup::GamepadKeyConfig{};
			}
		}

		for (const Input::NamedInputConfig& config : configs)
		{
			if (ImGui::Selectable(config.configName.c_str(), currentConfigId == config.configId))
			{
				ApplyNamedInputConfig(playerIndex, config);
			}
		}

		ImGui::Separator();
		if (ImGui::Selectable("New Config..."))
		{
			BeginNewInputConfig(playerIndex);
		}

		ImGui::EndCombo();
	}
}

/// <summary>
/// 新規キーコンフィグの名前入力、キー割り当て、保存ボタンを描画する。
/// </summary>
/// <param name="playerIndex">編集する Player 番号。</param>
void BattleSetupScene::DrawNewInputConfigEditor(int playerIndex)
{
	const bool editingKeyboard = editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard;
	ImGui::TextUnformatted(editingKeyboard ? "New Keyboard Config" : "New Gamepad Config");
	ImGui::InputText("Config Name", inputConfigNameBuffer.data(), inputConfigNameBuffer.size());

	if (editingKeyboard)
	{
		DrawKeyboardKeyConfig(playerIndex);
	}
	else
	{
		DrawGamepadKeyConfig(playerIndex);
	}

	if (ImGui::Button("Save Key Config", ImVec2(150.0f, 0.0f)))
	{
		SaveDraftInputConfig();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
	{
		CancelNewInputConfig();
	}
}

/// <summary>
/// 指定プレイヤーのキーボード割り当て UI を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawKeyboardKeyConfig(int playerIndex)
{
	const BattleSetup::KeyboardKeyConfig& config = GetKeyboardConfigForDraw(playerIndex);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::MoveUp, "Move Up Key", config.moveUp);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::MoveDown, "Move Down Key", config.moveDown);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::MoveLeft, "Move Left Key", config.moveLeft);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::MoveRight, "Move Right Key", config.moveRight);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::AttackA, "AttackA Key", config.attackA);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::AttackB, "AttackB Key", config.attackB);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::AttackX, "AttackX Key", config.attackX);
	DrawKeyboardBindingButton(playerIndex, BattleSetupCaptureTarget::AttackY, "AttackY Key", config.attackY);
}

/// <summary>
/// 指定プレイヤーのゲームパッド割り当て UI を描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
void BattleSetupScene::DrawGamepadKeyConfig(int playerIndex)
{
	const BattleSetup::GamepadKeyConfig& config = GetGamepadConfigForDraw(playerIndex);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::MoveUp, "Move Up Button", config.moveUp);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::MoveDown, "Move Down Button", config.moveDown);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::MoveLeft, "Move Left Button", config.moveLeft);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::MoveRight, "Move Right Button", config.moveRight);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::AttackA, "AttackA Button", config.attackA);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::AttackB, "AttackB Button", config.attackB);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::AttackX, "AttackX Button", config.attackX);
	DrawGamepadBindingButton(playerIndex, BattleSetupCaptureTarget::AttackY, "AttackY Button", config.attackY);
}

/// <summary>
/// キーボードの 1 項目分の割り当て変更ボタンを描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
/// <param name="target">変更対象の入力項目。</param>
/// <param name="label">項目名。</param>
/// <param name="currentKey">現在割り当てられているキー。</param>
void BattleSetupScene::DrawKeyboardBindingButton(int playerIndex, BattleSetupCaptureTarget target, const char* label, Input::KeyboardKey currentKey)
{
	const bool capturing =
		capturingPlayerIndex == playerIndex
		&& capturingDevice == BattleSetupCaptureDevice::Keyboard
		&& capturingTarget == target;
	const char* buttonText = capturing ? "Press key..." : ToKeyboardKeyText(currentKey);

	ImGui::TextUnformatted(label);
	ImGui::SameLine(170.0f);
	ImGui::PushID(static_cast<int>(target));
	if (ImGui::Button(buttonText, ImVec2(140.0f, 0.0f)))
	{
		BeginBindingCapture(playerIndex, BattleSetupCaptureDevice::Keyboard, target);
	}
	ImGui::PopID();
}

/// <summary>
/// ゲームパッドの 1 項目分の割り当て変更ボタンを描画する。
/// </summary>
/// <param name="playerIndex">設定する Player 番号。</param>
/// <param name="target">変更対象の入力項目。</param>
/// <param name="label">項目名。</param>
/// <param name="currentButton">現在割り当てられているボタン。</param>
void BattleSetupScene::DrawGamepadBindingButton(int playerIndex, BattleSetupCaptureTarget target, const char* label, Input::GamepadButton currentButton)
{
	const bool capturing =
		capturingPlayerIndex == playerIndex
		&& capturingDevice == BattleSetupCaptureDevice::Gamepad
		&& capturingTarget == target;
	const char* buttonText = capturing ? "Press button..." : ToGamepadButtonText(currentButton);

	ImGui::TextUnformatted(label);
	ImGui::SameLine(170.0f);
	ImGui::PushID(static_cast<int>(target));
	if (ImGui::Button(buttonText, ImVec2(140.0f, 0.0f)))
	{
		BeginBindingCapture(playerIndex, BattleSetupCaptureDevice::Gamepad, target);
	}
	ImGui::PopID();
}

/// <summary>
/// 現在選択中のデバイス種別をもとに、新規キーコンフィグ作成を開始する。
/// </summary>
/// <param name="playerIndex">作成対象の Player 番号。</param>
void BattleSetupScene::BeginNewInputConfig(int playerIndex)
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

	inputConfigNameBuffer.fill('\0');
	const char* defaultName = editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard
		? "New Keyboard Config"
		: "New Gamepad Config";
	std::snprintf(inputConfigNameBuffer.data(), inputConfigNameBuffer.size(), "%s", defaultName);
	statusMessage = "Editing new key config.";
}

/// <summary>
/// 新規キーコンフィグ作成を破棄し、保存済み選択表示へ戻る。
/// </summary>
void BattleSetupScene::CancelNewInputConfig()
{
	creatingInputConfig = false;
	editingInputConfigPlayerIndex = -1;
	capturingTarget = BattleSetupCaptureTarget::None;
	capturingPlayerIndex = -1;
	statusMessage = "Key config creation canceled.";
}

/// <summary>
/// 編集中のキーコンフィグを JSON に保存し、作成した設定を現在プレイヤーへ適用する。
/// </summary>
void BattleSetupScene::SaveDraftInputConfig()
{
	if (!creatingInputConfig || editingInputConfigPlayerIndex < 0)
	{
		return;
	}

	const std::string configName = inputConfigNameBuffer.data();
	if (configName.empty())
	{
		statusMessage = "Config name is required.";
		return;
	}

	Input::NamedInputConfig config;
	config.deviceKind = editingInputConfigDeviceKind;
	config.configId = Input::InputConfigLoader::GenerateNextConfigId(config.deviceKind);
	config.configName = configName;
	config.filePath = Input::InputConfigLoader::BuildConfigFilePath(config.deviceKind, config.configId);
	config.keyboardConfig = draftKeyboardConfig;
	config.gamepadConfig = draftGamepadConfig;

	if (!Input::InputConfigLoader::SaveNamedInputConfig(config))
	{
		statusMessage = "Key config save failed.";
		return;
	}

	RefreshInputConfigSummaries();
	ApplyNamedInputConfig(editingInputConfigPlayerIndex, config);
	creatingInputConfig = false;
	editingInputConfigPlayerIndex = -1;
	capturingTarget = BattleSetupCaptureTarget::None;
	capturingPlayerIndex = -1;
	statusMessage = "Saved key config: " + config.configName;
}

/// <summary>
/// 保存済みキーコンフィグを指定プレイヤーの現在設定へ反映する。
/// </summary>
/// <param name="playerIndex">適用先 Player 番号。</param>
/// <param name="config">適用する保存済みキーコンフィグ。</param>
void BattleSetupScene::ApplyNamedInputConfig(int playerIndex, const Input::NamedInputConfig& config)
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
void BattleSetupScene::BeginBindingCapture(int playerIndex, BattleSetupCaptureDevice device, BattleSetupCaptureTarget target)
{
	capturingPlayerIndex = playerIndex;
	capturingDevice = device;
	capturingTarget = target;
	captureWaitFrames = 1;
	statusMessage = "Waiting for input.";
}

/// <summary>
/// キーコンフィグ入力待ち中なら、押されたキーまたはボタンを対象項目へ反映する。
/// </summary>
void BattleSetupScene::UpdateBindingCapture()
{
	if (capturingTarget == BattleSetupCaptureTarget::None || capturingPlayerIndex < 0)
	{
		return;
	}

	if (captureWaitFrames > 0)
	{
		--captureWaitFrames;
		return;
	}

	if (Input::InputSystem::IsKeyboardKeyDown(Input::KeyboardKey::Escape))
	{
		capturingTarget = BattleSetupCaptureTarget::None;
		statusMessage = "Binding canceled.";
		return;
	}

	if (capturingDevice == BattleSetupCaptureDevice::Keyboard)
	{
		for (Input::KeyboardKey key : ConfigurableKeyboardKeys)
		{
			if (Input::InputSystem::IsKeyboardKeyDown(key))
			{
				ApplyCapturedKeyboardKey(key);
				return;
			}
		}
		return;
	}

	const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(capturingPlayerIndex)];
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
			ApplyCapturedGamepadButton(button);
			return;
		}
	}
}

/// <summary>
/// キャプチャした KeyboardKey を対象のキーコンフィグ項目へ反映する。
/// </summary>
/// <param name="key">新しく割り当てる KeyboardKey。</param>
void BattleSetupScene::ApplyCapturedKeyboardKey(Input::KeyboardKey key)
{
	if (IsKeyboardKeyAlreadyUsed(capturingPlayerIndex, capturingTarget, key))
	{
		capturingTarget = BattleSetupCaptureTarget::None;
		statusMessage = "That key is already assigned in this player config.";
		return;
	}

	BattleSetup::KeyboardKeyConfig& config = GetKeyboardConfigForEdit(capturingPlayerIndex);
	switch (capturingTarget)
	{
	case BattleSetupCaptureTarget::MoveLeft: config.moveLeft = key; break;
	case BattleSetupCaptureTarget::MoveRight: config.moveRight = key; break;
	case BattleSetupCaptureTarget::MoveDown: config.moveDown = key; break;
	case BattleSetupCaptureTarget::MoveUp: config.moveUp = key; break;
	case BattleSetupCaptureTarget::AttackA: config.attackA = key; break;
	case BattleSetupCaptureTarget::AttackB: config.attackB = key; break;
	case BattleSetupCaptureTarget::AttackX: config.attackX = key; break;
	case BattleSetupCaptureTarget::AttackY: config.attackY = key; break;
	case BattleSetupCaptureTarget::None:
	default:
		break;
	}

	capturingTarget = BattleSetupCaptureTarget::None;
	statusMessage = "Keyboard binding updated.";
}

/// <summary>
/// キャプチャした GamepadButton を対象のキーコンフィグ項目へ反映する。
/// </summary>
/// <param name="button">新しく割り当てる GamepadButton。</param>
void BattleSetupScene::ApplyCapturedGamepadButton(Input::GamepadButton button)
{
	if (IsGamepadButtonAlreadyUsed(capturingPlayerIndex, capturingTarget, button))
	{
		capturingTarget = BattleSetupCaptureTarget::None;
		statusMessage = "That button is already assigned in this player config.";
		return;
	}

	BattleSetup::GamepadKeyConfig& config = GetGamepadConfigForEdit(capturingPlayerIndex);
	switch (capturingTarget)
	{
	case BattleSetupCaptureTarget::MoveLeft: config.moveLeft = button; break;
	case BattleSetupCaptureTarget::MoveRight: config.moveRight = button; break;
	case BattleSetupCaptureTarget::MoveDown: config.moveDown = button; break;
	case BattleSetupCaptureTarget::MoveUp: config.moveUp = button; break;
	case BattleSetupCaptureTarget::AttackA: config.attackA = button; break;
	case BattleSetupCaptureTarget::AttackB: config.attackB = button; break;
	case BattleSetupCaptureTarget::AttackX: config.attackX = button; break;
	case BattleSetupCaptureTarget::AttackY: config.attackY = button; break;
	case BattleSetupCaptureTarget::None:
	default:
		break;
	}

	capturingTarget = BattleSetupCaptureTarget::None;
	statusMessage = "Gamepad binding updated.";
}

/// <summary>
/// 指定キーが同じプレイヤーの別項目に既に使われていないか確認する。
/// </summary>
/// <param name="playerIndex">確認する Player 番号。</param>
/// <param name="target">今回変更する対象項目。</param>
/// <param name="key">割り当て候補の KeyboardKey。</param>
/// <returns>別項目で使用済みなら true。</returns>
bool BattleSetupScene::IsKeyboardKeyAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::KeyboardKey key) const
{
	const BattleSetup::KeyboardKeyConfig& config = GetKeyboardConfigForDraw(playerIndex);
	return (target != BattleSetupCaptureTarget::MoveLeft && config.moveLeft == key)
		|| (target != BattleSetupCaptureTarget::MoveRight && config.moveRight == key)
		|| (target != BattleSetupCaptureTarget::MoveDown && config.moveDown == key)
		|| (target != BattleSetupCaptureTarget::MoveUp && config.moveUp == key)
		|| (target != BattleSetupCaptureTarget::AttackA && config.attackA == key)
		|| (target != BattleSetupCaptureTarget::AttackB && config.attackB == key)
		|| (target != BattleSetupCaptureTarget::AttackX && config.attackX == key)
		|| (target != BattleSetupCaptureTarget::AttackY && config.attackY == key);
}

/// <summary>
/// 指定ボタンが同じプレイヤーの別項目に既に使われていないか確認する。
/// </summary>
/// <param name="playerIndex">確認する Player 番号。</param>
/// <param name="target">今回変更する対象項目。</param>
/// <param name="button">割り当て候補の GamepadButton。</param>
/// <returns>別項目で使用済みなら true。</returns>
bool BattleSetupScene::IsGamepadButtonAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::GamepadButton button) const
{
	const BattleSetup::GamepadKeyConfig& config = GetGamepadConfigForDraw(playerIndex);
	return (target != BattleSetupCaptureTarget::MoveLeft && config.moveLeft == button)
		|| (target != BattleSetupCaptureTarget::MoveRight && config.moveRight == button)
		|| (target != BattleSetupCaptureTarget::MoveDown && config.moveDown == button)
		|| (target != BattleSetupCaptureTarget::MoveUp && config.moveUp == button)
		|| (target != BattleSetupCaptureTarget::AttackA && config.attackA == button)
		|| (target != BattleSetupCaptureTarget::AttackB && config.attackB == button)
		|| (target != BattleSetupCaptureTarget::AttackX && config.attackX == button)
		|| (target != BattleSetupCaptureTarget::AttackY && config.attackY == button);
}

/// <summary>
/// 指定プレイヤーが現在キーコンフィグ新規作成中か確認する。
/// </summary>
/// <param name="playerIndex">確認する Player 番号。</param>
/// <returns>指定プレイヤーの新規作成中なら true。</returns>
bool BattleSetupScene::IsCreatingInputConfigFor(int playerIndex) const
{
	return creatingInputConfig && editingInputConfigPlayerIndex == playerIndex;
}

/// <summary>
/// キーボードキー設定の表示対象を取得する。
/// </summary>
/// <param name="playerIndex">取得する Player 番号。</param>
/// <returns>新規作成中なら draft、通常時なら PlayerSetup のキー設定。</returns>
const BattleSetup::KeyboardKeyConfig& BattleSetupScene::GetKeyboardConfigForDraw(int playerIndex) const
{
	if (IsCreatingInputConfigFor(playerIndex)
		&& editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard)
	{
		return draftKeyboardConfig;
	}

	return setupData.players[static_cast<size_t>(playerIndex)].keyboardConfig;
}

/// <summary>
/// ゲームパッドボタン設定の表示対象を取得する。
/// </summary>
/// <param name="playerIndex">取得する Player 番号。</param>
/// <returns>新規作成中なら draft、通常時なら PlayerSetup のボタン設定。</returns>
const BattleSetup::GamepadKeyConfig& BattleSetupScene::GetGamepadConfigForDraw(int playerIndex) const
{
	if (IsCreatingInputConfigFor(playerIndex)
		&& editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Gamepad)
	{
		return draftGamepadConfig;
	}

	return setupData.players[static_cast<size_t>(playerIndex)].gamepadConfig;
}

/// <summary>
/// キーボードキー設定の編集対象を取得する。
/// </summary>
/// <param name="playerIndex">取得する Player 番号。</param>
/// <returns>新規作成中なら draft、通常時なら PlayerSetup のキー設定。</returns>
BattleSetup::KeyboardKeyConfig& BattleSetupScene::GetKeyboardConfigForEdit(int playerIndex)
{
	if (IsCreatingInputConfigFor(playerIndex)
		&& editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Keyboard)
	{
		return draftKeyboardConfig;
	}

	return setupData.players[static_cast<size_t>(playerIndex)].keyboardConfig;
}

/// <summary>
/// ゲームパッドボタン設定の編集対象を取得する。
/// </summary>
/// <param name="playerIndex">取得する Player 番号。</param>
/// <returns>新規作成中なら draft、通常時なら PlayerSetup のボタン設定。</returns>
BattleSetup::GamepadKeyConfig& BattleSetupScene::GetGamepadConfigForEdit(int playerIndex)
{
	if (IsCreatingInputConfigFor(playerIndex)
		&& editingInputConfigDeviceKind == BattleSetup::InputDeviceKind::Gamepad)
	{
		return draftGamepadConfig;
	}

	return setupData.players[static_cast<size_t>(playerIndex)].gamepadConfig;
}

/// <summary>
/// 現在のバトル開始設定が実行可能か検証する。
/// </summary>
/// <param name="outMessage">検証結果の表示メッセージ。</param>
/// <returns>バトル開始可能なら true。</returns>
bool BattleSetupScene::ValidateSetup(std::string& outMessage) const
{
	if (!BattleSetup::AreDevicesUnique(setupData))
	{
		outMessage = "1P and 2P cannot use the same input device.";
		return false;
	}

	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
		const BattleSetupCharacterSlotSummary& summary =
			characterSlotSummaries[static_cast<size_t>(player.characterSlotIndex)];
		if (!summary.hasSavedData)
		{
			outMessage = std::to_string(playerIndex + 1) + "P character slot is empty.";
			return false;
		}
	}

	for (int playerIndex = 0; playerIndex < Input::MaxPlayers; ++playerIndex)
	{
		const BattleSetup::PlayerSetup& player = setupData.players[static_cast<size_t>(playerIndex)];
		if (player.device.kind == BattleSetup::InputDeviceKind::Gamepad
			&& !Input::InputSystem::IsGamepadConnected(player.device.gamepadIndex))
		{
			// 未接続ゲームパッドは入力なしとして扱えるため、開発中の確認を止めない。
			outMessage = std::to_string(playerIndex + 1) + "P gamepad is disconnected. Battle can start without that input.";
			return true;
		}
	}

	outMessage = "Ready to start battle.";
	return true;
}

/// <summary>
/// 現在の設定を BattleScene へ渡して、バトル開始を予約する。
/// </summary>
void BattleSetupScene::StartBattle()
{
	Input::InputSystem::SetBindings(BattleSetup::BuildInputBindings(setupData));
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<BattleScene>(width, height, setupData));
}

/// <summary>
/// TitleScene への切り替えを予約する。
/// </summary>
void BattleSetupScene::RequestTitleScene()
{
	SceneManager::GetInstance().RequestChangeScene(
		std::make_unique<TitleScene>(width, height));
}
