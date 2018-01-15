// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/linux_input_method_context_delegate_impl.h"

#include "ui/base/ime/linux/linux_input_method_context.h"

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMusOzone, public:

LinuxInputMethodContextDelegateImpl::LinuxInputMethodContextDelegateImpl(
    ui::LinuxInputMethodContextDelegate* delegate)
    : delegate_(delegate), binding_(this) {}

LinuxInputMethodContextDelegateImpl::~LinuxInputMethodContextDelegateImpl() {}

ui::mojom::LinuxInputMethodContextDelegatePtr
LinuxInputMethodContextDelegateImpl::CreateInterfacePtrAndBind() {
  ui::mojom::LinuxInputMethodContextDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

////////////////////////////////////////////////////////////////////////////////
// LinuxInputMethodContextMus, ui::mojom::LinuxInputMethodContextDelegate
// implementation:

void LinuxInputMethodContextDelegateImpl::OnCommit(const base::string16& text) {
  delegate_->OnCommit(text);
}

void LinuxInputMethodContextDelegateImpl::OnPreeditChanged(
    const ui::CompositionText& composition_text) {
  delegate_->OnPreeditChanged(composition_text);
}

void LinuxInputMethodContextDelegateImpl::OnPreeditEnd() {
  delegate_->OnPreeditEnd();
}

void LinuxInputMethodContextDelegateImpl::OnPreeditStart() {
  delegate_->OnPreeditStart();
}

}  // namespace aura
