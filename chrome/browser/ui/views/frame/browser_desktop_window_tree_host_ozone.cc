// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_ozone.h"
#include "base/logging.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

////////////////////////////////////////////////////////////////////////////////
//// BrowserDesktopWindowTreeHostOzone, public:

BrowserDesktopWindowTreeHostOzone::BrowserDesktopWindowTreeHostOzone(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {
  // TODO: set custom frame?
}

BrowserDesktopWindowTreeHostOzone::~BrowserDesktopWindowTreeHostOzone() {
  LOG(ERROR) << __PRETTY_FUNCTION__;
}

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostOzone::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostOzone::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostOzone::UsesNativeSystemMenu() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostOzone(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               browser_view, browser_frame);
}
