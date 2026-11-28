#pragma once

#include "Data/BattleSetupData.h"
#include "Input/InputConfigLoader.h"
#include "Core/Renderer.h"
#include "UI/UIRoot.h"

#include <array>
#include <future>
#include <optional>
#include <string>
#include <vector>

class SaveWorker;

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
	std::array<std::array<std::string, 4>, 3> attackNames;
};

enum class BattleSetupControl : size_t { Left, Right, Up, Down, Confirm, Cancel, Count };
using BattleSetupControls = std::array<bool, static_cast<size_t>(BattleSetupControl::Count)>;

// Scene-owned editing state shared by the focused setup controllers.
struct BattleSetupSession
{
	BattleSetup::BattleSetupData setupData = BattleSetup::CreateDefaultBattleSetupData();
	std::array<BattleSetupCharacterSlotSummary, BattleSetup::CharacterSlotCount> characterSlotSummaries = {};
	std::vector<Input::NamedInputConfig> keyboardInputConfigs;
	std::vector<Input::NamedInputConfig> gamepadInputConfigs;
	std::array<bool, Input::MaxPlayers> characterConfirmed = {};
	std::array<bool, Input::MaxPlayers> ready = {};
	int cpuSide = -1;
	int activeSide = 0;
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
	void Update(const std::vector<BattleSetupControls>& controls);
	void BuildUI(GameUI::Root& root) const;
	bool IsComplete() const;
	bool WantsTitle() const;
	void Reset();

private:
	BattleSetup::BattleSetupData& setupData;
	std::array<int, Input::MaxPlayers> lockedDevices{-1, -1};
	bool titleRequested = false; // 未決定の機器から受けた、今フレームの戻る要求。
	std::vector<int> deviceSides;
};

class BattleSetupCharacterSelectionController : protected BattleSetupControllerContext
{
public:
	explicit BattleSetupCharacterSelectionController(BattleSetupSession& session);
	void RefreshCharacterSlotSummaries();
	void SelectFirst();
	void MoveSelection(int playerIndex, int horizontal, int vertical);
	void SelectCharacter(int playerIndex, int slotIndex);
	void BuildRoster(GameUI::Root& root, const GameUI::Rect& bounds, bool enabled) const;
	void BuildSummary(GameUI::Root& root, int playerIndex, const GameUI::Rect& bounds);
	bool HasSavedCharacterSlot(int slotIndex) const;

private:
	BattleSetup::BattleSetupData& setupData;
	std::array<BattleSetupCharacterSlotSummary, BattleSetup::CharacterSlotCount>& characterSlotSummaries;
	std::array<int, Input::MaxPlayers> attackScroll{};
};

class BattleSetupInputConfigController : protected BattleSetupControllerContext
{
public:
	explicit BattleSetupInputConfigController(BattleSetupSession& session);

	void RefreshInputConfigSummaries();
	void SetSaveWorker(SaveWorker *writer);
	void ApplyInputConfigSave();
	bool IsSaving() const;
	void UpdateBindingCapture();
	void Open(int playerIndex);
	void CyclePreset(int playerIndex, int direction);
	std::string PresetLabel(int playerIndex) const;
	void BuildUI(GameUI::Root& root, const GameUI::Rect& bounds);
	void HandleUI(const GameUI::Root& root);
	void MoveEditorFocus(int horizontal, int vertical);
	void Close();
	int Owner() const;

private:
	BattleSetup::BattleSetupData& setupData;
	std::vector<Input::NamedInputConfig>& keyboardInputConfigs;
	std::vector<Input::NamedInputConfig>& gamepadInputConfigs;
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
	SaveWorker *saveWorker = nullptr;
	std::future<std::optional<Input::NamedInputConfig>> pendingSave;
	int savingPlayerIndex = -1;

	int owner = -1;
	int editorFocus = 0;
	int PresetIndex(int playerIndex) const;
	void BuildConfigEditor(GameUI::Root& root, const GameUI::Rect& bounds);
	void BeginPresetEdit(int playerIndex);
	void CancelPresetEdit();
	void SaveDraftInputConfig();
	void ApplyNamedInputConfig(int playerIndex, const Input::NamedInputConfig& config);
	void BeginBindingCapture(int playerIndex, BattleSetupCaptureDevice device, BattleSetupCaptureTarget target);
	void ApplyCapturedKeyboardKey(Input::KeyboardKey key);
	void ApplyCapturedGamepadButton(Input::GamepadButton button);
};

class BattleSetupFlowController : protected BattleSetupControllerContext
{
public:
	BattleSetupFlowController(BattleSetupSession& session, int initialWidth, int initialHeight);

	void Enter(SaveWorker *writer, bool resumeCharacterSelection);
	void Exit();
	void Update();
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
	GameUI::Root root;
	std::vector<BattleSetupControls> previousControls;
	std::array<bool, Input::MaxPlayers> readyFocused{};
	bool selectingCharacters = false;

	void BuildUI(GameUI::InputFrame frame);
	void UpdateSetupUI();
	void UpdateCharacters(const std::vector<BattleSetupControls>& controls);
	void HandleCharacterInput(int playerIndex, const BattleSetupControls& controls);
	void MarkReady(int playerIndex);
	void GoBack();
	bool ValidateSetup(std::string& outMessage) const;
	void StartBattle();
	void RequestTitleScene();
};
