// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_server_test_impl.h"

#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

WindowServerTestImpl::WindowServerTestImpl(WindowServer* window_server)
    : window_server_(window_server) {}

WindowServerTestImpl::~WindowServerTestImpl() {}

void WindowServerTestImpl::OnWindowPaint(
    const std::string& name,
    const EnsureClientHasDrawnRootWindowsCallback& cb,
    ServerWindow* window) {
  WindowTree* tree = window_server_->GetTreeWithClientName(name);
  if (!tree)
    return;
  if (tree->HasRoot(window) && window->has_created_compositor_frame_sink()) {
    painted_window_roots_[name]++;
    if (painted_window_roots_[name] == tree->roots().size()) {
      painted_window_roots_.erase(name);
      cb.Run(tree->roots().size());
      window_server_->SetPaintCallback(base::Callback<void(ServerWindow*)>());
    }
  }
}

void WindowServerTestImpl::EnsureClientHasDrawnRootWindows(
    const std::string& client_name,
    const EnsureClientHasDrawnRootWindowsCallback& callback) {
  if (painted_window_roots_.find(client_name) != painted_window_roots_.end()) {
    LOG(ERROR) << "EnsureClientHasDrawnRootWindows is already being executed "
                  "for that client name.";
    callback.Run(0);
    return;
  }

  painted_window_roots_[client_name] = 0;
  WindowTree* tree = window_server_->GetTreeWithClientName(client_name);
  if (tree) {
    for (const ServerWindow* window : tree->roots()) {
      if (window->has_created_compositor_frame_sink()) {
        painted_window_roots_[client_name]++;
      }
    }
    if (painted_window_roots_[client_name] == tree->roots().size()) {
      painted_window_roots_.erase(client_name);
      callback.Run(tree->roots().size());
      return;
    }
  }

  window_server_->SetPaintCallback(
      base::Bind(&WindowServerTestImpl::OnWindowPaint, base::Unretained(this),
                 client_name, std::move(callback)));
}

}  // namespace ws
}  // namespace ui
