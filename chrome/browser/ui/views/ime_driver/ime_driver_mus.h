// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "services/ui/public/interfaces/ime/ime.mojom.h"

#if defined(OS_LINUX) && defined(USE_OZONE) && !defined(OS_CHROMEOS)
namespace aura {
class LinuxInputMethodContextFactoryMus;
}
#endif

class IMEDriver : public ui::mojom::IMEDriver {
 public:
  IMEDriver();
  ~IMEDriver() override;

  // Instantiate the IME driver and register it to the UI service.
  static void Register();

 private:
  // ui::mojom::IMEDriver:
  void StartSession(ui::mojom::StartSessionDetailsPtr details) override;

#if defined(OS_LINUX) && defined(USE_OZONE) && !defined(OS_CHROMEOS)
  std::unique_ptr<aura::LinuxInputMethodContextFactoryMus>
      input_method_context_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(IMEDriver);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_IME_DRIVER_MUS_H_
