// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_

#include "ui/base/dragdrop/os_exchange_data_provider_aurax11_base.h"
#include "ui/events/platform/platform_event_dispatcher.h"

namespace ui {

// OSExchangeData::Provider implementation for aura on linux.
class UI_BASE_EXPORT OSExchangeDataProviderAuraX11
    : public OSExchangeDataProviderAuraX11Base,
      public PlatformEventDispatcher {
 public:
  // |x_window| is the window the cursor is over, and |selection| is the set of
  // data being offered.
  OSExchangeDataProviderAuraX11(::Window x_window,
                                const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  OSExchangeDataProviderAuraX11();

  ~OSExchangeDataProviderAuraX11() override;

  std::unique_ptr<Provider> Clone() const override;
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents) override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderAuraX11);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
