// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_

#include "services/ui/public/interfaces/external_window_tree_factory.mojom.h"
#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class WindowServer;

class ExternalWindowTreeFactory : public mojom::ExternalWindowTreeFactory {
 public:
  ExternalWindowTreeFactory(WindowServer* window_server, const UserId& user_id);
  ~ExternalWindowTreeFactory() override;

  const UserId& user_id() const { return user_id_; }

 private:
  void Create(mojom::WindowTreeRequest tree_request,
              mojom::WindowTreeClientPtr tree_client) override;

  WindowServer* window_server_;
  const UserId user_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowTreeFactory);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_FACTORY_H_
