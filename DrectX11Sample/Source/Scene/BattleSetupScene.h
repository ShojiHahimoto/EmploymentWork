#pragma once

#include "Data/BattleSetupData.h"
#include "Input/InputConfigLoader.h"
#include "Scene/Scene.h"
#include "World/World.h"

#include <array>
#include <string>
#include <vector>

enum class BattleSetupCaptureDevice
{
	Keyboard,
	Gamepad
};

enum class BattleSetupCaptureTarget
{
	None,
	MoveLeft,
	MoveRight,
	MoveDown,
	MoveUp,
	AttackA,
	AttackB,
	AttackX,
	AttackY
};

struct BattleSetupCharacterSlotSummary
{
	bool hasSavedData = false;
	std::string characterId;
	std::string characterFolderPath;
	std::string characterName;
};

class BattleSetupScene : public Scene
{
public:
	BattleSetupScene(int initialWidth, int initialHeight);
	~BattleSetupScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	World world;
	BattleSetup::BattleSetupData setupData;
	std::array<BattleSetupCharacterSlotSummary, BattleSetup::CharacterSlotCount> characterSlotSummaries = {};
	std::vector<Input::NamedInputConfig> keyboardInputConfigs;
	std::vector<Input::NamedInputConfig> gamepadInputConfigs;
	std::array<char, 128> inputConfigNameBuffer = {};
	std::string statusMessage;
	int width = 0;
	int height = 0;
	int capturingPlayerIndex = -1;
	int captureWaitFrames = 0;
	bool creatingInputConfig = false;
	int editingInputConfigPlayerIndex = -1;
	BattleSetup::InputDeviceKind editingInputConfigDeviceKind = BattleSetup::InputDeviceKind::Keyboard;
	BattleSetupCaptureDevice capturingDevice = BattleSetupCaptureDevice::Keyboard;
	BattleSetupCaptureTarget capturingTarget = BattleSetupCaptureTarget::None;
	BattleSetup::KeyboardKeyConfig draftKeyboardConfig;
	BattleSetup::GamepadKeyConfig draftGamepadConfig;

	void RefreshCharacterSlotSummaries();
	void RefreshInputConfigSummaries();
	void DrawSetupWindow();
	void DrawPlayerSetup(int playerIndex);
	void DrawDeviceSelector(int playerIndex);
	void DrawCharacterSelector(int playerIndex);
	void DrawKeyConfig(int playerIndex);
	void DrawInputConfigSelector(int playerIndex);
	void DrawNewInputConfigEditor(int playerIndex);
	void DrawKeyboardKeyConfig(int playerIndex);
	void DrawGamepadKeyConfig(int playerIndex);
	void DrawKeyboardBindingButton(int playerIndex, BattleSetupCaptureTarget target, const char* label, Input::KeyboardKey currentKey);
	void DrawGamepadBindingButton(int playerIndex, BattleSetupCaptureTarget target, const char* label, Input::GamepadButton currentButton);
	void BeginNewInputConfig(int playerIndex);
	void CancelNewInputConfig();
	void SaveDraftInputConfig();
	void ApplyNamedInputConfig(int playerIndex, const Input::NamedInputConfig& config);
	void BeginBindingCapture(int playerIndex, BattleSetupCaptureDevice device, BattleSetupCaptureTarget target);
	void UpdateBindingCapture();
	void ApplyCapturedKeyboardKey(Input::KeyboardKey key);
	void ApplyCapturedGamepadButton(Input::GamepadButton button);
	bool IsKeyboardKeyAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::KeyboardKey key) const;
	bool IsGamepadButtonAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::GamepadButton button) const;
	bool IsCreatingInputConfigFor(int playerIndex) const;
	const BattleSetup::KeyboardKeyConfig& GetKeyboardConfigForDraw(int playerIndex) const;
	const BattleSetup::GamepadKeyConfig& GetGamepadConfigForDraw(int playerIndex) const;
	BattleSetup::KeyboardKeyConfig& GetKeyboardConfigForEdit(int playerIndex);
	BattleSetup::GamepadKeyConfig& GetGamepadConfigForEdit(int playerIndex);
	bool ValidateSetup(std::string& outMessage) const;
	void StartBattle();
	void RequestTitleScene();
};
