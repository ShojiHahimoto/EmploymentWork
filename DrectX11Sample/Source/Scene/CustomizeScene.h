#pragma once

#include "Component/CameraComponent.h"
#include "Data/SkeletonPose.h"
#include "Component/TransformComponent.h"
#include "Data/AttackData.h"
#include "Data/CharacterData.h"
#include "Data/MotionData.h"
#include "Data/MotionSkeletonDefinition.h"
#include "Scene/Scene.h"
#include "System/Renderer.h"
#include "World/World.h"

#include <array>
#include <string>
#include <vector>

enum class CustomizeMode
{
	MainMenu,
	AttackCategorySelect,
	AttackSlotSelect,
	AttackEditor,
	MotionEditor,
	CommonMotionSelect,
	CharacterSlotSelect,
	CharacterEditor,
	AttackPicker
};

enum class CustomizeAttackCategory
{
	Ground,
	Air,
	Special
};

enum class CustomizeCharacterAttackSlotGroup
{
	Ground,
	Air,
	Special
};

/// <summary>
/// 技スロット一覧に表示する、保存済み AttackData の概要を保持する。
/// </summary>
struct CustomizeAttackSlotSummary
{
	bool hasSavedData = false;
	std::string displayName;
};

/// <summary>
/// キャラクタースロット一覧に表示する、保存済み CharacterData の概要を保持する。
/// </summary>
struct CustomizeCharacterSlotSummary
{
	bool hasSavedData = false;
	std::string characterName;
};

/// <summary>
/// キャラクター編集画面で一時的に保持する技スロット割り当て。
/// </summary>
struct CustomizeCharacterAttackSlotDraft
{
	std::string slotId;
	std::string attackDataId;
	std::string attackDisplayName;
	AttackSlotType slotType = AttackSlotType::Normal;
	AttackButtonId button = AttackButtonId::None;
	AttackUsableState slotUsableState = AttackUsableState::Ground;
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
	static constexpr int MaxAttackSlotCount = 20;
	static constexpr int CharacterSlotCount = 10;
	static constexpr int CharacterAttackButtonSlotCount = 4;
	static constexpr int CharacterNameBufferSize = 128;
	static constexpr int PreviewTextureWidth = 640;
	static constexpr int PreviewTextureHeight = 360;
	static constexpr int MotionEditorBoneCount = MotionBodyPartCount;
	static constexpr int CommonMotionSlotCount = 13;

	World world;
	CustomizeMode mode = CustomizeMode::MainMenu;
	CustomizeAttackCategory selectedCategory = CustomizeAttackCategory::Ground;
	int selectedSlotIndex = 0;
	int width = 0;
	int height = 0;

	AttackData draftAttack;
	std::array<char, 128> displayNameBuffer = {};
	std::array<char, 128> motionDataIdBuffer = {};
	MotionData draftMotion;
	bool hasDraftMotion = false;
	std::array<char, 128> motionDisplayNameBuffer = {};
	int selectedMotionEditorBoneIndex = 0;
	DirectX::SimpleMath::Vector3 motionKeyRotationEulerDegrees = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector2 attackMovementKeyOffset = DirectX::SimpleMath::Vector2::Zero;
	DirectX::SimpleMath::Vector3 motionRootOffsetKey = DirectX::SimpleMath::Vector3::Zero;
	bool editingCommonMotion = false;
	int selectedCommonMotionIndex = 0;
	std::string editingCommonMotionId;
	std::string editingAttackDataId;
	std::string statusMessage;
	std::array<std::array<CustomizeAttackSlotSummary, MaxAttackSlotCount>, 3> attackSlotSummaries = {};
	CharacterParameterData draftCharacterParameter;
	std::array<char, CharacterNameBufferSize> characterNameBuffer = {};
	std::array<CustomizeCharacterSlotSummary, CharacterSlotCount> characterSlotSummaries = {};
	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount> groundAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount> airAttackSlotDrafts = {};
	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount> specialAttackSlotDrafts = {};
	CustomizeCharacterAttackSlotGroup pickingSlotGroup = CustomizeCharacterAttackSlotGroup::Ground;
	int selectedCharacterSlotIndex = 0;
	int pickingSlotIndex = 0;
	std::string editingCharacterFolderPath;
	Renderer::RenderTexture previewRenderTexture;
	CameraComponent previewCamera;
	TransformComponent previewCameraTransform;
	TransformComponent previewPlayerTransform;
	SkeletonPose previewSkeletonPose;
	int previewCurrentFrame = 0;
	bool previewPlaying = false;
	float previewCameraYawDegrees = 0.0f;
	float previewCameraPitchDegrees = -2.5f;
	float previewCameraDistance = 14.0f;
	std::array<DirectX::SimpleMath::Vector3, MotionEditorBoneCount> copiedMotionPoseRotations = {};
	bool hasCopiedMotionPose = false;

	bool WasCancelTriggered();
	void RequestTitleScene();
	void NavigateBack();

	void DrawMainMenu();
	void DrawAttackCategorySelect();
	void DrawAttackSlotSelect();
	void DrawCommonMotionSelect();
	void DrawAttackEditor(Renderer& renderer);
	void DrawMotionEditorScreen(Renderer& renderer);
	void DrawAttackPreviewWindow(Renderer& renderer);
	void DrawPreviewPlaybackControls();
	void DrawAttackEditorWindow();
	void DrawAttackEditorControls();
	void DrawHitboxEditor();
	void DrawCancelSettingEditor();
	void DrawMotionEditor();
	void DrawMotionTimeline();
	void DrawCharacterSlotSelect();
	void DrawCharacterEditor();
	void DrawCharacterAttackSlotGroup(CustomizeCharacterAttackSlotGroup group, const char* label);
	void DrawAttackPicker();

	void SelectAttackSlot(CustomizeAttackCategory category, int slotIndex);
	void SelectCommonMotionSlot(int slotIndex);
	void SaveDraftAttack();
	void SyncDraftFromEditor();
	void EnsureDraftAttackMotionDataId();
	void LoadDraftMotionFromEditorId();
	void SaveDraftMotion();
	void AddWholeBodyMotionKeyframeAtPreviewFrame();
	void DeleteWholeBodyMotionKeyframeAtPreviewFrame();
	void SetMotionRotationKeyAtPreviewFrame();
	void AddAttackMovementKeyframeAtPreviewFrame();
	void DeleteAttackMovementKeyframeAtPreviewFrame();
	void SetAttackMovementKeyAtPreviewFrame();
	void AddMotionRootOffsetKeyframeAtPreviewFrame();
	void DeleteMotionRootOffsetKeyframeAtPreviewFrame();
	void SetMotionRootOffsetKeyAtPreviewFrame();
	void CopyWholeBodyMotionPoseAtPreviewFrame();
	void PasteWholeBodyMotionPoseAtPreviewFrame();
	void ApplyTPosePresetAtPreviewFrame();
	bool HasMotionKeyframeAtPreviewFrame() const;
	bool HasAttackMovementKeyframeAtPreviewFrame() const;
	bool HasMotionRootOffsetKeyframeAtPreviewFrame() const;
	void SelectCharacterSlot(int slotIndex);
	void SaveDraftCharacter();
	void CopyCharacterNameToBuffer();
	void RefreshCharacterSlotSummaries();
	void RefreshCharacterAttackSlotNames();
	void AssignAttackToCharacterSlot(const std::string& attackDataId, const AttackData& attackData);

	void InitializePreview();
	void ReleasePreview();
	void UpdatePreviewPlayback();
	void UpdatePreviewCameraTransform();
	void RenderAttackPreview(Renderer& renderer, const RECT* region = nullptr);
	void DrawPreviewAttackBoxes(Renderer& renderer);
	void ClampPreviewCurrentFrame();
	void StepPreviewFrame(int frameDelta);

	bool IsPreviewAttackActive() const;
	int GetPreviewTotalFrames() const;
	int GetPreviewActionFrame() const;
	const char* GetPreviewPhaseText() const;
	std::string GetEditingMotionDataId() const;

	int GetAttackSlotCount(CustomizeAttackCategory category) const;
	void RefreshAttackSlotSummaries(CustomizeAttackCategory category);
	std::string BuildAttackSlotButtonLabel(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildAttackDataId(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildMotionDataId(CustomizeAttackCategory category, int slotIndex) const;
	std::string BuildCommonMotionDataId(int slotIndex) const;
	AttackData CreateDefaultAttackData(CustomizeAttackCategory category, int slotIndex, const std::string& attackDataId) const;
	void CopyDisplayNameToBuffer();
	void CopyMotionDataIdToBuffer();
	void CopyMotionEditorBuffers();
	std::string BuildCharacterId(int slotIndex) const;
	std::string BuildCharacterFolderPath(int slotIndex) const;
	std::string BuildCharacterSlotButtonLabel(int slotIndex) const;
	CustomizeCharacterAttackSlotDraft CreateCharacterAttackSlotDraft(
		CustomizeCharacterAttackSlotGroup group,
		int slotIndex) const;
	std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& GetCharacterAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group);
	const std::array<CustomizeCharacterAttackSlotDraft, CharacterAttackButtonSlotCount>& GetCharacterAttackSlotDrafts(
		CustomizeCharacterAttackSlotGroup group) const;
	std::vector<CharacterAttackSlotData> BuildCharacterAttackSlotsForSave() const;
	bool AreRequiredCharacterAttackSlotsFilled(std::string& outMissingSlotName) const;
	bool IsAttackCompatibleWithCharacterSlotGroup(
		CustomizeCharacterAttackSlotGroup group,
		const AttackData& attackData) const;
};
