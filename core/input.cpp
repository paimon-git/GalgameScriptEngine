#include "input.h"

bool Input::keyDown(int key) const
{
    return IsKeyDown(key);
}

bool Input::keyPressed(int key) const
{
    return IsKeyPressed(key);
}

bool Input::keyReleased(int key) const
{
    return IsKeyReleased(key);
}

bool Input::mouseDown(int button) const
{
    return IsMouseButtonDown(button);
}

bool Input::mousePressed(int button) const
{
    return IsMouseButtonPressed(button);
}

bool Input::mouseReleased(int button) const
{
    return IsMouseButtonReleased(button);
}

Vector2 Input::mousePosition() const
{
    return GetMousePosition();
}

bool Input::advancePressed() const
{
    return IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool Input::ctrlDown() const
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
}
