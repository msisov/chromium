// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_tree_factory.h"

#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

ExternalWindowTreeFactory::ExternalWindowTreeFactory(
    WindowServer* window_server,
    const UserId& user_id)
    : window_server_(window_server), user_id_(user_id) {}

ExternalWindowTreeFactory::~ExternalWindowTreeFactory() {}

void ExternalWindowTreeFactory::Create(
    mojom::WindowTreeRequest tree_request,
    mojom::WindowTreeClientPtr tree_client) {
  // TODO(tonikitoo,msisov): Propose removing the bulk of "window manager"
  // suffix in methods and class names.
  bool automatically_create_display_roots = true;
  window_server_->CreateTreeForWindowManager(
      user_id_, std::move(tree_request), std::move(tree_client),
      automatically_create_display_roots);
}

}  // namespace ws
}  // namespace ui
