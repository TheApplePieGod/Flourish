#import "MacWindow.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

void MacOS::PollEvents()
{
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
    [event release];
}

void* MacOS::CreateWindowAndGetView(int width, int height)
{
    NSApplication* app = NSApplication.sharedApplication;
    app.activationPolicy = NSApplicationActivationPolicyRegular;

    NSRect frame = NSMakeRect(100, 250, width, height);
    NSUInteger windowStyle = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame styleMask:windowStyle backing:NSBackingStoreBuffered defer:NO];
    [window setTitle: @"Flourish"];
    
    [NSApp finishLaunching];
    [NSRunningApplication.currentApplication
        activateWithOptions:NSApplicationActivateAllWindows];

    [window makeKeyAndOrderFront: window];

    NSView* view = window.contentView;
    view.wantsLayer = YES;
    view.layer = [CAMetalLayer layer];

    return view;
}