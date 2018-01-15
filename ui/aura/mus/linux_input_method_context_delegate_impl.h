// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_LINUX_INPUT_METHOD_DELEGATE_MUS_H_
#define UI_AURA_MUS_LINUX_INPUT_METHOD_DELEGATE_MUS_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/ime/linux_input_method_context.mojom.h"
#include "ui/aura/aura_export.h"

namespace ui {
class LinuxInputMethodContextDelegate;
}

namespace aura {

class AURA_EXPORT LinuxInputMethodContextDelegateImpl
    : public ui::mojom::LinuxInputMethodContextDelegate {
 public:
  explicit LinuxInputMethodContextDelegateImpl(
      ui::LinuxInputMethodContextDelegate* delegate);
  ~LinuxInputMethodContextDelegateImpl() override;

  ui::mojom::LinuxInputMethodContextDelegatePtr CreateInterfacePtrAndBind();

  // Overridden from ui::LinuxInputMethodContextDelegate:
  void OnCommit(const base::string16& text) override;
  void OnPreeditChanged(const ui::CompositionText& composition_text) override;
  void OnPreeditEnd() override;
  void OnPreeditStart() override;

 private:
  friend class LinuxInputMethodContextMusTestApi;

  // A set of callback functions.  Must not be nullptr.
  ui::LinuxInputMethodContextDelegate* delegate_;

  // Binds this object to the mojo interface.
  mojo::Binding<ui::mojom::LinuxInputMethodContextDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextDelegateImpl);
};

}  // namespace aura

#endif  // UI_AURA_MUS_LINUX_INPUT_METHOD_DELEGATE_MUS_H_
