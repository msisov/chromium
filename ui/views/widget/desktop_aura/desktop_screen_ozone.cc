// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace views {

DesktopScreenOzone::DesktopScreenOzone() {
  ui::OzonePlatform::GetInstance()->QueryHostDisplaysData(base::Bind(
      &DesktopScreenOzone::OnHostDisplaysReady, base::Unretained(this)));
}

DesktopScreenOzone::~DesktopScreenOzone() = default;

void DesktopScreenOzone::OnHostDisplaysReady(
    const std::vector<gfx::Size>& dimensions) {
  float device_scale_factor = 1.f;
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();

  gfx::Size scaled_size =
      gfx::ConvertSizeToDIP(device_scale_factor, dimensions[0]);

  display::Display display(next_display_id_++);
  display.set_bounds(gfx::Rect(scaled_size));
  display.set_work_area(display.bounds());
  display.set_device_scale_factor(device_scale_factor);

  ProcessDisplayChanged(display, true /* is_primary */);

  // TODO(tonikitoo, msisov): Before calling out to ScreenManagerDelegate check
  // if more than one host display is available.
  // delegate_->OnHostDisplaysReady();
}

//////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  LOG(ERROR) << __PRETTY_FUNCTION__;
  return new DesktopScreenOzone;
}

}  // namespace views
