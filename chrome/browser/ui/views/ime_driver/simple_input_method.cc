// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/simple_input_method.h"

#include <utility>

SimpleInputMethod::SimpleInputMethod(ui::mojom::TextInputClientPtr client)
    : client_(std::move(client)) {}

SimpleInputMethod::~SimpleInputMethod() {}

void SimpleInputMethod::OnTextInputTypeChanged(
    ui::TextInputType text_input_type) {}

void SimpleInputMethod::OnCaretBoundsChanged(const gfx::Rect& caret_bounds) {}

void SimpleInputMethod::ProcessKeyEvent(std::unique_ptr<ui::Event> event,
                                        ProcessKeyEventCallback callback) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);
  DCHECK(event->IsKeyEvent());

  ui::KeyEvent* key_event = event->AsKeyEvent();
  if (!key_event->is_char() && key_event->type() == ui::ET_KEY_PRESSED)
    client_->InsertChar(std::move(event));

  std::move(callback).Run(false);
}

void SimpleInputMethod::CancelComposition() {}
