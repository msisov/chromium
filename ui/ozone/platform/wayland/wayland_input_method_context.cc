// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_input_method_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/zwp_text_input_wrapper_v1.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

constexpr int kXkbKeycodeOffset = 8;

} // namespace

WaylandInputMethodContext::WaylandInputMethodContext(
    WaylandConnection* connection)
    : connection_(connection), text_input_(nullptr) {
  use_ozone_wayland_vkb_ = getenv("ENABLE_WAYLAND_IME") ||
                           base::CommandLine::ForCurrentProcess()->HasSwitch(
                               switches::kEnableWaylandIme);
}

WaylandInputMethodContext::~WaylandInputMethodContext() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::Initialize(
    ui::mojom::LinuxInputMethodContextDelegatePtr delegate,
    bool is_simple) {
  delegate_ = std::move(delegate);
  if (use_ozone_wayland_vkb_ && !is_simple &&
      connection_->text_input_manager_v1()) {
    text_input_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_->text_input_manager_v1());
    text_input_->Initialize(connection_, this);
  }
}

void WaylandInputMethodContext::DispatchKeyEvent(
    std::unique_ptr<ui::Event> key_event,
    DispatchKeyEventCallback callback) {
  const bool handled = false;
  std::move(callback).Run(handled);
}

void WaylandInputMethodContext::Reset() {
  if (text_input_)
    text_input_->Reset();
}

void WaylandInputMethodContext::Focus() {
  if (!text_input_ || !connection_->GetCurrentKeyboardFocusedWindow())
    return;

  text_input_->Activate(connection_->GetCurrentKeyboardFocusedWindow());
  text_input_->ShowInputPanel();
}

void WaylandInputMethodContext::Blur() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
  if (text_input_)
    text_input_->SetCursorRect(rect);
}

void WaylandInputMethodContext::OnPreeditString(const std::string& text,
                                                int preedit_cursor) {
  gfx::Range selection_range = gfx::Range::InvalidRange();

  // TODO(jani) Handle selection range

  if (!selection_range.IsValid()) {
    int cursor_pos = (preedit_cursor) ? text.length() : preedit_cursor;
    selection_range.set_start(cursor_pos);
    selection_range.set_end(cursor_pos);
  }

  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  composition_text.selection = selection_range;
  delegate_->OnPreeditChanged(composition_text);
}

void WaylandInputMethodContext::OnCommitString(const std::string& text) {
  delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void WaylandInputMethodContext::OnKeysym(uint32_t key,
                                         uint32_t state,
                                         uint32_t modifiers) {
  uint8_t flags = 0;  // for now ignore modifiers
  DomKey dom_key = NonPrintableXKeySymToDomKey(key);
  KeyboardCode key_code = NonPrintableDomKeyToKeyboardCode(dom_key);
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(key_code + kXkbKeycodeOffset);
  if (dom_code == ui::DomCode::NONE)
    return;

  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  ui::KeyEvent event(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code,
                     dom_code, flags, dom_key, EventTimeForNow());
  connection_->DispatchUiEvent(&event);
}

}  // namespace ui
