// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection_proxy.h"

#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace ui {

WaylandConnectionProxy::WaylandConnectionProxy(WaylandConnection* connection)
    : connection_(connection),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

WaylandConnectionProxy::~WaylandConnectionProxy() = default;

void WaylandConnectionProxy::SetWaylandConnection(
    ozone::mojom::WaylandConnectionPtr wc_ptr) {
  // Store current thread's task runner to be able to make mojo calls on a
  // right sequence.
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  wc_ptr.Bind(wc_ptr.PassInterface());
  wc_ptr_ = std::move(wc_ptr);
}

void WaylandConnectionProxy::CreateZwpLinuxDmabuf(base::File file,
                                                  uint32_t width,
                                                  uint32_t height,
                                                  uint32_t strides[],
                                                  uint32_t offsets[],
                                                  uint32_t current_format,
                                                  uint64_t modifiers[],
                                                  uint32_t planes_count,
                                                  uint32_t buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  const std::vector<uint32_t> strides_v(strides, strides + planes_count);
  const std::vector<uint32_t> offsets_v(offsets, offsets + planes_count);
  const std::vector<uint64_t> modifiers_v(modifiers, modifiers + planes_count);
  wc_ptr_->CreateZwpLinuxDmabuf(std::move(file), width, height, strides_v, offsets_v,
                                current_format, modifiers_v, planes_count, buffer_id);
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabuf(uint32_t buffer_id) {
  DCHECK(task_runner_);
  // Mojo calls must be done on a right sequence.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal,
                     base::Unretained(this), buffer_id));
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal(uint32_t buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->DestroyZwpLinuxDmabuf(buffer_id);
}

void WaylandConnectionProxy::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                                uint32_t buffer_id) {
  DCHECK(task_runner_);
  // Mojo calls must be done on a right sequence.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::ScheduleBufferSwapInternal,
                     base::Unretained(this), widget, buffer_id));
}

void WaylandConnectionProxy::ScheduleBufferSwapInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->ScheduleBufferSwap(widget, buffer_id);
}

WaylandWindow* WaylandConnectionProxy::GetWindow(
    gfx::AcceleratedWidget widget) {
  DCHECK(connection_ && !gbm_device_);
  return connection_->GetWindow(widget);
}

void WaylandConnectionProxy::ScheduleFlush() {
  DCHECK(connection_ && !gbm_device_);
  return connection_->ScheduleFlush();
}

wl_shm* WaylandConnectionProxy::shm() {
  DCHECK(connection_ && !gbm_device_);
  return connection_->shm();
}

intptr_t WaylandConnectionProxy::Display() {
  if (connection_)
    return reinterpret_cast<intptr_t>(connection_->display());

  // It must not be a single process mode. Thus, shared dmabuf approach is used,
  // which requires |gbm_device_|.
  DCHECK(gbm_device_);
  return EGL_DEFAULT_DISPLAY;
}

void WaylandConnectionProxy::AddBindingWaylandConnectionClient(
    ozone::mojom::WaylandConnectionClientRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui
