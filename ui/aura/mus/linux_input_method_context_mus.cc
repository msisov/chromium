// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/linux_input_method_context_mus.h"

#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/aura/mus/linux_input_method_context_delegate_impl.h"
#include "ui/base/ime/text_input_client.h"

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMusOzone, public:

LinuxInputMethodContextMus::LinuxInputMethodContextMus(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple)
    : delegate_(
          std::make_unique<LinuxInputMethodContextDelegateImpl>(delegate)),
      is_simple_(is_simple) {}

LinuxInputMethodContextMus::~LinuxInputMethodContextMus() {
  // Mus won't dispatch the next key event until the existing one is acked. We
  // may have KeyEvents sent to IME context and awaiting the result, we need to
  // ack them otherwise mus won't process the next event until it times out.
  AckPendingCallbacksUnhandled();
}

void LinuxInputMethodContextMus::Init(service_manager::Connector* connector) {
  if (connector)
    connector->BindInterface(ui::mojom::kServiceName, &context_ptr_);
  context_ptr_->Initialize(delegate_->CreateInterfacePtrAndBind(), is_simple_);
  context_ = context_ptr_.get();
}

void LinuxInputMethodContextMus::DispatchKeyEvent(
    const ui::KeyEvent& key_event,
    std::unique_ptr<EventResultCallback> ack_callback) {
  // IME context will notify us whether it handled the event or not by calling
  // ProcessKeyEventCallback(), in which we will run the |ack_callback| to tell
  // the IME if context handled the event or not.
  pending_callbacks_.push_back(std::move(ack_callback));
  context_->DispatchKeyEvent(
      ui::Event::Clone(key_event),
      base::Bind(&LinuxInputMethodContextMus::DispatchKeyEventCallback,
                 base::Unretained(this), key_event));
}

////////////////////////////////////////////////////////////////////////////////
// LinuxInputMethodContextMus, ui::LinuxInputMethodContext implementation:

bool LinuxInputMethodContextMus::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  DispatchKeyEvent(key_event, nullptr);
  return false;
}

void LinuxInputMethodContextMus::SetCursorLocation(const gfx::Rect& rect) {
  context_->SetCursorLocation(rect);
}

void LinuxInputMethodContextMus::Reset() {
  context_->Reset();
}

void LinuxInputMethodContextMus::Focus() {
  context_->Focus();
}

void LinuxInputMethodContextMus::Blur() {
  context_->Blur();
}

void LinuxInputMethodContextMus::AckPendingCallbacksUnhandled() {
  for (auto& callback_ptr : pending_callbacks_) {
    if (callback_ptr)
      std::move(*callback_ptr).Run(false);
  }
  pending_callbacks_.clear();
}

void LinuxInputMethodContextMus::DispatchKeyEventCallback(
    const ui::KeyEvent& event,
    bool handled) {
  DCHECK(!pending_callbacks_.empty());
  std::unique_ptr<EventResultCallback> ack_callback =
      std::move(pending_callbacks_.front());
  pending_callbacks_.pop_front();

  // |ack_callback| can be null if the standard form of DispatchKeyEvent() is
  // called instead of the version which provides a callback. In mus+ash we
  // use the version with callback, but some unittests use the standard form.
  if (ack_callback)
    std::move(*ack_callback).Run(handled);
}

}  // namespace aura
