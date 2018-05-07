// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "ui/ozone/public/interfaces/wayland_connection.mojom.h"

namespace ui {

class WaylandConnection;

class WaylandConnectionConnector : public GpuPlatformSupportHost {
 public:
  WaylandConnectionConnector(WaylandConnection* connection);
  ~WaylandConnectionConnector() override;

  // GpuPlatformSupportHost:
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::RepeatingCallback<void(IPC::Message*)>& send_callback)
      override;
  void OnChannelDestroyed(int host_id) override;
  void OnMessageReceived(const IPC::Message& message) override;
  void OnGpuServiceLaunched(
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder) override;

 private:
  WaylandConnection* connection_ = nullptr;
  
  ozone::mojom::WaylandConnectionClientPtr wcp_ptr_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnectionConnector);
};

} // namespace ui

#endif // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_CONNECTOR_H_
