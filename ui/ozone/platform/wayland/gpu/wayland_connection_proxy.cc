// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"

#include "base/process/process.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace ui {

namespace {

bool ValidateParameters(const base::File& file,
                        const gfx::Size& size,
                        const std::vector<uint32_t>& strides,
                        const std::vector<uint32_t>& offsets,
                        const std::vector<uint64_t>& modifiers,
                        uint32_t current_format,
                        uint32_t planes_count,
                        uint32_t buffer_id) {
  if (!file.IsValid())
    return false;
  if (size.IsEmpty())
    return false;
  if (planes_count <= 0)
    return false;
  if (planes_count != strides.size())
    return false;
  if (planes_count != offsets.size())
    return false;
  if (planes_count != modifiers.size())
    return false;
  if (buffer_id <= 0)
    CHECK(false);

  return file.IsValid() && !size.IsEmpty() && planes_count > 0 &&
         planes_count == strides.size() && planes_count == offsets.size() &&
         planes_count == modifiers.size() && buffer_id > 0 &&
         IsValidBufferFormat(current_format);
}

}  // namespace

WaylandConnectionProxy::WaylandConnectionProxy(WaylandConnection* connection)
    : connection_(connection),
      ui_runner_(base::ThreadTaskRunnerHandle::Get()) {}

WaylandConnectionProxy::~WaylandConnectionProxy() = default;

void WaylandConnectionProxy::SetWaylandConnection(
    ozone::mojom::WaylandConnectionPtr wc_ptr) {
  // Store current thread's task runner to be able to make mojo calls on the
  // right sequence.
  ui_runner_ = base::ThreadTaskRunnerHandle::Get();
  wc_ptr.Bind(wc_ptr.PassInterface());
  wc_ptr_ = std::move(wc_ptr);
}

void WaylandConnectionProxy::CreateZwpLinuxDmabuf(
    base::File file,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(ui_runner_);
  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::CreateZwpLinuxDmabufInternal,
                     base::Unretained(this), base::Passed(&file), size, strides, offsets, modifiers, current_format, planes_count, buffer_id));
}

void WaylandConnectionProxy::CreateZwpLinuxDmabufInternal(
    base::File file,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  // For security reasons, validate the data sent by the GPU process.
  if (!ValidateParameters(file, size, strides, offsets, modifiers,
                          current_format, planes_count, buffer_id)) {
    LOG(ERROR) << "Failed to import a dmabuf based wl_buffer";
    base::Process::Current().Terminate(1, false);
    return;
  }

  DCHECK(ui_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->CreateZwpLinuxDmabuf(std::move(file), size.width(), size.height(),
                                strides, offsets, current_format, modifiers,
                                planes_count, buffer_id);
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabuf(uint32_t buffer_id) {
  // Mojo calls must be done on the right sequence.
  DCHECK(ui_runner_);
  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal,
                     base::Unretained(this), buffer_id));
}

void WaylandConnectionProxy::DestroyZwpLinuxDmabufInternal(uint32_t buffer_id) {
  // For security reasons, validate the data sent by the GPU process.
  if (buffer_id < 1) {
    LOG(ERROR) << "Failed to destroy a dmabuf based wl_buffer";
    base::Process::Current().Terminate(1, false);
    return;
  }

  DCHECK(ui_runner_->BelongsToCurrentThread());
  DCHECK(wc_ptr_);
  wc_ptr_->DestroyZwpLinuxDmabuf(buffer_id);
}

void WaylandConnectionProxy::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                                uint32_t buffer_id) {
  // Mojo calls must be done on the right sequence.
  DCHECK(ui_runner_);
  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnectionProxy::ScheduleBufferSwapInternal,
                     base::Unretained(this), widget, buffer_id));
}

void WaylandConnectionProxy::ScheduleBufferSwapInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  // For security reasons, validate the data sent by the GPU process.
  if (buffer_id < 1 && widget == gfx::kNullAcceleratedWidget) {
    LOG(ERROR) << "Failed to swap a dmabuf based wl_buffer";
    base::Process::Current().Terminate(1, false);
    return;
  }

  DCHECK(ui_runner_->BelongsToCurrentThread());
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
