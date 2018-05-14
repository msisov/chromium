// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gbm_device_base.h"

#include <gbm.h>

namespace ui {

GbmDeviceBase::GbmDeviceBase() {}

GbmDeviceBase::~GbmDeviceBase() {
  if (device_)
    gbm_device_destroy(device_);
}

bool GbmDeviceBase::InitializeGbmDevice(int fd) {
  device_ = gbm_create_device(fd);
  if (!device_)
    return false;
  return true;
}

}  // namespace ui
