#include "cocoa-util.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

namespace rhi {

void* CocoaUtil::createMetalLayer(void* nswindow)
{
    NSWindow* window = (NSWindow*)nswindow;
    if (!window)
        return nullptr;
    // Preserve the host layer and its scale, filters, and resize policy. The caller
    // adopts an owned reference, including when the view already owns this layer.
    if ([window.contentView.layer isKindOfClass:[CAMetalLayer class]])
        return [window.contentView.layer retain];
    CAMetalLayer* layer = [[CAMetalLayer alloc] init];
    window.contentView.layer = layer;
    window.contentView.wantsLayer = YES;
    return layer;
}

void CocoaUtil::setMetalLayerVSync(void* metalLayer, bool enabled)
{
    [(CAMetalLayer*)metalLayer setDisplaySyncEnabled:enabled];
}

void* CocoaUtil::nextDrawable(void* metalLayer)
{
    CAMetalLayer* layer = (CAMetalLayer*)metalLayer;
    return [layer nextDrawable];
}

void CocoaUtil::destroyMetalLayer(void* metalLayer)
{
    CAMetalLayer* layer = (CAMetalLayer*)metalLayer;
    [layer release];
}

} // namespace rhi
