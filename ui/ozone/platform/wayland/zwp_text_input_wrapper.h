// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_ZWP_TEXT_INPUT_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_ZWP_TEXT_INPUT_WRAPPER_H_

#include "ui/ozone/platform/wayland/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

class ZWPTextInputWrapperClient {
 public:
  virtual ~ZWPTextInputWrapperClient() {}

  virtual void OnPreeditString(const std::string& text,
                               int32_t preedit_cursor) = 0;
  virtual void OnCommitString(const std::string& text) = 0;
  virtual void OnKeysym(uint32_t key, uint32_t state, uint32_t modifiers) = 0;
};

// A wrapper around different versions of zwp text inputs.
class ZWPTextInputWrapper {
 public:
  virtual ~ZWPTextInputWrapper() {}

  virtual void Initialize(WaylandConnection* connection,
                          ZWPTextInputWrapperClient* client) = 0;

  virtual void Reset() = 0;

  virtual void Activate(WaylandWindow* window) = 0;
  virtual void Deactivate() = 0;

  virtual void ShowInputPanel() = 0;
  virtual void HideInputPanel() = 0;

  virtual void SetCursorRect(const gfx::Rect& rect) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_ZWP_TEXT_INPUT_WRAPPER_H_
