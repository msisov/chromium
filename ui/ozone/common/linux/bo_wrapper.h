// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_BO_WRAPPER_H_
#define UI_OZONE_COMMON_LINUX_BO_WRAPPER_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct NativePixmapPlane;
}

namespace ui {

// Generic gbm_bo wrapper for ozone backends.
class BoWrapper {
 public:
  BoWrapper(uint32_t format,
               uint32_t flags,
               uint64_t modifier,
               std::vector<base::ScopedFD> fds,
               const gfx::Size& size,
               std::vector<gfx::NativePixmapPlane> planes);
  virtual ~BoWrapper();

  uint32_t format() const { return format_; }
  uint64_t format_modifier() const { return format_modifier_; }
  uint32_t flags() const { return flags_; }
  size_t fd_count() const { return fds_.size(); }
  // TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
  // as the size would be queried directly from the underlying bo.
  gfx::Size size() const { return size_; }
  bool AreFdsValid() const;
  int GetFd(size_t plane) const;
  int GetStride(size_t plane) const;
  int GetOffset(size_t plane) const;
  size_t GetPlaneSize(size_t plane) const;
  
  // Returns a unique handle for this buffer object.
  virtual uint32_t GetBoHandle() const;

 private:
  uint64_t format_modifier_ = 0;
  uint32_t format_ = 0;
  uint32_t flags_ = 0;

  std::vector<base::ScopedFD> fds_;

  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  uint32_t id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BoWrapper);

};

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_BO_WRAPPER_H_
