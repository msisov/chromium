// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/wayland_screen.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

namespace {

constexpr uint32_t kNumberOfDisplays = 1;
constexpr uint32_t kOutputWidth = 1024;
constexpr uint32_t kOutputHeight = 768;

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() {}
  ~TestDisplayObserver() override {}

  display::Display GetDisplay() { return std::move(display_); }
  uint32_t GetAndClearChangedMetrics() {
    uint32_t changed_metrics = changed_metrics_;
    changed_metrics_ = 0;
    return changed_metrics;
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    display_ = new_display;
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    display_ = old_display;
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    changed_metrics_ = changed_metrics;
    display_ = display;
  }

 private:
  uint32_t changed_metrics_ = 0;
  display::Display display_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

}  // namespace

class WaylandScreenTest : public WaylandTest {
 public:
  WaylandScreenTest() {}
  ~WaylandScreenTest() override {}

  void SetUp() override {
    output_ = server_.output();
    output_->SetRect(gfx::Rect(0, 0, kOutputWidth, kOutputHeight));

    WaylandTest::SetUp();

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);

    EXPECT_TRUE(output_manager_->IsPrimaryOutputReady());
    platform_screen_ = output_manager_->CreateWaylandScreen(connection_.get());
  }

 protected:
  void ValidateTheDisplayForWidget(gfx::AcceleratedWidget widget,
                                   int64_t expected_display_id,
                                   const gfx::Point& expected_origin) {
    display::Display display_for_widget =
        platform_screen_->GetDisplayForAcceleratedWidget(widget);
    EXPECT_EQ(display_for_widget.id(), expected_display_id);
    EXPECT_EQ(display_for_widget.bounds().origin(), expected_origin);
  }

  wl::MockOutput* CreateOutputWithRectAndSync(const gfx::Rect& rect) {
    wl::MockOutput* output = server_.CreateAndInitializeOutput();
    if (!rect.IsEmpty())
      output->SetRect(rect);

    Sync();

    // The geometry changes are automatically sent only after the client is
    // bound to the output. Sync one more time to enqueue the geometry events.
    Sync();

    return output;
  }

  void SendGeometryAndModeChangesAndSync(const gfx::Rect& new_rect,
                                         wl_resource* resource) {
    wl_output_send_geometry(output_->resource(), new_rect.x(), new_rect.y(),
                            0 /* physical_width */, 0 /* physical_height */,
                            0 /* subpixel */, "unkown_make", "unknown_model",
                            0 /* transform */);
    wl_output_send_mode(output_->resource(), WL_OUTPUT_MODE_CURRENT,
                        new_rect.width(), new_rect.height(), 0 /* refresh */);

    Sync();
  }

  std::unique_ptr<WaylandWindow> CreateWaylandWindowAndSync(
      const gfx::Rect& bounds,
      PlatformWindowType window_type,
      gfx::AcceleratedWidget parent_widget,
      MockPlatformWindowDelegate* delegate) {
    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_))
        .WillOnce(SaveArg<0>(&widget));

    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = window_type;
    properties.parent_widget = parent_widget;
    EXPECT_TRUE(window->Initialize(std::move(properties)));
    EXPECT_NE(widget, gfx::kNullAcceleratedWidget);

    Sync();

    return window;
  }

  wl::MockOutput* output_ = nullptr;
  WaylandOutputManager* output_manager_ = nullptr;

  std::unique_ptr<WaylandScreen> platform_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandScreenTest);
};

// Tests whether a primary output has been initialized before PlatformScreen is
// created.
TEST_P(WaylandScreenTest, OutputBaseTest) {
  // IsPrimaryOutputReady and PlatformScreen creation is done in the
  // initialization part of the tests.

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen_->GetAllDisplays();
  EXPECT_EQ(all_displays.size(), kNumberOfDisplays);

  // Ensure the size property of the primary display.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, kOutputWidth, kOutputHeight));
}

TEST_P(WaylandScreenTest, MultipleOutputsAddedAndRemoved) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // Add a second display.
  const gfx::Rect rect(gfx::Point(output_->GetRect().width(), 0),
                       output_->GetRect().size());
  CreateOutputWithRectAndSync(rect);

  Sync();

  // Ensure that second display is not a primary one and have a different id.
  int64_t added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Remove the second output.
  output_manager_->RemoveWaylandOutput(added_display_id);

  Sync();

  // Ensure that removed display has correct id.
  int64_t removed_display_id = observer.GetDisplay().id();
  EXPECT_EQ(added_display_id, removed_display_id);

  // Create another display again.
  CreateOutputWithRectAndSync(rect);

  // The newly added display is not a primary yet.
  added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Make sure the geometry changes are sent by syncing one more time again.
  Sync();

  int64_t old_primary_display_id = platform_screen_->GetPrimaryDisplay().id();
  output_manager_->RemoveWaylandOutput(old_primary_display_id);

  // Ensure that previously added display is now a primary one.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().id(), added_display_id);
  // Ensure that the removed display was the one, which was a primary display.
  EXPECT_EQ(observer.GetDisplay().id(), old_primary_display_id);
}

TEST_P(WaylandScreenTest, OutputPropertyChanges) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const gfx::Rect new_rect(0, 0, 800, 600);
  SendGeometryAndModeChangesAndSync(new_rect, output_->resource());

  uint32_t changed_values = 0;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().bounds(), new_rect);

  const float new_scale_value = 2.0f;
  wl_output_send_scale(output_->resource(), new_scale_value);

  Sync();

  changed_values = 0;
  changed_values |=
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().device_scale_factor(), new_scale_value);
}

TEST_P(WaylandScreenTest, GetDisplayForAcceleratedWidget) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // Add a test window and get an accelerated widget for it.
  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> window = CreateWaylandWindowAndSync(
      gfx::Rect(0, 0, 640, 480), PlatformWindowType::kWindow,
      gfx::kNullAcceleratedWidget, &delegate);

  gfx::AcceleratedWidget widget = window->GetWidget();

  // There must be a primary display used if the window has not entered a
  // surface yet (very unlikely in the production).
  int64_t expected_display_id = platform_screen_->GetPrimaryDisplay().id();
  gfx::Point expected_origin =
      platform_screen_->GetPrimaryDisplay().bounds().origin();
  ValidateTheDisplayForWidget(widget, expected_display_id, expected_origin);

  Sync();

  // Get the surface of that window.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(widget);
  ASSERT_TRUE(surface);

  // Now, send enter event for the surface, which was created before.
  wl_surface_send_enter(surface->resource(), output_->resource());

  Sync();

  // The id of the entered display must still correspond to the primary output.
  ValidateTheDisplayForWidget(widget, expected_display_id, expected_origin);

  // Create one more output. Make sure two output are placed next to each other.
  const gfx::Rect new_rect(gfx::Point(output_->GetRect().width(), 0),
                           output_->GetRect().size());
  wl::MockOutput* output2 = CreateOutputWithRectAndSync(new_rect);

  Sync();

  const display::Display display2 = observer.GetDisplay();

  // Enter the second output now.
  wl_surface_send_enter(surface->resource(), output2->resource());

  Sync();

  // The id of the entered display must still correspond to the primary output.
  ValidateTheDisplayForWidget(widget, expected_display_id, expected_origin);

  // Leave the first output.
  wl_surface_send_leave(surface->resource(), output_->resource());

  Sync();

  // The id and the origin of the display for widget must corresponds to the
  // second display now.
  expected_display_id = display2.id();
  expected_origin = display2.bounds().origin();
  ValidateTheDisplayForWidget(widget, expected_display_id, expected_origin);
}

TEST_P(WaylandScreenTest, GetAcceleratedWidgetAtScreenPoint) {
  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowAndSync(
      gfx::Rect(window_->GetBounds().width() - 10,
                window_->GetBounds().height() - 10, 100, 100),
      PlatformWindowType::kPopup, window_->GetWidget(), &delegate);

  // If there is no focused window (focus is set whenever a pointer enters any
  // of the windows), there must be kNullAcceleratedWidget returned. There is no
  // real way to determine what window is located on a certain screen point in
  // Wayland.
  gfx::AcceleratedWidget widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  // Set a focus to the main window.
  window_->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Imagine the mouse enters a menu window, which is located on top of the main
  // window, and gathers focus.
  window_->set_pointer_focus(false);
  menu_window->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(
          menu_window->GetBounds().x() + 1, menu_window->GetBounds().y() + 1));
  EXPECT_EQ(widget_at_screen_point, menu_window->GetWidget());

  // Despite the menu window being focused, the accelerated widget at origin
  // must be the parent widget.
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(0, 0));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  menu_window->set_pointer_focus(false);
}

TEST_P(WaylandScreenTest, DefaultDisplayForNonExistingWidget) {
  display::Display default_display =
      platform_screen_->GetDisplayForAcceleratedWidget(
          gfx::kNullAcceleratedWidget);
  EXPECT_EQ(default_display.id(), platform_screen_->GetPrimaryDisplay().id());
}

TEST_P(WaylandScreenTest, GetDisplayNearestPoint) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const int64_t first_display_id = platform_screen_->GetPrimaryDisplay().id();

  // Prepare the first output.
  const gfx::Rect new_rect(gfx::Rect(0, 0, 640, 480));
  SendGeometryAndModeChangesAndSync(new_rect, output_->resource());

  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds().ToString(),
            new_rect.ToString());

  const gfx::Rect output2_new_rect(640, 0, 1024, 768);
  CreateOutputWithRectAndSync(output2_new_rect);

  const display::Display second_display = observer.GetDisplay();
  const int64_t second_display_id = second_display.id();
  EXPECT_EQ(second_display.bounds().ToString(), output2_new_rect.ToString());

  EXPECT_EQ(first_display_id,
            platform_screen_->GetDisplayNearestPoint(gfx::Point(630, 10)).id());
  EXPECT_EQ(second_display_id,
            platform_screen_->GetDisplayNearestPoint(gfx::Point(650, 10)).id());
  EXPECT_EQ(first_display_id,
            platform_screen_->GetDisplayNearestPoint(gfx::Point(10, 10)).id());
  EXPECT_EQ(
      second_display_id,
      platform_screen_->GetDisplayNearestPoint(gfx::Point(10000, 10000)).id());
  EXPECT_EQ(
      first_display_id,
      platform_screen_->GetDisplayNearestPoint(gfx::Point(639, -10)).id());
  EXPECT_EQ(
      second_display_id,
      platform_screen_->GetDisplayNearestPoint(gfx::Point(641, -20)).id());
  EXPECT_EQ(
      second_display_id,
      platform_screen_->GetDisplayNearestPoint(gfx::Point(600, 760)).id());
  EXPECT_EQ(
      first_display_id,
      platform_screen_->GetDisplayNearestPoint(gfx::Point(-1000, 760)).id());
}

TEST_P(WaylandScreenTest, GetDisplayMatchingBasic) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // Prepare the first output.
  const gfx::Rect new_rect(gfx::Rect(0, 0, 640, 480));
  SendGeometryAndModeChangesAndSync(new_rect, output_->resource());

  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds().ToString(),
            new_rect.ToString());

  const gfx::Rect output2_new_rect(640, 0, 1024, 768);
  CreateOutputWithRectAndSync(output2_new_rect);

  const display::Display second_display = observer.GetDisplay();
  const int64_t second_display_id = second_display.id();
  EXPECT_EQ(second_display.bounds().ToString(), output2_new_rect.ToString());

  EXPECT_EQ(
      second_display_id,
      platform_screen_->GetDisplayMatching(gfx::Rect(700, 20, 100, 100)).id());
}

TEST_P(WaylandScreenTest, GetDisplayMatchingOverlap) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // Prepare the first output.
  const gfx::Rect new_rect(gfx::Rect(0, 0, 640, 480));
  SendGeometryAndModeChangesAndSync(new_rect, output_->resource());

  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds().ToString(),
            new_rect.ToString());

  const gfx::Rect output2_new_rect(640, 0, 1024, 768);
  CreateOutputWithRectAndSync(output2_new_rect);

  const display::Display second_display = observer.GetDisplay();
  const int64_t second_display_id = second_display.id();
  EXPECT_EQ(second_display.bounds().ToString(), output2_new_rect.ToString());

  EXPECT_EQ(
      second_display_id,
      platform_screen_->GetDisplayMatching(gfx::Rect(630, 20, 100, 100)).id());
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
