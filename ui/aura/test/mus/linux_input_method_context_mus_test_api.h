// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_TEST_API_H_
#define UI_AURA_TEST_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_TEST_API_H_

#include "base/macros.h"
#include "ui/aura/mus/linux_input_method_context_delegate_impl.h"
#include "ui/aura/mus/linux_input_method_context_mus.h"

namespace aura {

class LinuxInputMethodContextMusTestApi {
 public:
  static void SetInputMethodContext(
      LinuxInputMethodContextMus* context_mus,
      ui::mojom::LinuxInputMethodContext* context) {
    context_mus->context_ = context;
  }

  static void SetInputMethodContextAndDelegate(
      LinuxInputMethodContextMus* context_mus,
      ui::mojom::LinuxInputMethodContext* context) {
    context_mus->context_ = context;
    context->Initialize(context_mus->delegate_->CreateInterfacePtrAndBind(),
                        true);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LinuxInputMethodContextMusTestApi);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_LINUX_INPUT_METHOD_CONTEXT_MUS_TEST_API_H_
