// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/events/platform/platform_event_source.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/clipboard/mock_clipboard_delegate.h"

ui::ClipboardDelegate* g_clipboard_delegate_ = nullptr;
#endif

namespace ui {

struct PlatformClipboardTraits {
#if defined(USE_AURA)
  static std::unique_ptr<PlatformEventSource> GetEventSource() {
    return PlatformEventSource::CreateDefault();
  }
#endif

  static Clipboard* Create() {
    auto clipboard = Clipboard::GetForCurrentThread();
#if defined(USE_OZONE)
    DCHECK(!g_clipboard_delegate_);
    g_clipboard_delegate_ = new MockClipboardDelegate();
    clipboard->SetDelegate(g_clipboard_delegate_);
#endif
    return clipboard;
  }

  static void Destroy(Clipboard* clipboard) {
    ASSERT_EQ(Clipboard::GetForCurrentThread(), clipboard);
    Clipboard::DestroyClipboardForCurrentThread();
  }
};

typedef PlatformClipboardTraits TypesToTest;

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
