#include "Input.h"

#include <windowsx.h>

InputManager::InputManager()
{
}

InputManager::~InputManager()
{

}

void InputManager::MapKeyPressed(WPARAM key, KeyPressedCallback cb)
{
	auto& callbacks = m_keyboardMap[key];
	callbacks.push_back(cb);
}

void InputManager::MapKeysPressed(std::vector<WPARAM> keys, KeyPressedCallback cb)
{
	for (auto key : keys)
		MapKeyPressed(key, cb);
}

void InputManager::MapMouseButton(MouseButtonsCallback cb)
{
	m_mouseMap.push_back(cb);
}

void InputManager::RegisterKeyboardEvent(WPARAM key)
{
	m_keyboardInputs.push_back(KeyInput(key));
}

void InputManager::RegisterMouseEvent(MouseInput::Button b, MouseInput::ButtonState state, WPARAM wparam, LPARAM lparam)
{
	m_mouseInput.SetPoint(glm::uvec2(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
	m_mouseInput.SetButtonState(b, state);
	m_mouseInput.SetWheelDelta(GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA);

	SetMouseSpecialKeyState(wparam);
}


void InputManager::SetMouseSpecialKeyState(WPARAM wparam)
{
	WORD keyMask = GET_KEYSTATE_WPARAM(wparam);
	if ((keyMask & MK_SHIFT))
		m_mouseInput.SetSpecialKeyPressed(SpecialKey::Shift);
	else if (keyMask & MK_CONTROL)
		m_mouseInput.SetSpecialKeyPressed(SpecialKey::Ctrl);
}

void InputManager::Update()
{
	for (const auto& ki : m_keyboardInputs)
	{
		auto cbs = m_keyboardMap.find(ki.GetKeyPressed());
		if (cbs != m_keyboardMap.end())
			for (auto& cb : cbs->second)
				cb(ki);
	}

	if (m_mouseInput.IsTriggered())
	{
		for (auto& cb : m_mouseMap)
			cb(m_mouseInput);
	}

	m_keyboardInputs.clear();
	m_mouseInput.Reset();
}