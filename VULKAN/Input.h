#pragma once

#include <windows.h>

#include "Singleton.h"
#include "Callback.h"
#include "glm/glm.hpp"

#include <vector>
#include <map>

enum class SpecialKey: uint16_t
{
	Shift = MK_SHIFT,
	Ctrl = MK_CONTROL
};

class KeyInput
{
	friend class InputManager;
public:
	bool IsKeyPressed(WPARAM key) const { return m_keyPressed == key; }
	WPARAM GetKeyPressed() const { return m_keyPressed; }

private:
	KeyInput(WPARAM key) : m_keyPressed(key) { };
private:
	WPARAM		m_keyPressed;
};

class MouseInput
{
	friend class InputManager;
public:
	enum Button
	{
		Left,
		Middle,
		Right,
		Wheel,
		Count
	};

	enum ButtonState : uint8_t
	{
		None = 0,
		Down = 1,
		Up = 2
	};

	bool IsButtonDown(Button b) const { return (m_mouseButtonTargeted & (1 << (uint8_t)b)) != 0 && m_mouseButtonsState[(uint8_t)b] == ButtonState::Down; }
	bool IsButtonUp(Button b) const { return (m_mouseButtonTargeted & (1 << (uint8_t)b)) != 0 && m_mouseButtonsState[(uint8_t)b] == ButtonState::Up; }
	bool IsSpecialKeyPressed(SpecialKey key) const { return (m_specialKeyState & (uint16_t)key) != 0; } //should use underlaying cast of enum
	int GetWheelDelta() const { return m_wheelDelta; }

	const glm::uvec2& GetPoint() const { return m_mousePoint; }

	bool IsTriggered() const { return m_mouseButtonTargeted != 0; }
private:
	MouseInput() { Reset(); }
	void SetButtonState(Button b, ButtonState state) { m_mouseButtonTargeted = m_mouseButtonTargeted | (1 << (uint8_t)b); m_mouseButtonsState[(uint8_t)b] = state; }
	void SetSpecialKeyPressed(SpecialKey key) { m_specialKeyState = m_specialKeyState | (uint16_t)key; }
	void SetPoint(glm::vec2 point) { m_mousePoint = point; }
	void SetWheelDelta(int delta) { m_wheelDelta = delta; }

	void Reset() 
	{ 
		m_wheelDelta = 0;
		m_mouseButtonTargeted = 0;
		m_specialKeyState = 0;
		m_mousePoint = glm::vec2(0);
		memset(m_mouseButtonsState, 0, sizeof(m_mouseButtonsState));
	}
private:
	uint8_t		m_mouseButtonTargeted;
	ButtonState	m_mouseButtonsState[Button::Count];
	uint16_t	m_specialKeyState;
	glm::uvec2	m_mousePoint;
	int			m_wheelDelta;
};

class InputManager : public Singleton<InputManager>
{
	friend class Singleton<InputManager>;
public:
	typedef Callback1<bool, const KeyInput&>	KeyPressedCallback;
	typedef Callback1<bool, const MouseInput&>	MouseButtonsCallback;

	void MapKeyPressed(WPARAM key, KeyPressedCallback cb);
	void MapKeysPressed(std::vector<WPARAM> keys, KeyPressedCallback cb);

	void MapMouseButton(MouseButtonsCallback cb);

	void RegisterKeyboardEvent(WPARAM key);
	void RegisterMouseEvent(MouseInput::Button b, MouseInput::ButtonState state, WPARAM wparam, LPARAM lparam);
	void Update();
private:
	InputManager();
	virtual ~InputManager();

	void SetMouseSpecialKeyState(WPARAM wparam);
private:
	std::map<WPARAM, std::vector<KeyPressedCallback>>	m_keyboardMap;
	std::vector<MouseButtonsCallback>					m_mouseMap;

	std::vector<KeyInput>								m_keyboardInputs;
	MouseInput											m_mouseInput;
};