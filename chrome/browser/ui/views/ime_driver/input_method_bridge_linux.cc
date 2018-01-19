// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/input_method_bridge_linux.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/base/ime/input_method_auralinux.h"

InputMethodBridgeLinux::InputMethodBridgeLinux(
    std::unique_ptr<RemoteTextInputClient> client)
    : client_(std::move(client)),
      input_method_linux_(
          std::make_unique<ui::InputMethodAuraLinux>(client_.get())) {
  input_method_linux_->SetFocusedTextInputClient(client_.get());
}

InputMethodBridgeLinux::~InputMethodBridgeLinux() {}

void InputMethodBridgeLinux::OnTextInputTypeChanged(
    ui::TextInputType text_input_type) {
  client_->SetTextInputType(text_input_type);
  input_method_linux_->OnTextInputTypeChanged(client_.get());
}

void InputMethodBridgeLinux::OnCaretBoundsChanged(
    const gfx::Rect& caret_bounds) {
  client_->SetCaretBounds(caret_bounds);
  input_method_linux_->OnCaretBoundsChanged(client_.get());
}

void InputMethodBridgeLinux::ProcessKeyEvent(std::unique_ptr<ui::Event> event,
                                             ProcessKeyEventCallback callback) {
  DCHECK(event->IsKeyEvent());
  ui::KeyEvent* key_event = event->AsKeyEvent();
  if (!key_event->is_char()) {
    input_method_linux_->DispatchKeyEvent(key_event, std::move(callback));
  } else {
    const bool handled = false;
    std::move(callback).Run(handled);
  }
}

void InputMethodBridgeLinux::CancelComposition() {
  input_method_linux_->CancelComposition(client_.get());
}
