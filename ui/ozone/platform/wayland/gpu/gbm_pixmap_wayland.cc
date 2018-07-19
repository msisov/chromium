// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drmMode.h>

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
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmPixmapWayland::GbmPixmapWayland(WaylandSurfaceFactory* surface_manager,
                                   WaylandConnectionProxy* connection)
    : surface_manager_(surface_manager), connection_(connection) {}

GbmPixmapWayland::~GbmPixmapWayland() {
  connection_->DestroyZwpLinuxDmabuf(GetUniqueId());
}

bool GbmPixmapWayland::InitializeBuffer(gfx::Size size,
                                        gfx::BufferFormat format,
                                        gfx::BufferUsage usage) {
  TRACE_EVENT1("Wayland", "GbmPixmapWayland::InitializeBuffer", "size",
               size.ToString());
  uint32_t flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      flags = GBM_BO_USE_LINEAR;
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      // mmap cannot be used with gbm buffers on a different process. That is,
      // Linux disallows this and "permission denied" is returned. To overcome
      // this and make software rasterization working, buffers must be created
      // on the browser process and gbm_bo_map must be used.
      // TODO(msisov): add support fir these two buffer usage cases.
      // https://crbug.com/864914
      LOG(FATAL) << "This scenario is not supported in Wayland now";
      break;
    default:
      NOTREACHED() << "Not supported buffer format";
      break;
  }

  const uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);
  if (!CreateBo(fourcc_format, size, flags)) {
    LOG(FATAL) << "Cannot create bo";
    return false;
  }

  CreateZwpLinuxDmabuf();
  return true;
}

bool GbmPixmapWayland::AreDmaBufFdsValid() const {
  return gbm_bo_->AreFdsValid();
}

size_t GbmPixmapWayland::GetDmaBufFdCount() const {
  return gbm_bo_->fd_count();
}

int GbmPixmapWayland::GetDmaBufFd(size_t plane) const {
  return gbm_bo_->GetFd(plane);
}

int GbmPixmapWayland::GetDmaBufPitch(size_t plane) const {
  return gbm_bo_->GetStride(plane);
}

int GbmPixmapWayland::GetDmaBufOffset(size_t plane) const {
  return gbm_bo_->GetOffset(plane);
}

uint64_t GbmPixmapWayland::GetDmaBufModifier(size_t plane) const {
  // TODO(msisov): figure out why returning format modifier results in
  // EGL_BAD_ALLOC.
  //
  // return gbm_bo_->get_format_modifier();
  return 0;
}

gfx::BufferFormat GbmPixmapWayland::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(gbm_bo_->format());
}

gfx::Size GbmPixmapWayland::GetBufferSize() const {
  return gbm_bo_->size();
}

uint32_t GbmPixmapWayland::GetUniqueId() const {
  return gbm_bo_->GetBoHandle();
}

bool GbmPixmapWayland::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  GbmSurfacelessWayland* surfaceless = surface_manager_->GetSurface(widget);
  DCHECK(surfaceless);
  surfaceless->QueueOverlayPlane(
      OverlayPlane(this, std::move(gpu_fence), plane_z_order, plane_transform,
                   display_bounds, crop_rect, enable_blend));
  return true;
}

gfx::NativePixmapHandle GbmPixmapWayland::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = GetBufferFormat();

  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < GetDmaBufFdCount()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(GetDmaBufFd(i))));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.planes.emplace_back(GetDmaBufPitch(i), GetDmaBufOffset(i),
                               gbm_bo_->GetPlaneSize(i), GetDmaBufModifier(i));
  }
  return handle;
}

bool GbmPixmapWayland::CreateBo(uint32_t format,
                                const gfx::Size& size,
                                uint32_t flags) {
  DCHECK(connection_);
  gbm_bo* bo = gbm_bo_create(connection_->gbm_device()->device(), size.width(),
                             size.height(), format, flags);
  if (!bo) {
    LOG(ERROR) << "Failed to create bo";
    return false;
  }

  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  const uint64_t modifier = gbm_bo_get_format(bo);
  size_t plane_count = gbm_bo_get_plane_count(bo);
  for (size_t i = 0; i < plane_count; ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and needs to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(gbm_bo_get_fd(bo));

    // TODO(dcastagna): support multiple fds.
    // crbug.com/642410
    if (!i) {
      if (!fd.is_valid()) {
        PLOG(ERROR) << "Failed to export buffer to dma_buf";
        gbm_bo_destroy(bo);
        return false;
      }
      fds.emplace_back(std::move(fd));
    }

    planes.emplace_back(
        gbm_bo_get_stride_for_plane(bo, i), gbm_bo_get_offset(bo, i),
        gbm_bo_get_height(bo) * gbm_bo_get_stride_for_plane(bo, i), modifier);
  }

  gbm_bo_.reset(new GbmBoWrapper(bo, format, flags, modifier, std::move(fds),
                                 size, std::move(planes)));
  return true;
}

void GbmPixmapWayland::CreateZwpLinuxDmabuf() {
  gbm_bo* bo = gbm_bo_->bo();
  uint64_t modifier = gbm_bo_->format_modifier();

  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;

  size_t plane_count = gbm_bo_get_plane_count(bo);
  for (size_t i = 0; i < plane_count; ++i) {
    strides.push_back(gbm_bo_get_stride_for_plane(bo, i));
    offsets.push_back(gbm_bo_get_offset(bo, i));
    if (modifier != DRM_FORMAT_MOD_INVALID)
      modifiers.push_back(modifier);
  }

  base::ScopedFD fd(HANDLE_EINTR(dup(gbm_bo_->GetFd(0))));
  if (!fd.is_valid()) {
    PLOG(FATAL) << "dup";
    return;
  }
  base::File file(fd.release());

  // Asks Wayland to create a wl_buffer based on the |file| fd.
  connection_->CreateZwpLinuxDmabuf(std::move(file), gbm_bo_->size(), strides,
                                    offsets, modifiers, gbm_bo_->format(),
                                    plane_count, GetUniqueId());
}

}  // namespace ui
