// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection_connector.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/public/interfaces/wayland_connection.mojom.h"

namespace ui {

namespace {

using BinderCallback = ui::GpuPlatformSupportHost::GpuHostBindInterfaceCallback;

void BindInterfaceInGpuProcess(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe,
                               const BinderCallback& binder_callback) {
  return binder_callback.Run(interface_name, std::move(interface_pipe));
}

// TODO: share this with drm_device_connector.cc
template <typename Interface>
void BindInterfaceInGpuProcess(mojo::InterfaceRequest<Interface> request,
                               const BinderCallback& binder_callback) {
  BindInterfaceInGpuProcess(
      Interface::Name_, std::move(request.PassMessagePipe()), binder_callback);
}

}  // namespace

WaylandConnectionConnector::WaylandConnectionConnector(
    WaylandConnection* connection)
    : connection_(connection) {}

WaylandConnectionConnector::~WaylandConnectionConnector() = default;

void WaylandConnectionConnector::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::RepeatingCallback<void(IPC::Message*)>& send_callback) {}

void WaylandConnectionConnector::OnChannelDestroyed(int host_id) {}

void WaylandConnectionConnector::OnMessageReceived(
    const IPC::Message& message) {}

void WaylandConnectionConnector::OnGpuServiceLaunched(
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder) {
  ozone::mojom::WaylandConnectionPtr wc_ptr;
  connection_->binding_.Bind(MakeRequest(&wc_ptr));

  ozone::mojom::WaylandConnectionClientPtr wcp_ptr;
  auto request = mojo::MakeRequest(&wcp_ptr);
  BindInterfaceInGpuProcess(std::move(request), binder);
  DCHECK(wcp_ptr);
  wcp_ptr_ = std::move(wcp_ptr);
  wcp_ptr_->SetWaylandConnection(std::move(wc_ptr));
}

}  // namespace ui
