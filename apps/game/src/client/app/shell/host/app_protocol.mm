// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/app_protocol.h"
#include "src/shared/base/exceptions.h"
#import <Cocoa/Cocoa.h>
#include <include/cef_application_mac.h>
#include <objc/runtime.h>

namespace shell {

namespace {
// Whether the app is inside sendEvent:, the state CefAppProtocol asks after.
BOOL g_handling = NO;
IMP g_send_event = nullptr; // SDL's own sendEvent:

BOOL isHandlingSendEvent(id, SEL) { return g_handling; }
void setHandlingSendEvent(id, SEL, BOOL handling) { g_handling = handling; }
void sendEvent(id self, SEL cmd, NSEvent *event)
{
	CefScopedSendingEvent sending; // sets and restores the flag around the event
	reinterpret_cast<void (*)(id, SEL, NSEvent *)>(g_send_event)(self, cmd, event);
}
} // namespace

void adoptAppProtocol()
{
	if (!NSApp)
		throw BaseException("shell: no NSApplication yet (SDL's video must be up before Chromium)");
	Class cls = [NSApp class];
	if ([NSApp conformsToProtocol:@protocol(CefAppProtocol)])
		return;
	class_addMethod(cls, @selector(isHandlingSendEvent), (IMP)isHandlingSendEvent, "c@:");
	class_addMethod(cls, @selector(setHandlingSendEvent:), (IMP)setHandlingSendEvent, "v@:c");
	Method send = class_getInstanceMethod(cls, @selector(sendEvent:));
	g_send_event = method_setImplementation(send, (IMP)sendEvent);
	class_addProtocol(cls, @protocol(CrAppProtocol));
	class_addProtocol(cls, @protocol(CrAppControlProtocol));
	class_addProtocol(cls, @protocol(CefAppProtocol));
	if (![NSApp conformsToProtocol:@protocol(CefAppProtocol)])
		throw BaseException("shell: SDL's NSApplication didn't take CefAppProtocol");
}

} // namespace shell
