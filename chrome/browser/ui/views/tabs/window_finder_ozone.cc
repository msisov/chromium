// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "ui/views/widget/widget.h"

namespace {

gfx::NativeWindow GetLocalProcessWindowAtPointOzone(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<aura::Window*> root_windows;
  for (auto* browser : *BrowserList::GetInstance())
    root_windows.insert(browser->window()->GetNativeWindow());

  for (aura::Window* root : root_windows) {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(root);
    if (widget && widget->GetWindowBoundsInScreen().Contains(screen_point)) {
      aura::Window* content_window = widget->GetNativeWindow();

      // If we were instructed to ignore this window, ignore it.
      if (base::ContainsKey(ignore, content_window))
        continue;

      return content_window;
    }
  }
  return nullptr;
}

}  // namespace

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  NOTIMPLEMENTED()
      << "For Ozone builds, window finder is not supported for now.";
  return nullptr;
}
