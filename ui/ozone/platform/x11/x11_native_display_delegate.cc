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
  display_manager_ = std::make_unique<X11DisplayManagerOzone>();
  display_manager_->SetObserver(this);
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
  if (displays_ready_)
    display_manager_->GetDisplaysSnapshot(std::move(callback));
}

void X11NativeDisplayDelegate::Configure(const display::DisplaySnapshot& output,
                                         const display::DisplayMode* mode,
                                         const gfx::Point& origin,
                                         display::ConfigureCallback callback) {
  NOTIMPLEMENTED();

  // It should call |callback| after configuration.
  // Even if we don't have implementation, it calls |callback| to finish the
  // logic. otherwise, several tests from views_mus_unittests don't work.
  std::move(callback).Run(true);
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

void X11NativeDisplayDelegate::OnOutputReadyForUse() {
  if (!displays_ready_)
    displays_ready_ = true;

  for (display::NativeDisplayObserver& observer : observers_)
    observer.OnConfigurationChanged();
}
}  // namespace ui
