#pragma once

#include "Data/AttackData.h"
#include "Scene/Scene.h"
#include "World/World.h"

#include <array>
#include <string>

enum class CustomizeMode
{
	MainMenu,
	AttackCategorySelect,
	AttackSlotSelect,
	AttackEditor
};

enum class CustomizeAttackCategory
{
	Ground,
	Air,
	Special
};

class CustomizeScene : public Scene
{
public:
	CustomizeScene(int initialWidth, int initialHeight);
	~CustomizeScene() override = default;

	void Enter() override;
	void Exit() override;
	void RunSystems() override;
	void Draw(Renderer& renderer) override;
	void OnResize(int newWidth, int newHeight) override;

	World& GetWorld() override;
	const World& GetWorld() const override;

private:
	static constexpr int GroundAttackSlotCount = 20;
	static constexpr int AirAttackSlotCount = 20;
	static constexpr int SpecialAttackSlotCount = 20;

	World world;
	CustomizeMode mode = CustomizeMode::MainMenu;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int selectedSlotIndex = 0;
	int width = 0;
	int height = 0;

	AttackData draftAttack;
	std::array<char, 128> displayNameBuffer = {};
	std::string editingAttackDataId;
	std::string statusMessage;

	bool WasCancelTriggered();
	void RequestTitleScene();
	void NavigateBack();

	void DrawMainMenu();
	void DrawAttackCategorySelect();
	void DrawAttackSlotSelect();
	void DrawAttackEditor();
	void DrawAttackEditorWindow();
	void DrawHitboxEditor();

	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);
	void SaveDraftAttack();
	void SyncDraftFromEditor();

	int GetAttackSlotCount(CustomizeAttackCategory category) const;
	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;
	AttackData CreateDefaultAttackData(CustomizeAttackCategory category, int slotIndex, const std::string& attackDataId) const;
	void CopyDisplayNameToBuffer();
};
