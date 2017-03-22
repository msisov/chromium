// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_MODE_REGISTRAR_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_MODE_REGISTRAR_H_

#include "services/ui/public/interfaces/external_window_mode_registrar.mojom.h"
#include "services/ui/public/interfaces/external_window_tree_host_factory.mojom.h"

namespace ui {
namespace ws {

class WindowServer;

class ExternalWindowModeRegistrar : public mojom::ExternalWindowModeRegistrar {
 public:
  explicit ExternalWindowModeRegistrar(WindowServer* window_server);
  ~ExternalWindowModeRegistrar() override;

 private:
  void Register(
      mojom::WindowTreeRequest tree_request,
      mojom::ExternalWindowTreeHostFactoryRequest tree_host_factory_request,
      mojom::WindowTreeClientPtr tree_client) override;

  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowModeRegistrar);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_MODE_REGISTRAR_H_
