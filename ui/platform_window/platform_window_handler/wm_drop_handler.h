// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DROP_HANDLER_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DROP_HANDLER_H_

#include "ui/platform_window/platform_window_handler/wm_platform_export.h"

namespace ui {
class PlatformWindowDelegate;

class WmDropHandler {
 public:
  // Notifies that Drag and Drop is completed or canceled and the session is
  // finished. If Drag and Drop is completed, |operation| has the result
  // operation.
  virtual void OnDragSessionClosed(int operation) = 0;

 protected:
  virtual ~WmDropHandler() {}
};

WM_PLATFORM_EXPORT void SetWmDropHandler(PlatformWindowDelegate* delegate,
                                         WmDropHandler* drop_handler);
WM_PLATFORM_EXPORT WmDropHandler* GetWmDropHandler(
    const PlatformWindowDelegate& delegate);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_DROP_HANDLER_H_
