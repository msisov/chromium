// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_access_policy.h"

#include "services/ui/ws/access_policy_delegate.h"
#include "services/ui/ws/server_window.h"

namespace ui {
namespace ws {

ExternalWindowAccessPolicy::ExternalWindowAccessPolicy() {}

ExternalWindowAccessPolicy::~ExternalWindowAccessPolicy() {}

bool ExternalWindowAccessPolicy::CanSetWindowBounds(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool ExternalWindowAccessPolicy::CanSetWindowProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool ExternalWindowAccessPolicy::CanStackAtTop(
    const ServerWindow* window) const {
  return WasCreatedByThisClient(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool ExternalWindowAccessPolicy::CanInitiateMoveLoop(
    const ServerWindow* window) const {
  return delegate_->HasRootForAccessPolicy(window);
}

}  // namespace ws
}  // namespace ui
