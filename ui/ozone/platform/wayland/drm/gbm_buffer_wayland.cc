// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/drm/gbm_buffer_wayland.h"

#include <drm_fourcc.h>
#include <gbm.h>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_device_linux.h"
#include "ui/ozone/common/linux/overlay_plane.h"
#include "ui/ozone/platform/wayland/drm/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmBufferWayland::GbmBufferWayland(
    WaylandConnectionProxy* connection,
    gbm_bo* bo,
    uint32_t format,
    uint32_t flags,
    uint64_t modifier,
    std::vector<base::ScopedFD>&& fds,
    const gfx::Size& size,
    const std::vector<gfx::NativePixmapPlane>&& planes)
    : GbmBufferLinux(bo,
                     format,
                     flags,
                     modifier,
                     std::move(fds),
                     size,
                     std::move(planes)),
      connection_(connection) {
  if (flags & GBM_BO_USE_SCANOUT) {
    gbm_bo* bo = GbmBufferLinux::bo();
    DCHECK(bo);

    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};

    for (int i = 0; i < gbm_bo_get_plane_count(bo); ++i) {
      handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
      strides[i] = gbm_bo_get_stride_for_plane(bo, i);
      offsets[i] = gbm_bo_get_offset(bo, i);
      if (modifier != DRM_FORMAT_MOD_INVALID)
        modifiers[i] = modifier;
    }

    base::ScopedFD fd(GetBoFd());
    base::File file(fd.release());
    // Asks Wayland to create a wl_buffer based on the |file| fd.
    connection_->CreateZwpLinuxDmabuf(
        std::move(file), GetWidth(), GetHeight(), GetStride(0), GetOffset(0),
        GetFormat(), GetFormatModifier(), GetHandle() /* use as buffer id */);
  }
}

GbmBufferWayland::~GbmBufferWayland() {
  // Asks Wayland to destroy a wl_buffer, which is based on the |bo_|'s fd.
  connection_->DestroyZwpLinuxDmabuf(GetHandle());
}

int GbmBufferWayland::GetBoFd() const {
  return gbm_bo_get_fd(GbmBufferLinux::bo());
}

uint32_t GbmBufferWayland::GetWidth() const {
  return gbm_bo_get_width(GbmBufferLinux::bo());
}

uint32_t GbmBufferWayland::GetHeight() const {
  return gbm_bo_get_height(GbmBufferLinux::bo());
}

uint32_t GbmBufferWayland::GetFramebufferId() const {
  NOTREACHED();
  return 0;
}

uint32_t GbmBufferWayland::GetOpaqueFramebufferId() const {
  NOTREACHED();
  return 0;
}

uint32_t GbmBufferWayland::GetFramebufferPixelFormat() const {
  NOTREACHED();
  return 0;
}

uint32_t GbmBufferWayland::GetOpaqueFramebufferPixelFormat() const {
  NOTREACHED();
  return 0;
}

const GbmDeviceLinux* GbmBufferWayland::GetGbmDeviceLinux() const {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<GbmBufferWayland> GbmBufferWayland::CreateBufferForBO(
    WaylandConnectionProxy* connection,
    gbm_bo* bo,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags) {
  DCHECK(bo);

  const uint64_t modifier = gbm_bo_get_format(bo);
  base::ScopedFD fd(gbm_bo_get_fd(bo));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to export buffer to dma_buf";
    gbm_bo_destroy(bo);
    return nullptr;
  }

  std::vector<base::ScopedFD> fds;
  fds.emplace_back(std::move(fd));

  std::vector<gfx::NativePixmapPlane> planes;
  planes.emplace_back(
      gbm_bo_get_stride_for_plane(bo, 0), gbm_bo_get_offset(bo, 0),
      gbm_bo_get_height(bo) *
          gbm_bo_get_stride_for_plane(bo, 0) /* find out if this is correct */,
      modifier);

  scoped_refptr<GbmBufferWayland> buffer(
      new GbmBufferWayland(connection, bo, format, flags, modifier,
                           std::move(fds), size, std::move(planes)));
  return buffer;
}

// static
scoped_refptr<GbmBufferWayland> GbmBufferWayland::CreateBuffer(
    WaylandConnectionProxy* connection,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  TRACE_EVENT1("Wayland", "GbmBufferWayland::CreateBuffer", "size", size.ToString());
  uint32_t flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      flags =
          GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
      break;
    default:
      NOTREACHED() << "Not supported buffer format";
      break;
  }

  uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);
  if (!gbm_device_is_format_supported(connection->gbm_device()->device(),
                                      fourcc_format, flags)) {
    LOG(FATAL) << "Not supported usage for this format";
  }

  DCHECK(connection);
  gbm_bo* bo = gbm_bo_create(connection->gbm_device()->device(), size.width(),
                             size.height(), fourcc_format, flags);
  if (!bo)
    return nullptr;
  return CreateBufferForBO(connection, bo, fourcc_format, size, flags);
}

GbmPixmapWayland::GbmPixmapWayland(
    WaylandSurfaceFactory* surface_manager,
    const scoped_refptr<GbmBufferWayland>& buffer)
    : GbmPixmapLinux(buffer), surface_manager_(surface_manager) {}

GbmPixmapWayland::~GbmPixmapWayland() {}

uint64_t GbmPixmapWayland::GetDmaBufModifier(size_t plane) const {
  // TODO(msisov): fix format modifiers.
  return 0;
}

bool GbmPixmapWayland::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    gfx::GpuFence* gpu_fence) {
  DCHECK(buffer()->GetFlags() & GBM_BO_USE_SCANOUT);
  GbmSurfacelessWayland* surfaceless = surface_manager_->GetSurface(widget);
  DCHECK(surfaceless);
  surfaceless->QueueOverlayPlane(
      OverlayPlane(buffer(), plane_z_order, plane_transform, display_bounds,
                   crop_rect, enable_blend, gpu_fence));
  return true;
}

}  // namespace ui
