// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/external_window_tree_host_factory.mojom.h"
#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class WindowServer;

class ExternalWindowTreeHostFactory
    : public mojom::ExternalWindowTreeHostFactory {
 public:
  ExternalWindowTreeHostFactory(WindowServer* window_server,
                                const UserId& user_id);
  ~ExternalWindowTreeHostFactory() override;

  void AddBinding(mojom::ExternalWindowTreeHostFactoryRequest request);

 private:
  // mojom::WindowTreeHostFactory implementation.
  void CreatePlatformWindow(mojom::WindowTreeHostRequest tree_host_request,
                            Id client_id) override;

  WindowServer* window_server_;
  const UserId user_id_;
  mojo::BindingSet<mojom::ExternalWindowTreeHostFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowTreeHostFactory);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_
