// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GBM_DEVICE_BASE_H_
#define UI_OZONE_COMMON_GBM_DEVICE_BASE_H_

#include "base/files/file.h"
#include "base/macros.h"

struct gbm_device;

namespace ui {

class GbmDeviceBase {
 public:
  GbmDeviceBase();
  virtual ~GbmDeviceBase();

  gbm_device* device() const { return device_; }

  bool InitializeGbmDevice(int fd);

 private:
  gbm_device* device_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GbmDeviceBase);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GBM_DEVICE_BASE_H_
