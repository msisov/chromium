// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_native_display_delegate.h"

#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

X11NativeDisplayDelegate::X11NativeDisplayDelegate() = default;

X11NativeDisplayDelegate::~X11NativeDisplayDelegate() = default;

void X11NativeDisplayDelegate::Initialize() {
  // This shouldn't be called twice.
  DCHECK(!current_snapshot_);

  XDisplay* display = gfx::GetXDisplay();
  Screen* screen = DefaultScreenOfDisplay(display);
  current_snapshot_.reset(new display::DisplaySnapshot(
      XScreenNumberOfScreen(screen), gfx::Point(0, 0),
      gfx::Size(WidthOfScreen(screen), HeightOfScreen(screen)),
      display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE, false,
      false, false, gfx::ColorSpace(), "", base::FilePath(),
      display::DisplaySnapshot::DisplayModeList(), std::vector<uint8_t>(),
      nullptr, nullptr, 0, 0, gfx::Size()));
  const int default_refresh = 60;
  current_mode_.reset(new display::DisplayMode(
      gfx::Size(WidthOfScreen(screen), HeightOfScreen(screen)), false,
      default_refresh));
  current_snapshot_->set_current_mode(current_mode_.get());

  for (display::NativeDisplayObserver& observer : observers_)
    observer.OnConfigurationChanged();
}

void X11NativeDisplayDelegate::TakeDisplayControl(
    display::DisplayControlCallback callback) {
  NOTREACHED();
}

void X11NativeDisplayDelegate::RelinquishDisplayControl(
    display::DisplayControlCallback callback) {
  NOTREACHED();
}

void X11NativeDisplayDelegate::GetDisplays(
    display::GetDisplaysCallback callback) {
  // TODO(msisov): Add support for multiple displays and dynamic display data
  // change.
  std::vector<display::DisplaySnapshot*> snapshot;
  snapshot.push_back(current_snapshot_.get());
  std::move(callback).Run(snapshot);
}

void X11NativeDisplayDelegate::Configure(const display::DisplaySnapshot& output,
                                         const display::DisplayMode* mode,
                                         const gfx::Point& origin,
                                         display::ConfigureCallback callback) {
  NOTREACHED();
}

void X11NativeDisplayDelegate::GetHDCPState(
    const display::DisplaySnapshot& output,
    display::GetHDCPStateCallback callback) {
  NOTREACHED();
}

void X11NativeDisplayDelegate::SetHDCPState(
    const display::DisplaySnapshot& output,
    display::HDCPState state,
    display::SetHDCPStateCallback callback) {
  NOTREACHED();
}

bool X11NativeDisplayDelegate::SetColorMatrix(int64_t display_id,
                    const std::vector<float>& color_matrix) {
  NOTREACHED();
  return false;
}

bool X11NativeDisplayDelegate::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  NOTREACHED();
  return false;
}

void X11NativeDisplayDelegate::AddObserver(
    display::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void X11NativeDisplayDelegate::RemoveObserver(
    display::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::FakeDisplayController*
X11NativeDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

}  // namespace ui
