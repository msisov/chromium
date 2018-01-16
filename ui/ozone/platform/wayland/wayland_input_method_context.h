// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_INPUT_METHOD_CONTEXT_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_INPUT_METHOD_CONTEXT_H_

#include "base/macros.h"
#include "services/ui/public/interfaces/ime/linux_input_method_context.mojom.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/ozone/platform/wayland/zwp_text_input_wrapper.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;
class ZWPTextInputWrapper;

class WaylandInputMethodContext : public ui::mojom::LinuxInputMethodContext,
                                  public ZWPTextInputWrapperClient {
 public:
  explicit WaylandInputMethodContext(WaylandConnection* connection);
  ~WaylandInputMethodContext() override;

  // ui::mojom::LinuxInputMethodContext.
  void Initialize(ui::mojom::LinuxInputMethodContextDelegatePtr delegate,
                  bool is_simple) override;
  void DispatchKeyEvent(std::unique_ptr<ui::Event> key_event,
                        DispatchKeyEventCallback callback) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;

  // ui::ZWPTextInputWrapperClient
  void OnPreeditString(const std::string& text, int preedit_cursor) override;
  void OnCommitString(const std::string& text) override;
  void OnKeysym(uint32_t key, uint32_t state, uint32_t modifiers) override;

 private:
  WaylandConnection* connection_ = nullptr;
  bool use_ozone_wayland_vkb_;

  std::unique_ptr<ZWPTextInputWrapper> text_input_;

  // Delegate interface back to IME code in ui.
  ui::mojom::LinuxInputMethodContextDelegatePtr delegate_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContext);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_INPUT_METHOD_CONTEXT_H_
