#import "MacWindow.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

void MacOS::PollEvents()
{
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
    [event release];
}

void* MacOS::CreateWindowAndGetView()
{
    NSApplication* app = NSApplication.sharedApplication;
    app.activationPolicy = NSApplicationActivationPolicyRegular;

    NSRect frame = NSMakeRect(100, 100, 500, 500);
    NSUInteger windowStyle = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSScreen.mainScreen.visibleFrame styleMask:windowStyle backing:NSBackingStoreBuffered defer:NO];
    //[window setBackgroundColor: [NSColor blueColor]];
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