#include "util/MacOsHelpers.h"

#include <AppKit/AppKit.h>

void chatterinoSetMacOsActivationPolicyProhibited()
{
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyProhibited];
}

void chatterinoShowOnAllSpaces(std::uintptr_t nativeView)
{
    NSView *view = (NSView *)(void *)nativeView;
    NSWindow *window = [view window];
    if (window == nil)
    {
        return;
    }

    NSWindowCollectionBehavior behavior = [window collectionBehavior];
    // Moving to the active space and joining all of them exclude each other
    behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces |
                NSWindowCollectionBehaviorFullScreenAuxiliary;
    [window setCollectionBehavior:behavior];
}
