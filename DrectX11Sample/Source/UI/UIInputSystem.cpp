#include "UI/UIInputSystem.h"

#include "Input/InputSystem.h"

#include <imm.h>

#pragma comment(lib, "imm32.lib")

namespace GameUI
{
namespace
{
InputFrame currentFrame;
std::wstring pendingText;
std::wstring pendingComposition;
std::array<bool, static_cast<size_t>(Key::Count)> pendingKeys = {};
bool leftDown = false;
bool middleDown = false;
bool rightDown = false;
bool leftPressed = false;
bool leftReleased = false;
bool middlePressed = false;
bool middleReleased = false;
bool rightPressed = false;
bool rightReleased = false;
int pendingWheelDelta = 0;
POINT previousPointer = {};
bool hasPreviousPointer = false;
bool composing = false;
bool lostFocus = false;
int pendingTab = 0;
int navigationDirection = 0;
int navigationHeldFrames = 0;

void AddKeyPress(Key key)
{
	pendingKeys[static_cast<size_t>(key)] = true;
}

bool ReadCompositionString(HWND windowHandle, std::wstring &destination)
{
	HIMC inputContext = ImmGetContext(windowHandle);
	if (!inputContext)
	{
		return false;
	}

	const LONG byteCount = ImmGetCompositionStringW(inputContext, GCS_COMPSTR, nullptr, 0);
	if (byteCount < 0 || byteCount % static_cast<LONG>(sizeof(wchar_t)) != 0)
	{
		ImmReleaseContext(windowHandle, inputContext);
		return false;
	}

	destination.resize(static_cast<size_t>(byteCount) / sizeof(wchar_t));
	if (byteCount > 0)
	{
		ImmGetCompositionStringW(inputContext, GCS_COMPSTR, destination.data(),
								 static_cast<DWORD>(byteCount));
	}
	ImmReleaseContext(windowHandle, inputContext);
	return true;
}
} // namespace

void UIInputSystem::HandleWindowMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_LBUTTONDOWN:
		leftDown = true;
		leftPressed = true;

		break;
	case WM_LBUTTONUP:
		leftDown = false;
		leftReleased = true;

		break;
	case WM_MBUTTONDOWN:
		middleDown = true;
		middlePressed = true;

		break;
	case WM_MBUTTONUP:
		middleDown = false;
		middleReleased = true;

		break;
	case WM_RBUTTONDOWN:
		rightDown = true;
		rightPressed = true;

		break;
	case WM_RBUTTONUP:
		rightDown = false;
		rightReleased = true;

		break;
	case WM_MOUSEWHEEL:
		pendingWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
		break;
	case WM_KEYDOWN:
		if (composing)
			break;
		if (wParam == VK_TAB)
			pendingTab += (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1;
		switch (wParam)
		{
		case VK_BACK:
			AddKeyPress(Key::Backspace);
			break;
		case VK_DELETE:
			AddKeyPress(Key::Delete);
			break;
		case VK_LEFT:
			AddKeyPress(Key::Left);
			break;
		case VK_RIGHT:
			AddKeyPress(Key::Right);
			break;
		case VK_HOME:
			AddKeyPress(Key::Home);
			break;
		case VK_END:
			AddKeyPress(Key::End);
			break;
		case VK_UP:
			AddKeyPress(Key::Up);
			break;
		case VK_DOWN:
			AddKeyPress(Key::Down);
			break;
		case VK_RETURN:
			AddKeyPress(Key::Enter);
			break;
		case VK_ESCAPE:
			AddKeyPress(Key::Escape);
			break;
		}
		break;
	case WM_CHAR:
		if (wParam >= 0x20 && wParam != 0x7f)
		{
			pendingText.push_back(static_cast<wchar_t>(wParam));
		}
		break;
	case WM_IME_STARTCOMPOSITION:
		composing = true;
		break;
	case WM_IME_COMPOSITION:
		if (lParam & GCS_COMPSTR)
		{
			ReadCompositionString(windowHandle, pendingComposition);
		}
		if (lParam & GCS_RESULTSTR)
		{
			pendingComposition.clear();
		}
		break;
	case WM_IME_ENDCOMPOSITION:
		composing = false;
		pendingComposition.clear();
		break;
	case WM_KILLFOCUS:
		lostFocus = true;
		composing = false;
		pendingText.clear();
		pendingKeys.fill(false);
		pendingTab = 0;
		if (leftDown)
			leftReleased = true;
		if (middleDown)
			middleReleased = true;
		if (rightDown)
			rightReleased = true;
		leftDown = false;
		middleDown = false;
		rightDown = false;
		pendingComposition.clear();
		break;
	case WM_CAPTURECHANGED:
		// 他の操作へ捕捉が移った場合、ボタン確定ではなく取消として扱う。
		if (leftDown || middleDown || rightDown) lostFocus = true;
		if (leftDown)
			leftReleased = true;
		if (middleDown)
			middleReleased = true;
		if (rightDown)
			rightReleased = true;
		leftDown = false;
		middleDown = false;
		rightDown = false;
		break;
	default:
		break;
	}
}

void UIInputSystem::Update(HWND windowHandle)
{
	currentFrame = {};
	if (windowHandle)
	{
		POINT pointer = {};
		if (GetCursorPos(&pointer) && ScreenToClient(windowHandle, &pointer))
		{
			currentFrame.pointerPosition = pointer;
			if (hasPreviousPointer)
			{
				currentFrame.pointerDelta =
					POINT{pointer.x - previousPointer.x, pointer.y - previousPointer.y};
			}
			previousPointer = pointer;
			hasPreviousPointer = true;
		}
	}

	// ウィンドウ外で離した場合も押下状態を残さない。OS捕捉は既存UIから奪わない。
	if (windowHandle)
	{
		if (leftDown && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) { leftDown = false; leftReleased = true; }
		if (middleDown && !(GetAsyncKeyState(VK_MBUTTON) & 0x8000)) { middleDown = false; middleReleased = true; }
		if (rightDown && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) { rightDown = false; rightReleased = true; }
	}
	currentFrame.pointerPressed = leftPressed;
	currentFrame.pointerDown = leftDown;
	currentFrame.pointerReleased = leftReleased;
	currentFrame.middlePointerPressed = middlePressed;
	currentFrame.middlePointerDown = middleDown;
	currentFrame.middlePointerReleased = middleReleased;
	currentFrame.rightPointerPressed = rightPressed;
	currentFrame.rightPointerDown = rightDown;
	currentFrame.rightPointerReleased = rightReleased;
	currentFrame.wheelDelta = pendingWheelDelta;
	currentFrame.textInput.swap(pendingText);
	currentFrame.compositionText = pendingComposition;
	currentFrame.keysPressed = pendingKeys;
	currentFrame.composing = composing;
	currentFrame.focusLost = lostFocus;
	lostFocus = false;

	leftPressed = false;
	leftReleased = false;
	middlePressed = false;
	middleReleased = false;
	rightPressed = false;
	rightReleased = false;
	pendingWheelDelta = 0;
	pendingKeys.fill(false);

	currentFrame.focusMove = pendingTab;
	pendingTab = 0;
	// キーボードの文字入力と決定キーを混ぜない。パッドだけ既存UIアクションから取り込む。
	if (!composing && !currentFrame.focusLost &&
		Input::InputSystem::GetActionMap() == Input::InputActionMapId::UI)
	{
		const auto &move = Input::InputSystem::GetActionState(0, Input::InputActionId::Move);
		const int direction = move.deviceType != Input::InputDeviceType::Gamepad ? 0
							  : move.value.axis.y > 0.5f						 ? -1
							  : move.value.axis.y < -0.5f						 ? 1
																				 : 0;
		if (direction != navigationDirection)
		{
			navigationDirection = direction;
			navigationHeldFrames = 0;
			currentFrame.focusMove += direction;
		}
		else if (direction && ++navigationHeldFrames >= 18 && (navigationHeldFrames - 18) % 5 == 0)
			currentFrame.focusMove += direction;
		const auto &submit = Input::InputSystem::GetActionState(0, Input::InputActionId::Submit);
		const auto &cancel = Input::InputSystem::GetActionState(0, Input::InputActionId::Cancel);
		currentFrame.activatePressed = submit.deviceType == Input::InputDeviceType::Gamepad && submit.trigger;
		currentFrame.cancelPressed = cancel.deviceType == Input::InputDeviceType::Gamepad && cancel.trigger;
	}
	else
	{
		navigationDirection = 0;
		navigationHeldFrames = 0;
	}
	// WM_CHAR is the sole committed-text source. Never append GCS_RESULTSTR as well.
}

const InputFrame &UIInputSystem::GetFrame()
{
	return currentFrame;
}

void UIInputSystem::Reset()
{
	currentFrame = {};
	pendingText.clear();
	pendingComposition.clear();
	pendingKeys.fill(false);
	leftDown = middleDown = rightDown = false;
	leftPressed = leftReleased = false;
	middlePressed = middleReleased = false;
	rightPressed = rightReleased = false;
	pendingWheelDelta = 0;
	composing = false;
	lostFocus = false;
	pendingTab = 0;
	previousPointer = {};
	hasPreviousPointer = false;
	navigationDirection = 0;
	navigationHeldFrames = 0;
}
} // namespace GameUI
