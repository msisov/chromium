// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_bo_wrapper.h"

#include <gbm.h>

#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

GbmBoWrapper::GbmBoWrapper(gbm_bo* bo,
                     uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     std::vector<base::ScopedFD> fds,
                     const gfx::Size& size,
                     std::vector<gfx::NativePixmapPlane> planes)
  :   BoWrapper(format, flags, modifier, std::move(fds), size, std::move(planes)),
      bo_(bo) {}

GbmBoWrapper::~GbmBoWrapper() {
  if (bo_)
    gbm_bo_destroy(bo_);
}

uint32_t GbmBoWrapper::GetBoHandle() const {
  return bo_ ? gbm_bo_get_handle(bo_).u32 : 0;
}

}  // namespace ui
