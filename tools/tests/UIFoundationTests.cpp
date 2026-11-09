#include "Input/InputSystem.h"
#include "System/Renderer.h"
#include "UI/UIInputSystem.h"
#include "UI/UIRenderer.h"
#include "UI/UIRoot.h"
#include <cassert>
#include <iostream>
#include <wrl/client.h>
using namespace GameUI;
using Microsoft::WRL::ComPtr;
static ID3D11DeviceContext *testContext = nullptr;
static int composites = 0;
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
	const Rect viewport{0, 0, 640, 480}, field{10, 10, 200, 50};
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
	UIRenderer::Shutdown();
	context.Reset();
	device.Reset();
	CoUninitialize();
	std::cout << "UI foundation tests passed\n";
}
