// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_buffer_linux.h"

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
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

GbmBufferLinux::GbmBufferLinux(
    struct gbm_bo* bo,
    uint32_t format,
    uint32_t flags,
    uint64_t modifier,
    std::vector<base::ScopedFD>&& fds,
    const gfx::Size& size,
    const std::vector<gfx::NativePixmapPlane>&& planes)
    : bo_(bo),
      format_modifier_(modifier),
      format_(format),
      flags_(flags),
      fds_(std::move(fds)),
      size_(size),
      planes_(std::move(planes)) {}

GbmBufferLinux::~GbmBufferLinux() {
  if (bo_)
    gbm_bo_destroy(bo_);
}

bool GbmBufferLinux::AreFdsValid() const {
  if (fds_.empty())
    return false;

  for (const auto& fd : fds_) {
    if (fd.get() == -1)
      return false;
  }
  return true;
}

size_t GbmBufferLinux::GetFdCount() const {
  return fds_.size();
}

int GbmBufferLinux::GetFd(size_t index) const {
  DCHECK_LT(index, fds_.size());
  return fds_[index].get();
}

int GbmBufferLinux::GetStride(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].stride;
}

int GbmBufferLinux::GetOffset(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].offset;
}

size_t GbmBufferLinux::GetSize(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].size;
}

uint32_t GbmBufferLinux::GetHandle() const {
  return bo_ ? gbm_bo_get_handle(bo_).u32 : 0;
}

// TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
// as the size would be queried directly from the underlying bo.
gfx::Size GbmBufferLinux::GetSize() const {
  return size_;
}

uint64_t GbmBufferLinux::GetFormatModifier() const {
  return format_modifier_;
}

bool GbmBufferLinux::RequiresGlFinish() const {
  return false;
}

GbmPixmapLinux::GbmPixmapLinux(const scoped_refptr<GbmBufferLinux>& buffer)
    : buffer_(buffer) {}

gfx::NativePixmapHandle GbmPixmapLinux::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format =
      ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < buffer_->GetFdCount()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(buffer_->GetFd(i))));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.planes.emplace_back(buffer_->GetStride(i), buffer_->GetOffset(i),
                               buffer_->GetSize(i),
                               buffer_->GetFormatModifier());
  }
  return handle;
}

GbmPixmapLinux::~GbmPixmapLinux() {}

bool GbmPixmapLinux::AreDmaBufFdsValid() const {
  return buffer_->AreFdsValid();
}

size_t GbmPixmapLinux::GetDmaBufFdCount() const {
  return buffer_->GetFdCount();
}

int GbmPixmapLinux::GetDmaBufFd(size_t plane) const {
  return buffer_->GetFd(plane);
}

int GbmPixmapLinux::GetDmaBufPitch(size_t plane) const {
  return buffer_->GetStride(plane);
}

int GbmPixmapLinux::GetDmaBufOffset(size_t plane) const {
  return buffer_->GetOffset(plane);
}

uint64_t GbmPixmapLinux::GetDmaBufModifier(size_t plane) const {
  return buffer_->GetFormatModifier();
}

gfx::BufferFormat GbmPixmapLinux::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(buffer_->GetFormat());
}

gfx::Size GbmPixmapLinux::GetBufferSize() const {
  return buffer_->GetSize();
}

uint32_t GbmPixmapLinux::GetUniqueId() const {
  return buffer_->GetHandle();
}

}  // namespace ui
