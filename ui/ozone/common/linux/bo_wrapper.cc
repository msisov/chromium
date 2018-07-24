// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/bo_wrapper.h"

#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

BoWrapper::BoWrapper(uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     std::vector<base::ScopedFD> fds,
                     const gfx::Size& size,
                     std::vector<gfx::NativePixmapPlane> planes)
  :
      format_modifier_(modifier),
      format_(format),
      flags_(flags),
      fds_(std::move(fds)),
      size_(size),
      planes_(std::move(planes)) {}

BoWrapper::~BoWrapper() {}

bool BoWrapper::AreFdsValid() const {
  if (fds_.empty())
    return false;

  for (const auto& fd : fds_) {
    if (fd.get() == -1)
      return false;
  }
  return true;
}

int BoWrapper::GetFd(size_t index) const {
  DCHECK_LT(index, fds_.size());
  return fds_[index].get();
}

int BoWrapper::GetStride(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].stride;
}

int BoWrapper::GetOffset(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].offset;
}

size_t BoWrapper::GetPlaneSize(size_t index) const {
  DCHECK_LT(index, planes_.size());
  return planes_[index].size;
}

uint32_t BoWrapper::GetBoHandle() const {
  NOTREACHED() << "Each buffer object implementation must implement this";
  return 0;
}

}  // namespace ui
