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

bool chatterinoIsOnActiveSpace(std::uintptr_t nativeView)
{
    NSWindow *window = [(NSView *)(void *)nativeView window];
    // Without a window there is no space to switch to
    return window == nil || [window isOnActiveSpace];
}

void chatterinoOrderFrontWithoutActivating(std::uintptr_t nativeView)
{
    NSWindow *window = [(NSView *)(void *)nativeView window];
    if (window != nil)
    {
        [window orderFrontRegardless];
    }
}

void chatterinoActivateApp()
{
    if (@available(macOS 14.0, *))
    {
        [NSApp activate];
    }
    else
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    }
}

void chatterinoSetMacOsDockIcon(const char *path)
{
    NSApplication *app = [NSApplication sharedApplication];
    if (path == nullptr || path[0] == '\0')
    {
        // Back to the icon of the bundle
        [app setApplicationIconImage:nil];
        return;
    }

    NSString *file = [NSString stringWithUTF8String:path];
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:file];
    if (image != nil)
    {
        [app setApplicationIconImage:image];
    }
}
