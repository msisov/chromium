// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/drm/gbm_buffer.h"

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
#include "ui/ozone/common/drm_util_linux.h"
#include "ui/ozone/common/gbm_device_base.h"
#include "ui/ozone/common/overlay_plane.h"
#include "ui/ozone/platform/wayland/drm/gbm_surfaceless.h"
#include "ui/ozone/platform/wayland/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmBuffer::GbmBuffer(WaylandConnectionProxy* connection,
                     gbm_bo* bo,
                     uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     std::vector<base::ScopedFD>&& fds,
                     const gfx::Size& size,
                     const std::vector<gfx::NativePixmapPlane>&& planes)
    : connection_(connection),
      bo_(bo),
      format_(format),
      flags_(flags),
      fds_(std::move(fds)),
      size_(size),
      planes_(std::move(planes)) {
  if (flags & GBM_BO_USE_SCANOUT) {
    DCHECK(bo_);
    format_modifier_ = modifier;

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

GbmBuffer::~GbmBuffer() {
  // Asks Wayland to destroy a wl_buffer, which is based on the |bo_|'s fd.
  connection_->DestroyZwpLinuxDmabuf(GetHandle());
  if (bo())
    gbm_bo_destroy(bo());
}

bool GbmBuffer::AreFdsValid() const {
  if (fds_.empty())
    return false;

  for (const auto& fd : fds_) {
    if (fd.get() == -1)
      return false;
  }
  return true;
}

size_t GbmBuffer::GetFdCount() const {
  return fds_.size();
}

int GbmBuffer::GetBoFd() const {
  return gbm_bo_get_fd(bo_);
}

uint32_t GbmBuffer::GetWidth() const {
  return gbm_bo_get_width(bo_);
}

uint32_t GbmBuffer::GetHeight() const {
  return gbm_bo_get_height(bo_);
}

int GbmBuffer::GetStride(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].stride;
}

int GbmBuffer::GetOffset(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].offset;
}

size_t GbmBuffer::GetSize(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].size;
}

uint32_t GbmBuffer::GetHandle() const {
  return bo() ? gbm_bo_get_handle(bo()).u32 : 0;
}

uint32_t GbmBuffer::GetFramebufferId() const {
  NOTREACHED();
  return 0;
}

uint32_t GbmBuffer::GetOpaqueFramebufferId() const {
  NOTREACHED();
  return 0;
}

gfx::Size GbmBuffer::GetSize() const {
  return size_;
}

uint32_t GbmBuffer::GetFramebufferPixelFormat() const {
  NOTREACHED();
  return 0;
}

uint32_t GbmBuffer::GetOpaqueFramebufferPixelFormat() const {
  NOTREACHED();
  return 0;
}

uint64_t GbmBuffer::GetFormatModifier() const {
  return format_modifier_;
}

const DrmDevice* GbmBuffer::GetDrmDevice() const {
  NOTREACHED();
  return nullptr;
}

bool GbmBuffer::RequiresGlFinish() const {
  return false;
}

scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferForBO(
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

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(connection, bo, format, flags,
                                                modifier, std::move(fds), size,
                                                std::move(planes)));
  return buffer;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    WaylandConnectionProxy* connection,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  TRACE_EVENT2("Wayland", "GbmBuffer::CreateBuffer", "device",
               "/dev/path" /* TODO: gbm->device_path().value() */, "size",
               size.ToString());
  uint32_t flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
      NOTREACHED();
      // TODO: find a solution to this. On Linux, these don't exist -
      // flags = GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE;
      break;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      NOTREACHED();
      // TODO: find a solution to this. Check the comment above.
      // flags = GBM_BO_USE_LINEAR;
      break;
  }

  uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);

  DCHECK(connection);
  gbm_bo* bo = gbm_bo_create(connection->gbm_device()->device(), size.width(),
                             size.height(), fourcc_format, flags);
  if (!bo)
    return nullptr;
  return CreateBufferForBO(connection, bo, fourcc_format, size, flags);
}

GbmPixmap::GbmPixmap(WaylandSurfaceFactory* surface_manager,
                     const scoped_refptr<GbmBuffer>& buffer)
    : surface_manager_(surface_manager), buffer_(buffer) {}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format =
      ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < buffer_->GetFdCount()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(buffer_->GetBoFd())));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.planes.emplace_back(buffer_->GetStride(i), buffer_->GetOffset(i),
                               buffer_->GetSize(i), 0 /* fix get modifiers */);
  }
  return handle;
}

GbmPixmap::~GbmPixmap() {}

void* GbmPixmap::GetEGLClientBuffer() const {
  return nullptr;
}

bool GbmPixmap::AreDmaBufFdsValid() const {
  return buffer_->AreFdsValid();
}

size_t GbmPixmap::GetDmaBufFdCount() const {
  return buffer_->GetFdCount();
}

int GbmPixmap::GetDmaBufFd(size_t plane) const {
  return buffer_->GetBoFd();
}

int GbmPixmap::GetDmaBufPitch(size_t plane) const {
  return buffer_->GetStride(plane);
}

int GbmPixmap::GetDmaBufOffset(size_t plane) const {
  return buffer_->GetOffset(plane);
}

uint64_t GbmPixmap::GetDmaBufModifier(size_t plane) const {
  // TODO(msisov): fix format modifiers.
  return 0;
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->GetSize();
}

uint32_t GbmPixmap::GetUniqueId() const {
  // We can use bo handle as unique ids as long as those are unique
  // for a given bo. This is used to control buffer commit order on Wayland
  // side.
  return buffer_->GetHandle();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect,
                                     bool enable_blend,
                                     gfx::GpuFence* gpu_fence) {
  DCHECK(buffer_->GetFlags() & GBM_BO_USE_SCANOUT);
  GbmSurfaceless* surfaceless = surface_manager_->GetSurface(widget);
  if (!surfaceless)
    return false;

  surfaceless->QueueOverlayPlane(
      OverlayPlane(buffer_, plane_z_order, plane_transform, display_bounds,
                   crop_rect, enable_blend, base::kInvalidPlatformFile));
  return true;
}

}  // namespace ui
