#pragma once

#include "Controller/CustomizeCharacterEditorController.h"
#include "Core/Renderer.h"
#include "UI/UIRoot.h"

#include <future>

class SaveWorker;
class World;
struct SceneLoadRequest;

class CustomizeCharacterFlowController
{
public:
	CustomizeCharacterFlowController(int width, int height);
	void DescribeLoad(SceneLoadRequest &request) const;
	void ApplyPrepared(CustomizeCharacterEditorController prepared);
	void Enter(World &world, SaveWorker *writer);
	void Exit();
	void Update(World &world);
	void Draw(World &world, Renderer &renderer) const;
	void Resize(int width, int height);

private:
	using SlotGroup = std::array<CustomizeCharacterAttackSlotDraft, 4>;
	struct Snapshot
	{
		std::string name;
		std::array<SlotGroup, 3> groups;
	};

	Snapshot Capture() const;
	void Restore(const Snapshot &snapshot);
	void CommitName();
	void Record(Snapshot before);
	void Undo();
	void Redo();
	void BuildUI();
	void BuildSlots(const GameUI::Rect &bounds);
	void BuildPicker(const GameUI::Rect &bounds);
	void ApplyUI(World &world, bool wasEditingText);
	void AddAttackFields(std::string_view id, const AttackData &attack, const GameUI::Rect &bounds, bool command);
	void NavigateBack();
	void Save();
	void PollSave();
	void ResetPreview(World &world, const AttackData *attack);
	void UpdatePreview(World &world);
	void ResizePreviewTexture();
	void DrawPreview(World &world) const;
	GameUI::Rect previewRegion;
	GameUI::Rect pickerRegion;

	CustomizeCharacterEditorController editor;
	GameUI::Root root;
	mutable Renderer::RenderTexture preview;
	SaveWorker *saveWorker = nullptr;
	std::future<std::string> pendingSave;
	std::string status;
	std::string name;
	std::optional<Snapshot> nameEdit;
	std::vector<Snapshot> undo;
	std::vector<Snapshot> redo;
	std::vector<size_t> pickerRows;
	bool pendingAirAttack = false;
	int airborneFrames = 0;
	bool initialAction = false;
	bool pickerOpen = false;
	int slotScroll = 0;
	int pickerScroll = 0;
	int width;
	int height;
};
