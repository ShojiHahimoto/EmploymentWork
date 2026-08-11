#pragma once

#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Data/AttackData.h"
#include "Scene/Scene.h"
#include "System/Renderer.h"
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
	static constexpr int PreviewTextureWidth = 640;
	static constexpr int PreviewTextureHeight = 360;

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
	Renderer::RenderTexture previewRenderTexture;
	CameraComponent previewCamera;
	TransformComponent previewCameraTransform;
	TransformComponent previewPlayerTransform;
	int previewCurrentFrame = 0;
	bool previewPlaying = false;

	bool WasCancelTriggered();
	void RequestTitleScene();
	void NavigateBack();

	void DrawMainMenu();
	void DrawAttackCategorySelect();
	void DrawAttackSlotSelect();
	void DrawAttackEditor(Renderer& renderer);
	void DrawAttackPreviewWindow(Renderer& renderer);
	void DrawAttackEditorWindow();
	void DrawHitboxEditor();

	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);
	void SaveDraftAttack();
	void SyncDraftFromEditor();

	void InitializePreview();
	void ReleasePreview();
	void UpdatePreviewPlayback();
	void RenderAttackPreview(Renderer& renderer);
	void DrawPreviewAttackBoxes(Renderer& renderer);
	void ClampPreviewCurrentFrame();
	void StepPreviewFrame(int frameDelta);

	bool IsPreviewAttackActive() const;
	int GetPreviewTotalFrames() const;
	int GetPreviewActionFrame() const;
	const char* GetPreviewPhaseText() const;

	int GetAttackSlotCount(CustomizeAttackCategory category) const;
	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;
	AttackData CreateDefaultAttackData(CustomizeAttackCategory category, int slotIndex, const std::string& attackDataId) const;
	void CopyDisplayNameToBuffer();
};
