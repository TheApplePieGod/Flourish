#import "MacWindow.h"

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

void* MacOS::CreateWindowAndGetView()
{
    // NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    // [NSApplication sharedApplication];

    NSRect frame = NSMakeRect(0, 0, 500, 500);
    NSUInteger windowStyle = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    NSRect rect = [NSWindow contentRectForFrameRect:frame styleMask:windowStyle];

    NSWindow* window = [[[NSWindow alloc] initWithContentRect:rect styleMask:windowStyle backing:NSBackingStoreBuffered defer:NO] autorelease];
    [window setBackgroundColor: [NSColor blueColor]];
    [window makeKeyAndOrderFront: window];
    // // [window setTitle: [NSString stringWithUTF8String:title]];
    // [window orderFrontRegardless];

    // [pool drain];
    // [NSApp run];

    // NSView* view = [[[NSView alloc] initWithFrame:rect] autorelease];
    NSView* view = window.contentView;
    view.wantsLayer = YES;
    view.layer = [CAMetalLayer layer];

    return view;
}