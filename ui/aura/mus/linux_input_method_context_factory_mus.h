// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_FACTORY_MUS_H_
#define UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_FACTORY_MUS_H_

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"

namespace service_manager {
class Connector;
}

namespace aura {

class AURA_EXPORT LinuxInputMethodContextFactoryMus
    : public ui::LinuxInputMethodContextFactory {
 public:
  explicit LinuxInputMethodContextFactoryMus(service_manager::Connector* connector);
  ~LinuxInputMethodContextFactoryMus() override;

  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

 private:
  service_manager::Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextFactoryMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_LINUX_INPUT_METHOD_CONTEXT_FACTORY_MUS_H_
