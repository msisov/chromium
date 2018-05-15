// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_NATIVE_DISPLAY_DELEGATE_H_
#define UI_OZONE_PLATFORM_X11_X11_NATIVE_DISPLAY_DELEGATE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/types/display_snapshot.h"

namespace ui {

class X11NativeDisplayDelegate : public display::NativeDisplayDelegate {
 public:
  X11NativeDisplayDelegate();
  ~X11NativeDisplayDelegate() override;

  // display::NativeDisplayDelegate overrides:
  void Initialize() override;
  void TakeDisplayControl(display::DisplayControlCallback callback) override;
  void RelinquishDisplayControl(
      display::DisplayControlCallback callback) override;
  void GetDisplays(display::GetDisplaysCallback callback) override;
  void Configure(const display::DisplaySnapshot& output,
                 const display::DisplayMode* mode,
                 const gfx::Point& origin,
                 display::ConfigureCallback callback) override;
  void GetHDCPState(const display::DisplaySnapshot& output,
                    display::GetHDCPStateCallback callback) override;
  void SetHDCPState(const display::DisplaySnapshot& output,
                    display::HDCPState state,
                    display::SetHDCPStateCallback callback) override;
  bool SetColorMatrix(int64_t display_id,
                      const std::vector<float>& color_matrix) override;
  bool SetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut) override;
  void AddObserver(display::NativeDisplayObserver* observer) override;
  void RemoveObserver(display::NativeDisplayObserver* observer) override;
  display::FakeDisplayController* GetFakeDisplayController() override;

 private:
  std::unique_ptr<display::DisplaySnapshot> current_snapshot_;
  std::unique_ptr<display::DisplayMode> current_mode_;
 
  base::ObserverList<display::NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(X11NativeDisplayDelegate);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_NATIVE_DISPLAY_DELEGATE_H_
