// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/platform_display_default.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/public/interfaces/cursor/cursor_struct_traits.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/threaded_image_cursors.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/stub/stub_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#elif defined(USE_OZONE)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {
namespace ws {

PlatformDisplayDefault::PlatformDisplayDefault(
    ServerWindow* root_window,
    const display::ViewportMetrics& metrics,
    std::unique_ptr<ThreadedImageCursors> image_cursors)
    : root_window_(root_window),
      image_cursors_(std::move(image_cursors)),
      metrics_(metrics),
      widget_(gfx::kNullAcceleratedWidget) {}

PlatformDisplayDefault::~PlatformDisplayDefault() {
#if defined(OS_CHROMEOS)
  ui::CursorController::GetInstance()->ClearCursorConfigForWindow(
      GetAcceleratedWidget());
#endif

  // Don't notify the delegate from the destructor.
  delegate_ = nullptr;

  frame_generator_.reset();
  image_cursors_.reset();
  // Destroy the PlatformWindow early on as it may call us back during
  // destruction and we want to be in a known state. But destroy the surface
  // and ThreadedImageCursors first because they can still be using the platform
  // window.
  platform_window_.reset();
}

EventSink* PlatformDisplayDefault::GetEventSink() {
  return delegate_->GetEventSink();
}

void PlatformDisplayDefault::Init(PlatformDisplayDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  const gfx::Rect& bounds = metrics_.bounds_in_pixels;
  DCHECK(!bounds.size().IsEmpty());

  // Use StubWindow for virtual unified displays, like AshWindowTreeHostUnified.
  if (delegate_->GetDisplay().id() == display::kUnifiedDisplayId) {
    platform_window_ = std::make_unique<ui::StubWindow>(this, true, bounds);
  } else {
#if defined(OS_WIN)
    platform_window_ = std::make_unique<ui::WinWindow>(this, bounds);
#elif defined(USE_X11)
    platform_window_ = std::make_unique<ui::X11Window>(this, bounds);
#elif defined(OS_ANDROID)
    platform_window_ = std::make_unique<ui::PlatformWindowAndroid>(this);
    platform_window_->SetBounds(bounds);
#elif defined(USE_OZONE)
    platform_window_ =
        delegate_->GetOzonePlatform()->CreatePlatformWindow(this, bounds);
#else
    NOTREACHED() << "Unsupported platform";
#endif
  }

  if (image_cursors_) {
    image_cursors_->SetDisplay(delegate_->GetDisplay(),
                               metrics_.device_scale_factor);
  }
  // Show() must be called only after |image_cursors_| has created a
  // a cursor loader after display is sent to it. Otherwise, Show()
  // triggers a delegate call to OnBoundsChanged(), which end up to
  // changing cursor location for not existing cursor loader.
  //
  // On Linux desktop defer PlatformWindow::Show call to the client,
  // when reacting to 'ShowState' changes.
#if !(defined(OS_LINUX) && defined(USE_OZONE) && !defined(OS_CHROMEOS))
  // Show the platform window, unless it's the virtual unified display window.
  if (delegate_->GetDisplay().id() != display::kUnifiedDisplayId)
    platform_window_->Show();
#endif
}

void PlatformDisplayDefault::SetViewportSize(const gfx::Size& size) {
  platform_window_->SetBounds(gfx::Rect(size));
}

void PlatformDisplayDefault::SetTitle(const base::string16& title) {
  platform_window_->SetTitle(title);
}

void PlatformDisplayDefault::SetCapture() {
  platform_window_->SetCapture();
}

void PlatformDisplayDefault::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

void PlatformDisplayDefault::SetViewportBounds(const gfx::Rect& bounds) {
  platform_window_->SetBounds(bounds);
}

void PlatformDisplayDefault::SetWindowVisibility(bool visible) {
  if (visible)
    platform_window_->Show();
  else
    platform_window_->Hide();
}

void PlatformDisplayDefault::SetNativeWindowState(ui::mojom::ShowState state) {
  // TODO(tonikitoo,msisov): Rename to ShowWithState, as in WindowTreeHost{XXX}?
  platform_window_->Show();  

  if (applying_window_state_changes_)
    return;

  switch (state) {
    case (ui::mojom::ShowState::MINIMIZED):
      platform_window_->ReleaseCapture();
      platform_window_->Minimize();
      break;
    case (ui::mojom::ShowState::MAXIMIZED):
      platform_window_->Maximize();
      break;
    case (ui::mojom::ShowState::FULLSCREEN):
      platform_window_->ToggleFullscreen();
      break;
    case (ui::mojom::ShowState::NORMAL):
    case (ui::mojom::ShowState::DEFAULT):
      platform_window_->Restore();
      break;
    default:
      break;
  }
}

void PlatformDisplayDefault::GetWindowType(
    ui::PlatformWindowType* window_type) {
  DCHECK(window_type);
  switch (metrics_.window_type) {
    case ui::mojom::WindowType::MENU:
      *window_type = ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_MENU;
      break;
    case ui::mojom::WindowType::TOOLTIP:
      *window_type = ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_TOOLTIP;
      break;
    case ui::mojom::WindowType::POPUP:
      *window_type = ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_POPUP;
      break;
    case ui::mojom::WindowType::DRAG:
      *window_type = ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_DRAG;
      break;
    default:
      *window_type = ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW;
      break;
  }
}

void PlatformDisplayDefault::PerformNativeWindowDragOrResize(uint32_t hittest) {
  platform_window_->PerformNativeWindowDragOrResize(hittest);

  // Release capture explicitly set by EventDispatcher to ensure events are
  // passed properly after resizing/dragging is done.
  platform_window_->ReleaseCapture();
}

bool PlatformDisplayDefault::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  return platform_window_->RunMoveLoop(drag_offset);
}

void PlatformDisplayDefault::StopMoveLoop() {
  platform_window_->StopMoveLoop();
}

void PlatformDisplayDefault::SetCursor(const ui::CursorData& cursor_data) {
  if (!image_cursors_)
    return;

  image_cursors_->SetCursor(cursor_data, platform_window_.get());
}

void PlatformDisplayDefault::MoveCursorTo(
    const gfx::Point& window_pixel_location) {
  platform_window_->MoveCursorTo(window_pixel_location);
}

void PlatformDisplayDefault::SetCursorSize(const ui::CursorSize& cursor_size) {
  image_cursors_->SetCursorSize(cursor_size);
}

void PlatformDisplayDefault::ConfineCursorToBounds(
    const gfx::Rect& pixel_bounds) {
  platform_window_->ConfineCursorToBounds(pixel_bounds);
}

void PlatformDisplayDefault::UpdateTextInputState(
    const ui::TextInputState& state) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state);
}

void PlatformDisplayDefault::SetImeVisibility(bool visible) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

FrameGenerator* PlatformDisplayDefault::GetFrameGenerator() {
  return frame_generator_.get();
}

void PlatformDisplayDefault::UpdateViewportMetrics(
    const display::ViewportMetrics& metrics) {
  if (metrics_ == metrics)
    return;

  gfx::Rect bounds = platform_window_->GetBounds();
  if (bounds.size() != metrics.bounds_in_pixels.size()) {
    bounds.set_size(metrics.bounds_in_pixels.size());
    platform_window_->SetBounds(bounds);
  }

  metrics_ = metrics;
  if (frame_generator_) {
    frame_generator_->SetDeviceScaleFactor(metrics_.device_scale_factor);
    frame_generator_->OnWindowSizeChanged(metrics_.bounds_in_pixels.size());
  }
}

const display::ViewportMetrics& PlatformDisplayDefault::GetViewportMetrics() {
  return metrics_;
}

gfx::AcceleratedWidget PlatformDisplayDefault::GetAcceleratedWidget() const {
  return widget_;
}

void PlatformDisplayDefault::SetCursorConfig(
    display::Display::Rotation rotation,
    float scale) {
#if defined(OS_CHROMEOS)
  ui::CursorController::GetInstance()->SetCursorConfigForWindow(
      GetAcceleratedWidget(), rotation, scale);
#endif
}

void PlatformDisplayDefault::OnBoundsChanged(const gfx::Rect& new_bounds) {
  // We only care if the window size has changed.
  if (new_bounds == metrics_.bounds_in_pixels)
    return;

  metrics_.bounds_in_pixels = new_bounds;
  if (frame_generator_)
    frame_generator_->OnWindowSizeChanged(new_bounds.size());

  delegate_->OnBoundsChanged(new_bounds);
}

void PlatformDisplayDefault::OnDamageRect(const gfx::Rect& damaged_region) {
  if (frame_generator_)
    frame_generator_->OnWindowDamaged();
}

void PlatformDisplayDefault::DispatchEvent(ui::Event* event) {
  // Event location and event root location are the same, and both in pixels
  // and display coordinates.
  if (event->IsScrollEvent()) {
    // TODO(moshayedi): crbug.com/602859. Dispatch scroll events as
    // they are once we have proper support for scroll events.

    ui::PointerEvent pointer_event(
        ui::MouseWheelEvent(*event->AsScrollEvent()));
    SendEventToSink(&pointer_event);
  } else if (event->IsMouseEvent()) {
    ui::PointerEvent pointer_event(*event->AsMouseEvent());
    SendEventToSink(&pointer_event);
  } else if (event->IsTouchEvent()) {
    ui::PointerEvent pointer_event(*event->AsTouchEvent());
    SendEventToSink(&pointer_event);
  } else {
    SendEventToSink(event);
  }
}

void PlatformDisplayDefault::OnCloseRequest() {
#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  delegate_->OnCloseRequest();
#else
  const int64_t display_id = delegate_->GetDisplay().id();
  display::ScreenManager::GetInstance()->RequestCloseDisplay(display_id);
#endif
}

void PlatformDisplayDefault::OnClosed() {}

void PlatformDisplayDefault::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
  ui::mojom::ShowState state;
  switch (new_state) {
    case (ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED):
      state = ui::mojom::ShowState::MINIMIZED;
      break;
    case (ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED):
      state = ui::mojom::ShowState::MAXIMIZED;
      break;
    case (ui::PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL):
      state = ui::mojom::ShowState::NORMAL;
      break;
    // If the window is in the fullscreen mode, there is no need to notify the
    // client about the state as long as it has been the client who has changed
    // the state. Checked here explicitly on purpose.
    case (ui::PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN):
    default:
      // We don't support other states at the moment. Ignore them.
      return;
  }

  // OnWindowStateChanged() calls ServerWindow::SetProperty, which also calls to
  // PlatformDisplayDefault::SetNativeWindowState. This value ensures where are
  // not setting states of Ozone Windows twice.
  base::AutoReset<bool> ignore_ws_state(&applying_window_state_changes_, true);
  delegate_->OnWindowStateChanged(state);
}

void PlatformDisplayDefault::OnLostCapture() {
  delegate_->OnNativeCaptureLost();
}

void PlatformDisplayDefault::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_scale_factor) {
  // This will get called after Init() is called, either synchronously as part
  // of the Init() callstack or async after Init() has returned, depending on
  // the platform.
  DCHECK_EQ(gfx::kNullAcceleratedWidget, widget_);
  widget_ = widget;
  delegate_->OnAcceleratedWidgetAvailable();

  if (!delegate_->IsHostingViz())
    return;

  viz::mojom::CompositorFrameSinkAssociatedPtr compositor_frame_sink;
  viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client;
  viz::mojom::CompositorFrameSinkClientRequest
      compositor_frame_sink_client_request =
          mojo::MakeRequest(&compositor_frame_sink_client);

  // TODO(ccameron): |display_client| is not bound. This will need to
  // change to support macOS.
  viz::mojom::DisplayPrivateAssociatedPtr display_private;
  viz::mojom::DisplayClientPtr display_client;
  viz::mojom::DisplayClientRequest display_client_request =
      mojo::MakeRequest(&display_client);

  root_window_->CreateRootCompositorFrameSink(
      widget_, mojo::MakeRequest(&compositor_frame_sink),
      std::move(compositor_frame_sink_client),
      mojo::MakeRequest(&display_private), std::move(display_client));

  display_private->SetDisplayVisible(true);
  frame_generator_ = std::make_unique<FrameGenerator>();
  auto frame_sink_client_binding =
      std::make_unique<CompositorFrameSinkClientBinding>(
          frame_generator_.get(),
          std::move(compositor_frame_sink_client_request),
          std::move(compositor_frame_sink), std::move(display_private));
  frame_generator_->Bind(std::move(frame_sink_client_binding));
  frame_generator_->OnWindowSizeChanged(root_window_->bounds().size());
  frame_generator_->SetDeviceScaleFactor(metrics_.device_scale_factor);
}

void PlatformDisplayDefault::OnAcceleratedWidgetDestroyed() {
  NOTREACHED();
}

void PlatformDisplayDefault::OnActivationChanged(bool active) {
  delegate_->OnActivationChanged(active);
}

void PlatformDisplayDefault::GetParentWindowAcceleratedWidget(
    gfx::AcceleratedWidget* widget) {
  if (metrics_.parent_window_widget_id == gfx::kNullAcceleratedWidget)
    return;

  *widget = metrics_.parent_window_widget_id;
}

}  // namespace ws
}  // namespace ui
