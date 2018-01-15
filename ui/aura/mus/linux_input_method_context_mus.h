// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_H_
#define UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_H_

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/ime/linux_input_method_context.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace aura {

class LinuxInputMethodContextDelegateImpl;

class AURA_EXPORT LinuxInputMethodContextMus
    : public ui::LinuxInputMethodContext {
 public:
  using EventResultCallback = base::OnceCallback<void(bool)>;

  LinuxInputMethodContextMus(ui::LinuxInputMethodContextDelegate* delegate,
                             bool is_simple);
  ~LinuxInputMethodContextMus() override;

  void Init(service_manager::Connector* connector);
  void DispatchKeyEvent(const ui::KeyEvent& key_event,
                        std::unique_ptr<EventResultCallback> ack_callback);

  // Overridden from ui::LinuxInputMethodContext:
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;

 private:
  friend class LinuxInputMethodContextMusTestApi;

  // Runs all pending callbacks with UNHANDLED. This is called during shutdown,
  // to ensure we don't leave mus waiting for an ack.
  void AckPendingCallbacksUnhandled();

  // Called when the server responds to our request to process an event.
  void DispatchKeyEventCallback(const ui::KeyEvent& event, bool handled);

  // A set of callback functions.  Must not be nullptr.
  std::unique_ptr<LinuxInputMethodContextDelegateImpl> delegate_;

  bool is_simple_;

  // LinuxInputMethodContext interface.
  ui::mojom::LinuxInputMethodContextPtr context_ptr_;
  // Typically this is the same as |context_ptr_|, but it may be mocked
  // in tests.
  ui::mojom::LinuxInputMethodContext* context_ = nullptr;

  // Callbacks supplied to DispatchKeyEvent() are added here while awaiting
  // the response from the server. These are removed when the response is
  // received (ProcessKeyEventCallback()).
  base::circular_deque<std::unique_ptr<EventResultCallback>> pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_H_
