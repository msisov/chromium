// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_ACCESS_POLICY_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_ACCESS_POLICY_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/ui/ws/window_manager_access_policy.h"

namespace ui {
namespace ws {

class AccessPolicyDelegate;

// AccessPolicy for all clients, except the window manager.
class ExternalWindowAccessPolicy : public WindowManagerAccessPolicy {
 public:
  ExternalWindowAccessPolicy();
  ~ExternalWindowAccessPolicy() override;

 private:
  // WindowManagerAccessPolicy:
  bool CanSetWindowBounds(const ServerWindow* window) const override;
  bool CanSetWindowProperties(const ServerWindow* window) const override;
  bool CanStackAtTop(const ServerWindow* window) const override;
  bool CanInitiateMoveLoop(const ServerWindow* window) const override;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowAccessPolicy);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_ACCESS_POLICY_H_
