// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_LINUX_H_

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"

namespace ui {
class InputMethodAuraLinux;
}

// This bridges between mojo InputMethod API and aura::InputMethodBridgeLinux.
// It forwards the received events to an instance of
// aura::InputMethodBridgeLinux.
class InputMethodBridgeLinux : public ui::mojom::InputMethod {
 public:
  explicit InputMethodBridgeLinux(
      std::unique_ptr<RemoteTextInputClient> client);
  ~InputMethodBridgeLinux() override;

  // ui::mojom::InputMethod:
  void OnTextInputTypeChanged(ui::TextInputType text_input_type) override;
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override;
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       ProcessKeyEventCallback callback) override;
  void CancelComposition() override;

 private:
  std::unique_ptr<RemoteTextInputClient> client_;
  std::unique_ptr<ui::InputMethodAuraLinux> input_method_linux_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridgeLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_LINUX_H_
