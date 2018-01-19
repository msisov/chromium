// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/ime_driver/input_method_bridge_linux.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

enum class CompositionEventType {
  SET,
  CONFIRM,
  CLEAR,
  INSERT_TEXT,
  INSERT_CHAR
};

struct CompositionEvent {
  CompositionEventType type;
  base::string16 text_data;
  base::char16 char_data;
};

class TestInputMethodContext : public ui::LinuxInputMethodContext {
 public:
  TestInputMethodContext() {}

  // Overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override {
    if (key_event.key_code() == ui::VKEY_A) {
      return true;
    } else {
      return false;
    }
  }
  void Reset() override {}
  void Focus() override {}
  void Blur() override {}
  void SetCursorLocation(const gfx::Rect& rect) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContext);
};

class TestInputMethodContextFactory
    : public ui::LinuxInputMethodContextFactory {
 public:
  TestInputMethodContextFactory() {}

  // LinuxInputMethodContextFactory:
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override {
    return std::make_unique<TestInputMethodContext>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContextFactory);
};

class TestTextInputClient : public ui::mojom::TextInputClient {
 public:
  explicit TestTextInputClient(ui::mojom::TextInputClientRequest request)
      : binding_(this, std::move(request)) {}

  CompositionEvent WaitUntilCompositionEvent() {
    if (!receieved_event_.has_value()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    CompositionEvent result = receieved_event_.value();
    receieved_event_.reset();
    return result;
  }

 private:
  void SetCompositionText(const ui::CompositionText& composition) override {
    CompositionEvent ev = {CompositionEventType::SET, composition.text, 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void ConfirmCompositionText() override {
    CompositionEvent ev = {CompositionEventType::CONFIRM, base::string16(), 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void ClearCompositionText() override {
    CompositionEvent ev = {CompositionEventType::CLEAR, base::string16(), 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void InsertText(const base::string16& text) override {
    CompositionEvent ev = {CompositionEventType::INSERT_TEXT, text, 0};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void InsertChar(std::unique_ptr<ui::Event> event) override {
    ASSERT_TRUE(event->IsKeyEvent());
    CompositionEvent ev = {CompositionEventType::INSERT_CHAR, base::string16(),
                           event->AsKeyEvent()->GetCharacter()};
    receieved_event_ = ev;
    if (run_loop_)
      run_loop_->Quit();
  }
  void DispatchKeyEventPostIME(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventPostIMECallback callback) override {
    fprintf(stderr, "DispatchKeyEventPostIME!!!!\r\n");
    std::move(callback).Run(false);
  }

  mojo::Binding<ui::mojom::TextInputClient> binding_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<CompositionEvent> receieved_event_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class InputMethodBridgeLinuxTest : public testing::Test {
 public:
  InputMethodBridgeLinuxTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~InputMethodBridgeLinuxTest() override {}

  void SetUp() override {
    ui::IMEBridge::Initialize();
    input_method_context_factory_ =
        std::make_unique<TestInputMethodContextFactory>();
    ui::LinuxInputMethodContextFactory::SetInstance(
        input_method_context_factory_.get());
    ui::mojom::TextInputClientPtr client_ptr;
    client_ = std::make_unique<TestTextInputClient>(MakeRequest(&client_ptr));
    input_method_ = std::make_unique<InputMethodBridgeLinux>(
        std::make_unique<RemoteTextInputClient>(
            std::move(client_ptr), ui::TEXT_INPUT_TYPE_TEXT,
            ui::TEXT_INPUT_MODE_DEFAULT, base::i18n::LEFT_TO_RIGHT, 0,
            gfx::Rect()));
  }

  void TearDown() override {
    ui::LinuxInputMethodContextFactory::SetInstance(nullptr);
  }

  bool ProcessKeyEvent(std::unique_ptr<ui::Event> event) {
    handled_.reset();

    input_method_->ProcessKeyEvent(
        std::move(event),
        base::Bind(&InputMethodBridgeLinuxTest::ProcessKeyEventCallback,
                   base::Unretained(this)));

    if (!handled_.has_value()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }

    return handled_.value();
  }

  std::unique_ptr<ui::Event> UnicodeKeyPress(ui::KeyboardCode vkey,
                                             ui::DomCode code,
                                             int flags,
                                             base::char16 character) const {
    return std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, vkey, code, flags,
                                          ui::DomKey::FromCharacter(character),
                                          ui::EventTimeForNow());
  }

  std::unique_ptr<ui::Event> UnicodeKeyRelease(ui::KeyboardCode vkey,
                                               ui::DomCode code,
                                               int flags,
                                               base::char16 character) const {
    return std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, vkey, code, flags,
                                          ui::DomKey::FromCharacter(character),
                                          ui::EventTimeForNow());
  }

 protected:
  void ProcessKeyEventCallback(bool handled) {
    handled_ = handled;
    if (run_loop_)
      run_loop_->Quit();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestInputMethodContextFactory> input_method_context_factory_;
  std::unique_ptr<TestTextInputClient> client_;
  std::unique_ptr<InputMethodBridgeLinux> input_method_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<bool> handled_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridgeLinuxTest);
};

// Test key press A that is handled by context and thus it
// InputMethodAuraLinux doesn't stop event propagation if event is handled
// by context but there is no result text or composition
TEST_F(InputMethodBridgeLinuxTest, KeyPressHandledByContext) {
  EXPECT_FALSE(ProcessKeyEvent(
      UnicodeKeyPress(ui::VKEY_A, ui::DomCode::US_A, ui::EF_NONE, 'A')));
}

// Test key press B that is not handled by context.
// If a key event was not filtered by context then it means the key event
// didn't generate any result text but still it may generate valid character.
// Valid character is sent to client and InputMethodAuraLinux should stop
// propagation in such cases
TEST_F(InputMethodBridgeLinuxTest, KeyPressNotHandledByContext) {
  EXPECT_TRUE(ProcessKeyEvent(
      UnicodeKeyPress(ui::VKEY_B, ui::DomCode::US_B, ui::EF_NONE, 'B')));
}
