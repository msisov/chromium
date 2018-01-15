// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/linux_input_method_context_factory_mus.h"

#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/mus/linux_input_method_context_mus.h"

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// LinuxInputMethodContextFactoryMus, public:

LinuxInputMethodContextFactoryMus::LinuxInputMethodContextFactoryMus(
    service_manager::Connector* connector)
    : connector_(connector) {}

LinuxInputMethodContextFactoryMus::~LinuxInputMethodContextFactoryMus() {}

std::unique_ptr<ui::LinuxInputMethodContext>
LinuxInputMethodContextFactoryMus::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  auto context =
      std::make_unique<aura::LinuxInputMethodContextMus>(delegate, is_simple);
  context->Init(connector_);
  return std::move(context);
}

}  // namespace aura
