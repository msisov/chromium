// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/ime/linux_input_method_context.mojom.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/linux_input_method_context_mus_test_api.h"
#include "ui/events/event.h"

namespace aura {

class TestInputMethodContext : public ui::mojom::LinuxInputMethodContext {
 public:
  TestInputMethodContext(ui::mojom::LinuxInputMethodContextRequest request)
      : binding_(this, std::move(request)) {}

  void Initialize(ui::mojom::LinuxInputMethodContextDelegatePtr delegate,
                  bool is_simple) override {}
  void DispatchKeyEvent(std::unique_ptr<ui::Event> key_event,
                        DispatchKeyEventCallback callback) override {
    was_dispatch_key_event_called_ = true;
    const bool handled = false;
    std::move(callback).Run(handled);
  }
  void SetCursorLocation(const gfx::Rect& rect) override {
    was_set_cursor_location_called_ = true;
  }
  void Reset() override { was_reset_called_ = true; }
  void Focus() override { was_focus_called_ = true; }
  void Blur() override { was_blur_called_ = true; }

  bool was_dispatch_key_event_called() {
    return was_dispatch_key_event_called_;
  }

  bool was_set_cursor_location_called() {
    return was_set_cursor_location_called_;
  }

  bool was_reset_called() { return was_reset_called_; }

  bool was_focus_called() { return was_focus_called_; }

  bool was_blur_called() { return was_blur_called_; }

 private:
  mojo::Binding<ui::mojom::LinuxInputMethodContext> binding_;

  bool was_dispatch_key_event_called_ = false;
  bool was_set_cursor_location_called_ = false;
  bool was_reset_called_ = false;
  bool was_focus_called_ = false;
  bool was_blur_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContext);
};

class LinuxInputMethodContextMusTest : public test::AuraTestBaseMus {
 public:
  LinuxInputMethodContextMusTest() {}
  ~LinuxInputMethodContextMusTest() override {}

  // AuraTestBase:
  void SetUp() override {
    AuraTestBaseMus::SetUp();

    input_method_context_request_ =
        mojo::MakeRequest(&input_method_context_ptr_);
    input_method_context_ = std::make_unique<TestInputMethodContext>(
        std::move(input_method_context_request_));

    input_method_context_mus_ =
        std::make_unique<LinuxInputMethodContextMus>(nullptr, true);

    LinuxInputMethodContextMusTestApi::SetInputMethodContext(
        input_method_context_mus_.get(), input_method_context_ptr_.get());
  }

 protected:
  ui::mojom::LinuxInputMethodContextPtr input_method_context_ptr_;
  ui::mojom::LinuxInputMethodContextRequest input_method_context_request_;

  std::unique_ptr<TestInputMethodContext> input_method_context_;
  std::unique_ptr<LinuxInputMethodContextMus> input_method_context_mus_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextMusTest);
};

class TestInputMethodContextDelegate
    : public ui::LinuxInputMethodContextDelegate {
 public:
  TestInputMethodContextDelegate() {}

  void OnCommit(const base::string16& text) override {
    was_on_commit_called_ = true;
  }
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {
    was_on_preedit_changed_called_ = true;
  }
  void OnPreeditEnd() override { was_on_preedit_end_called_ = true; }
  void OnPreeditStart() override { was_on_preedit_start_called_ = true; }
  bool was_on_commit_called() { return was_on_commit_called_; }
  bool was_on_preedit_changed_called() {
    return was_on_preedit_changed_called_;
  }
  bool was_on_preedit_end_called() { return was_on_preedit_end_called_; }
  bool was_on_preedit_start_called() { return was_on_preedit_start_called_; }

 private:
  bool was_on_commit_called_ = false;
  bool was_on_preedit_changed_called_ = false;
  bool was_on_preedit_end_called_ = false;
  bool was_on_preedit_start_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContextDelegate);
};

class TestInputMethodContextForDelegate
    : public ui::mojom::LinuxInputMethodContext {
 public:
  TestInputMethodContextForDelegate(
      ui::mojom::LinuxInputMethodContextRequest request)
      : binding_(this, std::move(request)) {}

  void Initialize(ui::mojom::LinuxInputMethodContextDelegatePtr delegate,
                  bool is_simple) override {
    delegate_ = std::move(delegate);
  }

  void DispatchKeyEvent(std::unique_ptr<ui::Event> key_event,
                        DispatchKeyEventCallback callback) override {
    const bool handled = false;
    std::move(callback).Run(handled);
  }
  void SetCursorLocation(const gfx::Rect& rect) override {}
  void Reset() override {}
  void Focus() override {}
  void Blur() override {}
  void CallOnCommit() { delegate_->OnCommit(base::string16()); }
  void CallOnPreeditChanged() {
    ui::CompositionText composition_text;
    delegate_->OnPreeditChanged(composition_text);
  }
  void CallOnPreeditEnd() { delegate_->OnPreeditEnd(); }
  void CallOnPreeditStart() { delegate_->OnPreeditStart(); }

 private:
  mojo::Binding<ui::mojom::LinuxInputMethodContext> binding_;

  ui::mojom::LinuxInputMethodContextDelegatePtr delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContextForDelegate);
};

class LinuxInputMethodContextDelegateMusTest : public test::AuraTestBaseMus {
 public:
  LinuxInputMethodContextDelegateMusTest() {}
  ~LinuxInputMethodContextDelegateMusTest() override {}

  // AuraTestBase:
  void SetUp() override {
    AuraTestBaseMus::SetUp();

    input_method_context_request_ =
        mojo::MakeRequest(&input_method_context_ptr_);
    input_method_context_ = std::make_unique<TestInputMethodContextForDelegate>(
        std::move(input_method_context_request_));

    input_method_context_delegate_ =
        std::make_unique<TestInputMethodContextDelegate>();
    input_method_context_mus_ = std::make_unique<LinuxInputMethodContextMus>(
        input_method_context_delegate_.get(), true);

    LinuxInputMethodContextMusTestApi::SetInputMethodContextAndDelegate(
        input_method_context_mus_.get(), input_method_context_ptr_.get());
    RunAllPendingInMessageLoop();
  }

 protected:
  ui::mojom::LinuxInputMethodContextPtr input_method_context_ptr_;
  ui::mojom::LinuxInputMethodContextRequest input_method_context_request_;

  std::unique_ptr<TestInputMethodContextDelegate>
      input_method_context_delegate_;
  std::unique_ptr<TestInputMethodContextForDelegate> input_method_context_;
  std::unique_ptr<LinuxInputMethodContextMus> input_method_context_mus_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxInputMethodContextDelegateMusTest);
};

void RunDispatchKeyEventCallback(bool* was_run, bool result) {
  *was_run = true;
}

TEST_F(LinuxInputMethodContextMusTest, DispatchKeyEvent) {
  ui::KeyEvent key_event('A', ui::VKEY_A, 0);
  input_method_context_mus_->DispatchKeyEvent(key_event);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_dispatch_key_event_called());
}

TEST_F(LinuxInputMethodContextMusTest, DispatchKeyEventCallback) {
  bool was_event_result_callback_run = false;
  std::unique_ptr<LinuxInputMethodContextMus::EventResultCallback> callback =
      std::make_unique<LinuxInputMethodContextMus::EventResultCallback>(
          base::Bind(&RunDispatchKeyEventCallback,
                     &was_event_result_callback_run));

  ui::KeyEvent key_event('A', ui::VKEY_A, 0);
  input_method_context_mus_->DispatchKeyEvent(key_event, std::move(callback));
  EXPECT_FALSE(was_event_result_callback_run);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_dispatch_key_event_called());
  EXPECT_TRUE(was_event_result_callback_run);
}

TEST_F(LinuxInputMethodContextMusTest, SetCursorLocation) {
  input_method_context_mus_->SetCursorLocation(gfx::Rect());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_set_cursor_location_called());
}

TEST_F(LinuxInputMethodContextMusTest, Reset) {
  input_method_context_mus_->Reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_reset_called());
}

TEST_F(LinuxInputMethodContextMusTest, Focus) {
  input_method_context_mus_->Focus();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_focus_called());
}

TEST_F(LinuxInputMethodContextMusTest, Blur) {
  input_method_context_mus_->Blur();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_->was_blur_called());
}

TEST_F(LinuxInputMethodContextDelegateMusTest, OnCommit) {
  input_method_context_->CallOnCommit();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
}

TEST_F(LinuxInputMethodContextDelegateMusTest, OnPreeditChanged) {
  input_method_context_->CallOnPreeditChanged();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
}

TEST_F(LinuxInputMethodContextDelegateMusTest, OnPreeditEnd) {
  input_method_context_->CallOnPreeditEnd();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_end_called());
}

TEST_F(LinuxInputMethodContextDelegateMusTest, OnPreeditStart) {
  input_method_context_->CallOnPreeditStart();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_start_called());
}

}  // namespace aura
