// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server.h>

#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class TestInputMethodContextDelegate
    : public ui::mojom::LinuxInputMethodContextDelegate {
 public:
  TestInputMethodContextDelegate(
      ui::mojom::LinuxInputMethodContextDelegateRequest request)
      : binding_(this, std::move(request)) {}

  void OnCommit(const base::string16& text) override {
    was_on_commit_called_ = true;
  }
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {
    was_on_preedit_changed_called_ = true;
  }
  void OnPreeditEnd() override {}
  void OnPreeditStart() override {}

  bool was_on_commit_called() { return was_on_commit_called_; }

  bool was_on_preedit_changed_called() {
    return was_on_preedit_changed_called_;
  }

 private:
  mojo::Binding<ui::mojom::LinuxInputMethodContextDelegate> binding_;

  bool was_on_commit_called_ = false;
  bool was_on_preedit_changed_called_ = false;
};

class WaylandInputMethodContextTest : public WaylandTest {
 public:
  WaylandInputMethodContextTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    setenv("ENABLE_WAYLAND_IME", "true", 0);

    input_method_context_delegate_request =
        mojo::MakeRequest(&input_method_context_delegate_ptr);
    input_method_context_delegate =
        std::make_unique<TestInputMethodContextDelegate>(
            std::move(input_method_context_delegate_request));

    input_method_context =
        std::make_unique<WaylandInputMethodContext>(connection_.get());
    input_method_context->Initialize(
        std::move(input_method_context_delegate_ptr), false);
    connection_->ScheduleFlush();

    Sync();

    zwp_text_input = server_.text_input_manager_v1()->text_input.get();
    window_->set_keyboard_focus(true);

    ASSERT_TRUE(connection_->text_input_manager_v1());
    ASSERT_TRUE(zwp_text_input);
  }

 protected:
  ui::mojom::LinuxInputMethodContextDelegatePtr
      input_method_context_delegate_ptr;
  ui::mojom::LinuxInputMethodContextDelegateRequest
      input_method_context_delegate_request;

  std::unique_ptr<TestInputMethodContextDelegate> input_method_context_delegate;
  std::unique_ptr<WaylandInputMethodContext> input_method_context;
  wl::MockZwpTextInput* zwp_text_input = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContextTest);
};

TEST_P(WaylandInputMethodContextTest, Focus) {
  EXPECT_CALL(*zwp_text_input, Activate(surface_->resource()));
  EXPECT_CALL(*zwp_text_input, ShowInputPanel());
  input_method_context->Focus();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, Blur) {
  EXPECT_CALL(*zwp_text_input, Deactivate());
  EXPECT_CALL(*zwp_text_input, HideInputPanel());
  input_method_context->Blur();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, Reset) {
  EXPECT_CALL(*zwp_text_input, Reset());
  input_method_context->Reset();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, SetCursorLocation) {
  EXPECT_CALL(*zwp_text_input, SetCursorRect(50, 0, 1, 1));
  input_method_context->SetCursorLocation(gfx::Rect(50, 0, 1, 1));
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, OnPreeditChanged) {
  zwp_text_input_v1_send_preedit_string(zwp_text_input->resource(), 0,
                                        "PreeditString", "");
  Sync();
  EXPECT_TRUE(input_method_context_delegate->was_on_preedit_changed_called());
}

TEST_P(WaylandInputMethodContextTest, OnCommit) {
  zwp_text_input_v1_send_commit_string(zwp_text_input->resource(), 0,
                                       "CommitString");
  Sync();
  EXPECT_TRUE(input_method_context_delegate->was_on_commit_called());
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandInputMethodContextTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandInputMethodContextTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
