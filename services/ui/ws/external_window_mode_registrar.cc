// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_mode_registrar.h"

#include "services/ui/ws/external_window_tree_host_factory.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

ExternalWindowModeRegistrar::ExternalWindowModeRegistrar(
    WindowServer* window_server)
    : window_server_(window_server) {}

ExternalWindowModeRegistrar::~ExternalWindowModeRegistrar() {}

void ExternalWindowModeRegistrar::Register(
    mojom::WindowTreeRequest tree_request,
    mojom::ExternalWindowTreeHostFactoryRequest tree_host_factory_request,
    mojom::WindowTreeClientPtr tree_client) {
  std::unique_ptr<ExternalWindowTreeHostFactory> tree_host_factory(
      new ExternalWindowTreeHostFactory(window_server_));

  tree_host_factory->AddBinding(std::move(tree_host_factory_request));

  // TODO(tonikitoo,msisov): Propose removing the bulk of "window manager"
  // suffix in methods and class names.
  bool automatically_create_display_roots = true;
  window_server_->CreateTreeForWindowManager(
      std::move(tree_request), std::move(tree_client),
      automatically_create_display_roots);

  window_server_->set_window_tree_host_factory(std::move(tree_host_factory));
}

}  // namespace ws
}  // namespace ui
