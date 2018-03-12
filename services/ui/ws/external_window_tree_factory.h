// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_

#include "services/ui/public/interfaces/external_window_tree_factory.mojom.h"

namespace ui {
namespace ws {

class WindowServer;

class ExternalWindowTreeFactory : public mojom::ExternalWindowTreeFactory {
 public:
  explicit ExternalWindowTreeFactory(WindowServer* window_server);
  ~ExternalWindowTreeFactory() override;

 private:
  void Create(mojom::WindowTreeRequest tree_request,
              mojom::WindowTreeClientPtr tree_client) override;

  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowTreeFactory);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_
