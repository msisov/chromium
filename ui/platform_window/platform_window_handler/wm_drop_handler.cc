// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window_delegate.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WmDropHandler*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WmDropHandler*, kWmDropHandlerKey, nullptr);

void SetWmDropHandler(PlatformWindowDelegate* delegate,
                      WmDropHandler* drop_handler) {
  delegate->SetProperty(kWmDropHandlerKey, drop_handler);
}

WmDropHandler* GetWmDropHandler(const PlatformWindowDelegate& delegate) {
  return delegate.GetProperty(kWmDropHandlerKey);
  ;
}

}  // namespace ui
