// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_DRM_DRM_RENDER_NODE_HANDLE_
#define UI_OZONE_PLATFORM_WAYLAND_DRM_DRM_RENDER_NODE_HANDLE_

#include "base/macros.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"

namespace ui {

class DrmRenderNodeHandle {
 public: 
  DrmRenderNodeHandle();
  ~DrmRenderNodeHandle();
 
  bool Initialize(const base::FilePath& path);

  base::ScopedFD PassFD();

 private: 
  base::ScopedFD drm_fd_;
  
 DISALLOW_COPY_AND_ASSIGN(DrmRenderNodeHandle);
};

} // namespace ui

#endif // UI_OZONE_PLATFORM_WAYLAND_DRM_DRM_RENDER_NODE_HANDLE_
