#pragma once

#include <cstdint>

void chatterinoSetMacOsActivationPolicyProhibited();

/// Lets the window with the native view @a nativeView show on every space,
/// full screen ones included, so it is there whichever space macOS brings up
/// rather than left behind on the one it opened on.
void chatterinoShowOnAllSpaces(std::uintptr_t nativeView);
