#pragma once

#include <cstdint>

void chatterinoSetMacOsActivationPolicyProhibited();

/// Lets the window with the native view @a nativeView show on every space,
/// full screen ones included, so it is there whichever space macOS brings up
/// rather than left behind on the one it opened on.
void chatterinoShowOnAllSpaces(std::uintptr_t nativeView);

/// Whether the window with the native view @a nativeView is on the space the
/// user is looking at
bool chatterinoIsOnActiveSpace(std::uintptr_t nativeView);

/// Brings the window with the native view @a nativeView to the front without
/// activating the app, so whatever the user is typing into keeps the keyboard
void chatterinoOrderFrontWithoutActivating(std::uintptr_t nativeView);

/// Brings the app to the front, switching to its space
void chatterinoActivateApp();

/// Puts the picture at @a path in the Dock in place of the one the program
/// was built with. An empty path brings the built-in one back.
void chatterinoSetMacOsDockIcon(const char *path);
