#include "Input/InputSystem.h"
#include "System/Renderer.h"
#include "Controller/TitleMenuController.h"
#include "UI/UIInputSystem.h"
#include "UI/UIRenderer.h"
#include "UI/UIRoot.h"
#include <cassert>
#include <iostream>
#include <wrl/client.h>
#include <ScreenGrab.h>
using namespace GameUI;
using Microsoft::WRL::ComPtr;
static ID3D11DeviceContext *testContext = nullptr;
static int composites = 0;
static const wchar_t* capturePath = nullptr;
ID3D11DeviceContext *Renderer::GetDeviceContext()
{
	return testContext;
}
void Renderer::DrawScreenRect(const RECT &, const DirectX::SimpleMath::Color &)
{
}
void Renderer::DrawFullscreenTexture(ID3D11ShaderResourceView *view, int width, int height)
{
	assert(view && width > 0 && height > 0);
	ComPtr<ID3D11Resource> resource;
	view->GetResource(resource.GetAddressOf());
	ComPtr<ID3D11Texture2D> source;
	assert(SUCCEEDED(resource.As(&source)));
	D3D11_TEXTURE2D_DESC description;
	source->GetDesc(&description);
	description.Usage = D3D11_USAGE_STAGING;
	description.BindFlags = 0;
	description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Device> device;
	source->GetDevice(device.GetAddressOf());
	ComPtr<ID3D11Texture2D> staging;
	assert(SUCCEEDED(device->CreateTexture2D(&description, nullptr, staging.GetAddressOf())));
	testContext->CopyResource(staging.Get(), source.Get());
	D3D11_MAPPED_SUBRESOURCE mapped;
	assert(SUCCEEDED(testContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
	size_t visiblePixels = 0;
	for (UINT y = 0; y < description.Height; ++y)
		for (UINT x = 0; x < description.Width; ++x)
			visiblePixels += static_cast<unsigned char *>(mapped.pData)[y * mapped.RowPitch + x * 4 + 3] != 0;
	testContext->Unmap(staging.Get(), 0);
	assert(visiblePixels > 20);
	if (capturePath)
		assert(SUCCEEDED(DirectX::SaveWICTextureToFile(testContext, source.Get(), GUID_ContainerFormatPng, capturePath)));
	++composites;
}
Input::InputActionMapId Input::InputSystem::GetActionMap()
{
	return Input::InputActionMapId::UI;
}
const Input::InputActionState &Input::InputSystem::GetActionState(int, Input::InputActionId)
{
	static Input::InputActionState empty;
	return empty;
}
int main()
{
	// タイトルの左右選択、静止マウス、決定押しっぱなし、クリック捕捉の回帰確認。
	{
		TitleMenuController title(1672, 941);
		title.Enter();
		InputFrame f;
		title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		title.UpdateInput(f, 0, false);
		title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::StartBattleSetup);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		f.pointerPosition = {400, 820}; // 左ボタン上にマウスが静止。
		title.UpdateInput(f, 1, false);
		title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::OpenCustomize);
		title.UpdateInput(f, 0, false);
		f.pointerDelta = {1, 0};
		title.UpdateInput(f, 0, false);
		f.pointerDelta = {};
		title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::StartBattleSetup);
		title.UpdateInput(f, 0, false);
		f.pointerPosition = {1100, 820}; f.pointerPressed = f.pointerDown = true;
		title.UpdateInput(f, 0, false);
		f.pointerPressed = f.pointerDown = false; f.pointerReleased = true;
		title.UpdateInput(f, 0, true); // 同時決定でもクリック先を一度だけ発行。
		assert(title.ConsumeCommand() == TitleMenuCommand::OpenCustomize);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		f = {}; f.pointerPosition = {400, 820}; f.pointerPressed = f.pointerDown = true;
		title.UpdateInput(f, 0, false);
		f.pointerPressed = f.pointerDown = false; f.pointerReleased = true; f.pointerPosition = {800, 500};
		title.UpdateInput(f, 0, false);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		title.Enter();
		f = {}; f.focusLost = true;
		title.UpdateInput(f, 1, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
		f = {}; title.UpdateInput(f, 0, true);
		assert(title.ConsumeCommand() == TitleMenuCommand::None);
	}
	// 配置方向に依存しないUI除外。左上UIの右側や非表示行まで消していた回帰を防ぐ。
	const Rect previewArea{0, 0, 640, 360};
	for (const Rect excluded : {Rect{4, 4, 328, 38}, Rect{312, 4, 636, 38},
		Rect{104, 44, 210, 74}, Rect{700, 0, 800, 40}, Rect{0, 0, 640, 360}})
	{
		const auto parts = previewArea.Subtract(excluded);
		for (int y = 0; y < 360; y += 3)
			for (int x = 0; x < 640; x += 3)
			{
				int count = 0;
				for (const Rect& part : parts) count += part.Contains({x, y});
				assert(count == (excluded.Contains({x, y}) ? 0 : 1));
			}
	}
	assert((Rect{0, 0, 0, 0}.Subtract({4, 4, 10, 10}).empty()));
	const Rect viewport{0, 0, 640, 480}, field{10, 10, 200, 50};
	{
		Root inlineRoot;
		const Rect section{260, 10, 620, 180}, preview{0, 10, 240, 300};
		std::string poseName;
		int choice = 0;
		const auto inlineFrame = [&](const InputFrame& f)
		{
			inlineRoot.BeginFrame(viewport, f);
			inlineRoot.AddTextInput("preset/name", {270, 20, 600, 50}, poseName, 127);
			inlineRoot.AddDropdown("preset/list", {270, 60, 600, 90}, {"Pose A", "Pose B"}, choice);
			inlineRoot.AddButton("other/button", {270, 300, 600, 340}, "Other");
			inlineRoot.EndFrame();
		};
		InputFrame f;
		f.pointerPressed = f.pointerReleased = true; f.pointerPosition = {280, 30};
		inlineFrame(f);
		assert(!inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f = {}; f.keysPressed[static_cast<size_t>(Key::Enter)] = true;
		inlineFrame(f);
		assert(!inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f = {}; f.pointerPressed = f.pointerReleased = true; f.pointerPosition = {280, 70};
		inlineFrame(f);
		assert(!inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f.pointerPosition = {280, 135};
		inlineFrame(f);
		assert(choice == 1 && !inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f.pointerPosition = {40, 200};
		inlineFrame(f);
		assert(!inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f.pointerPosition = {280, 310};
		inlineFrame(f);
		assert(inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
		f = {}; f.focusLost = true;
		inlineFrame(f);
		assert(inlineRoot.ShouldCollapseInlineSection("preset/", section, preview));
	}
	{
		Root editRoot;
		int value = 5;
		const auto numericFrame = [&](const InputFrame& f)
		{
			editRoot.BeginFrame(viewport, f);
			editRoot.AddIntegerInput("history/number", field, value, 0, 999);
			editRoot.EndFrame();
		};
		InputFrame f;
		f.pointerPosition = {20, 20};
		f.pointerPressed = f.pointerReleased = true;
		numericFrame(f);
		assert(editRoot.GetEditInteraction().ongoing); // 数値欄本体のクリックはステップ操作ではない。
		f = {}; f.textInput = L"2";
		numericFrame(f);
		assert(value == 5 && editRoot.GetEditInteraction().ongoing);
		f = {}; f.keysPressed[static_cast<size_t>(Key::Enter)] = true;
		numericFrame(f);
		assert(value == 52 && !editRoot.GetEditInteraction().ongoing);
		f = {}; f.pointerPosition = {190, 15}; f.pointerPressed = f.pointerDown = true;
		numericFrame(f);
		f = {}; f.pointerPosition = {190, 15}; f.pointerReleased = true;
		numericFrame(f);
		assert(value == 53 && editRoot.GetEditInteraction().id == "step/history/number");
		f = {}; f.pointerPosition = {30, 20}; f.wheelDelta = 120;
		numericFrame(f);
		assert(value == 54 && editRoot.GetEditInteraction().wheel);
		numericFrame({});
		assert(editRoot.GetEditInteraction().wheel); // 0.3秒の区切りは履歴側で判断する。
		f = {}; f.focusLost = true;
		numericFrame(f);
		assert(!editRoot.GetEditInteraction().ongoing);
	}
	Root root;
	InputFrame input;
	auto button = [&](bool enabled = true) {
		root.BeginFrame(viewport, input);
		root.AddButton("button", field, std::string_view("確認"), enabled);
		root.AddRule({10, 20, 200, 21}); // 装飾線がボタンのクリックを遮らないことも確認する。
		root.EndFrame();
	};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerDown = true;
	button();
	assert(!root.WasActivated("button") && root.WantsPointerCapture());
	input = {};
	input.pointerPosition = {300, 300};
	input.pointerReleased = true;
	button();
	assert(!root.WasActivated("button"));
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerReleased = true;
	button();
	assert(root.WasActivated("button"));
	button(false);
	assert(!root.WasActivated("button"));
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerReleased = true;
	root.BeginFrame(viewport, input);
	root.AddButton("image-backed-button", field, "画像背景");
	root.AddImage("overlay-image", field, "button");
	root.EndFrame();
	assert(root.WasActivated("image-backed-button"));
	input = {};
	input.pointerPosition = {20, 20};
	root.BeginFrame(viewport, input);
	root.AddImage("decorative-image", field, "panel");
	root.EndFrame();
	assert(!root.WantsPointerCapture());

	std::string name;
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerReleased = true;
	auto text = [&] {
		root.BeginFrame(viewport, input);
		root.AddTextInput("name", field, name, 12);
		root.EndFrame();
	};
	text();
	input = {};
	input.textInput = L"日本語";
	text();
	assert(name == "日本語");
	input = {};
	input.textInput = L"あいう";
	text();
	assert(name == "日本語");
	input = {};
	input.keysPressed[static_cast<size_t>(Key::Backspace)] = true;
	text();
	assert(name == "日本");
	input = {};
	input.composing = true;
	input.compositionText = L"変換";
	text();
	assert(name == "日本");
	input = {};
	input.keysPressed[static_cast<size_t>(Key::Escape)] = true;
	text();
	assert(name.empty());

	int number = 10;
	auto numeric = [&] {
		root.BeginFrame(viewport, input);
		root.AddIntegerInput("number", field, number, -20, 20);
		root.EndFrame();
	};
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = true;
	numeric();
	input = {};
	input.keysPressed[static_cast<size_t>(Key::Home)] = true;
	numeric();
	input = {};
	input.keysPressed[static_cast<size_t>(Key::Delete)] = true;
	numeric();
	numeric();
	input = {};
	input.textInput = L"-";
	numeric();
	assert(number == 10);
	input = {};
	input.textInput = L"50";
	numeric();
	assert(number == 10);
	input = {};
	input.keysPressed[static_cast<size_t>(Key::Enter)] = true;
	numeric();
	assert(number == -20);
	assert(!root.WantsKeyboardCapture() && !root.WantsTextInput() && root.GetFocusedId().empty());
	input = {};
	input.pointerPosition = {190, 15};
	input.pointerPressed = input.pointerReleased = true;
	numeric();
	assert(number == -19);
	input = {};
	input.pointerPosition = {190, 45};
	input.pointerPressed = input.pointerReleased = true;
	numeric();
	assert(number == -20);

	bool checked = false;
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerReleased = true;
	root.BeginFrame(viewport, input);
	root.AddCheckbox("check", field, "有効", checked);
	root.EndFrame();
	assert(checked);
	int selected = 0;
	const std::vector<std::string> options{"上段", "中段", "下段"};
	auto dropdown = [&] {
		root.BeginFrame(viewport, input);
		root.AddDropdown("select", field, options, selected);
		root.EndFrame();
	};
	dropdown();
	input = {};
	input.pointerPosition = {20, 110};
	input.pointerPressed = input.pointerReleased = true;
	dropdown();
	assert(selected == 1);
	// 実操作では押下と解放は別フレーム。候補を再登録しても捕捉を失わない。
	input = {};
	input.pointerPosition = {20, 20};
	input.pointerPressed = input.pointerReleased = true;
	dropdown();
	input = {};
	input.pointerPosition = {20, 150};
	input.pointerPressed = input.pointerDown = true;
	dropdown();
	assert(selected == 1);
	input.pointerPressed = false;
	dropdown();
	input.pointerDown = false;
	input.pointerReleased = true;
	dropdown();
	assert(selected == 2 && root.WasChanged("select"));

	int offset = 0;
	input = {};
	input.pointerPosition = {20, 20};
	input.wheelDelta = -120;
	root.BeginFrame(viewport, input);
	root.BeginScrollArea("scroll", {0, 0, 200, 100}, 400, offset);
	root.AddButton("scrollButton", {0, 0, 100, 20});
	root.EndScrollArea();
	root.EndFrame();
	assert(offset == 40);
	input = {};
	input.focusLost = true;
	button();
	assert(!root.WantsKeyboardCapture());

	UIInputSystem::Reset();
	UIInputSystem::HandleWindowMessage(nullptr, WM_CHAR, L'日', 0);
	UIInputSystem::Update(nullptr);
	assert(UIInputSystem::GetFrame().textInput == L"日");
	UIInputSystem::Update(nullptr);
	assert(UIInputSystem::GetFrame().textInput.empty());
	UIInputSystem::Reset();

	assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	assert(
		SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
									device.GetAddressOf(), nullptr, context.GetAddressOf())));
	testContext = context.Get();
	assert(UIRenderer::Initialize(device.Get(), 640, 480));
	assert(UIRenderer::LoadAssets("assets/texture/UI/Parts/assets.json"));
	// モーション編集用素材も本番マニフェストに登録され、再出力後に欠落しないこと。
	for (const char* image : {"undo", "redo", "rotate", "translate", "add", "delete", "copy", "paste",
		"mirror", "preset_save", "preset_load", "select_all", "deselect_all", "keyframe_movement"})
		assert(UIRenderer::HasImage(image));
	assert(UIRenderer::MeasureText(L"日本語", 18) > 0);
	assert(UIRenderer::MeasureText(L"重要項目", 18, true) > 0);
	Renderer renderer;
	Root styledRoot;
	int styledValue = 3, styledSelection = 1;
	bool styledCheck = true;
	const std::vector<std::string> styledOptions{"通常", "強調"};
	styledRoot.BeginFrame(viewport, {});
	styledRoot.AddPanel({8, 8, 360, 300});
	styledRoot.AddSectionHeader("styled/section", {16, 16, 340, 48}, "素材付きセクション");
	styledRoot.AddPrimaryButton("styled/primary", {16, 56, 180, 92}, "保存");
	styledRoot.AddIconButton("styled/icon", {200, 56, 340, 92}, "戻る", "back");
	styledRoot.AddImageButton("styled/image", {16, 100, 72, 136}, "play", "再生");
	styledRoot.AddIntegerInput("styled/number", {80, 100, 300, 136}, styledValue, 0, 10);
	styledRoot.AddDropdown("styled/dropdown", {16, 144, 300, 180}, styledOptions, styledSelection);
	styledRoot.AddCheckbox("styled/check", {16, 188, 300, 220}, "チェック", styledCheck);
	styledRoot.EndFrame();
	styledRoot.Draw(renderer);
	assert(composites == 1);

	DrawCommand command;
	command.type = DrawCommandType::Text;
	command.bounds = {0, 0, 200, 50};
	command.text = L"日本語の描画";
	UIRenderer::Draw(renderer, {command});
	assert(composites == 2);
	UIRenderer::Draw(renderer, {command});
	assert(composites == 3);
	assert(UIRenderer::Resize(800, 600));
	UIRenderer::Draw(renderer, {command});
	assert(composites == 4);
	DrawCommand imageCommand;
	imageCommand.type = DrawCommandType::Image;
	imageCommand.bounds = {20, 20, 160, 80};
	imageCommand.imageId = "button_primary";
	UIRenderer::Draw(renderer, {imageCommand});
	assert(composites == 5);
	DrawCommand nineSliceCommand = imageCommand;
	nineSliceCommand.type = DrawCommandType::NineSliceImage;
	nineSliceCommand.bounds = {20, 20, 400, 100};
	UIRenderer::Draw(renderer, {nineSliceCommand});
	assert(composites == 6);
	// 背景と操作UIを交互に描いても、それぞれの合成テクスチャが有効であること。
	UIRenderer::Draw(renderer, {command});
	UIRenderer::Draw(renderer, {nineSliceCommand});
	UIRenderer::Draw(renderer, {command});
	assert(composites == 9);
	for (const char* id : {"title_v2_logo", "title_v2_normal", "title_v2_selected"})
		assert(UIRenderer::HasImage(id));
	for (const auto size : {POINT{1280,720}, POINT{1920,1080}, POINT{800,600}})
	{
		assert(UIRenderer::Resize(size.x, size.y));
		TitleMenuController title(size.x, size.y);
		title.Enter();
		title.Draw(renderer); // WARP上で非透明ピクセルが存在することも確認。
		title.UpdateInput({}, 1, false);
		capturePath = size.x == 1280 ? L"x64/UIFoundationTests/title-v2-1280.png" : nullptr;
		title.Draw(renderer);
		capturePath = nullptr;
	}
	UIRenderer::Shutdown();
	context.Reset();
	device.Reset();
	CoUninitialize();
	std::cout << "UI foundation tests passed\n";
}
