// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ui_features.h"
#include "ui/display/manager/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/system_input_injector.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/wayland_nested_compositor.h"
#include "ui/ozone/platform/wayland/wayland_nested_compositor_client.h"
#include "ui/ozone/platform/wayland/wayland_nested_compositor_watcher.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/clipboard_data_bridge.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland() {}
  ~OzonePlatformWayland() override {}

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    auto window =
        std::make_unique<WaylandWindow>(delegate, connection_.get(), bounds);
    if (!window->Initialize())
      return nullptr;
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  void QueryHostDisplaysData(QueryHostDisplaysDataCallback callback) override {
    // On Wayland, the screen dimensions come from WaylandOutput.
    DCHECK(connection_->PrimaryOutput());
    if (connection_->PrimaryOutput()) {
      DCHECK(!callback.is_null());
      gfx::Rect geometry = connection_->PrimaryOutput()->Geometry();
      callback.Run(std::vector<gfx::Size>(1, geometry.size()));
      return;
    }

    CHECK(false) << "Add support for asynchronous resolution fetch.";
  }

  void SetupClipboardDataBridge(ClipboardDataBridge* data,
                                ClipboardDelegate** delegate) override {
    connection_->SetupClipboardDataBridge(data, delegate);
  }

  void InitializeUI(const InitParams& args) override {
#if BUILDFLAG(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<WaylandXkbKeyboardLayoutEngine>(
            xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());
#endif
    connection_.reset(new WaylandConnection);
    if (!connection_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

    cursor_factory_.reset(new BitmapCursorFactoryOzone);
    overlay_manager_.reset(new StubOverlayManager);
    input_controller_ = CreateStubInputController();

    if (!args.single_process) {
      nested_compositor_.reset(new WaylandNestedCompositor(connection_.get()));
      if (!nested_compositor_->Initialize())
        CHECK(false) << "Wayland nested compositor failure.";

      nested_compositor_watcher_.reset(
          new WaylandNestedCompositorWatcher(nested_compositor_.get()));
    } else {
      surface_factory_.reset(new WaylandSurfaceFactory(connection_.get()));
    }

    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& args) override {
    if (!args.single_process) {
      DCHECK(!surface_factory_);
      nested_compositor_client_.reset(new WaylandNestedCompositorClient);
      if (!nested_compositor_client_->Initialize())
        CHECK(false) << "Wayland nested compositor client failure.";

      surface_factory_.reset(
          new WaylandSurfaceFactory(nested_compositor_client_.get()));
    }
  }

  void AddInterfaces(
      service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>* registry) override {
    registry->AddInterface<ui::mojom::LinuxInputMethodContext>(
        base::Bind(&OzonePlatformWayland::CreateInputMethodContext,
                   base::Unretained(this)));
  }

  void CreateInputMethodContext(
      ui::mojom::LinuxInputMethodContextRequest request,
      const service_manager::BindSourceInfo& source_info) {
    mojo::MakeStrongBinding(
        std::make_unique<WaylandInputMethodContext>(connection_.get()),
        std::move(request));
  }

 private:
  // Belong to browser process.
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_;
  std::unique_ptr<StubOverlayManager> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<WaylandNestedCompositor> nested_compositor_;
  std::unique_ptr<WaylandNestedCompositorWatcher> nested_compositor_watcher_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Can belong to either browser or gpu process depending on
  // |args.single_process| value.
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;

  // Belong to gpu process if exists.
  std::unique_ptr<WaylandNestedCompositorClient> nested_compositor_client_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
