// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_GBM_BO_WRAPPER_H_
#define UI_OZONE_COMMON_LINUX_GBM_BO_WRAPPER_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/common/linux/bo_wrapper.h"

struct gbm_bo;

namespace gfx {
class Size;
struct NativePixmapPlane;
}

namespace ui {

// Generic gbm_bo wrapper for ozone backends.
class GbmBoWrapper : public BoWrapper {
 public:
  GbmBoWrapper(gbm_bo* bo,
               uint32_t format,
               uint32_t flags,
               uint64_t modifier,
               std::vector<base::ScopedFD> fds,
               const gfx::Size& size,
               std::vector<gfx::NativePixmapPlane> planes);
  ~GbmBoWrapper() override;

  gbm_bo* bo() const { return bo_; }

  // BoWrapper overrides:
  uint32_t GetBoHandle() const override;

 private:
  // Owned gbm buffer object.
  gbm_bo* bo_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GbmBoWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_GBM_BO_WRAPPER_H_
