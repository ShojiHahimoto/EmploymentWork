#pragma once

#include "Data/BattleSetupData.h"
#include "Input/InputConfigLoader.h"
#include "System/Renderer.h"

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

// Scene-owned editing state shared by the focused setup controllers.
struct BattleSetupSession
{
	BattleSetup::BattleSetupData setupData = BattleSetup::CreateDefaultBattleSetupData();
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
};

class BattleSetupControllerContext
{
protected:
	explicit BattleSetupControllerContext(BattleSetupSession& session);

	BattleSetupSession& session;
};

class BattleSetupDeviceAssignmentController : protected BattleSetupControllerContext
{
public:
	explicit BattleSetupDeviceAssignmentController(BattleSetupSession& session);
	void DrawDeviceSelector(int playerIndex, bool inputConfigEditing);

private:
	BattleSetup::BattleSetupData& setupData;
};

class BattleSetupCharacterSelectionController : protected BattleSetupControllerContext
{
public:
	explicit BattleSetupCharacterSelectionController(BattleSetupSession& session);
	void RefreshCharacterSlotSummaries();
	void DrawCharacterSelector(int playerIndex);
	bool HasSavedCharacterSlot(int slotIndex) const;

private:
	BattleSetup::BattleSetupData& setupData;
	std::array<BattleSetupCharacterSlotSummary, BattleSetup::CharacterSlotCount>& characterSlotSummaries;
};

class BattleSetupInputConfigController : protected BattleSetupControllerContext
{
public:
	explicit BattleSetupInputConfigController(BattleSetupSession& session);

	void RefreshInputConfigSummaries();
	void UpdateBindingCapture();
	void DrawKeyConfig(int playerIndex);
	bool IsCreatingInputConfigFor(int playerIndex) const;

private:
	BattleSetup::BattleSetupData& setupData;
	std::vector<Input::NamedInputConfig>& keyboardInputConfigs;
	std::vector<Input::NamedInputConfig>& gamepadInputConfigs;
	std::array<char, 128>& inputConfigNameBuffer;
	std::string& statusMessage;
	int& capturingPlayerIndex;
	int& captureWaitFrames;
	bool& creatingInputConfig;
	int& editingInputConfigPlayerIndex;
	BattleSetup::InputDeviceKind& editingInputConfigDeviceKind;
	BattleSetupCaptureDevice& capturingDevice;
	BattleSetupCaptureTarget& capturingTarget;
	BattleSetup::KeyboardKeyConfig& draftKeyboardConfig;
	BattleSetup::GamepadKeyConfig& draftGamepadConfig;

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
	void ApplyCapturedKeyboardKey(Input::KeyboardKey key);
	void ApplyCapturedGamepadButton(Input::GamepadButton button);
	bool IsKeyboardKeyAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::KeyboardKey key) const;
	bool IsGamepadButtonAlreadyUsed(int playerIndex, BattleSetupCaptureTarget target, Input::GamepadButton button) const;
	const BattleSetup::KeyboardKeyConfig& GetKeyboardConfigForDraw(int playerIndex) const;
	const BattleSetup::GamepadKeyConfig& GetGamepadConfigForDraw(int playerIndex) const;
	BattleSetup::KeyboardKeyConfig& GetKeyboardConfigForEdit(int playerIndex);
	BattleSetup::GamepadKeyConfig& GetGamepadConfigForEdit(int playerIndex);
};

class BattleSetupFlowController : protected BattleSetupControllerContext
{
public:
	BattleSetupFlowController(BattleSetupSession& session, int initialWidth, int initialHeight);

	void Enter();
	void Exit();
	void RunSystems();
	void Draw(Renderer& renderer);
	void OnResize(int newWidth, int newHeight);

private:
	BattleSetup::BattleSetupData& setupData;
	std::string& statusMessage;
	int& width;
	int& height;

	BattleSetupDeviceAssignmentController deviceAssignmentController;
	BattleSetupCharacterSelectionController characterSelectionController;
	BattleSetupInputConfigController inputConfigController;

	void DrawSetupWindow();
	bool ValidateSetup(std::string& outMessage) const;
	void StartBattle();
	void RequestTitleScene();
};
