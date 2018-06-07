// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ui_features.h"
#include "ui/display/manager/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/system_input_injector.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_native_display_delegate.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#include "ui/ozone/common/linux/gbm_device_linux.h"
#include "ui/ozone/platform/wayland/drm/drm_render_node_handle.h"
#include "ui/ozone/platform/wayland/drm/drm_render_node_path_finder.h"
#include "ui/ozone/platform/wayland/wayland_connection_connector.h"
#include "ui/ozone/platform/wayland/wayland_connection_proxy.h"

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
    return connector_ ? connector_.get() : gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    return CreatePlatformWindowWithProperties(delegate, properties);
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindowWithProperties(
      PlatformWindowDelegate* delegate,
      const PlatformWindowInitProperties& properties) override {
    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    if (!window->Initialize(properties))
      return nullptr;
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<WaylandNativeDisplayDelegate>(connection_.get());
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

    if (!args.single_process) {
      CHECK(args.using_mojo);
      connector_.reset(new WaylandConnectionConnector(connection_.get()));
    }

    cursor_factory_.reset(new BitmapCursorFactoryOzone);
    overlay_manager_.reset(new StubOverlayManager);
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& args) override {
    proxy_.reset(new WaylandConnectionProxy(connection_.get()));

    if (!args.single_process) {
      DrmRenderNodePathFinder path_finder;
      const base::FilePath drm_node_path = path_finder.GetDrmRenderNodePath();
      if (drm_node_path.empty())
        LOG(FATAL) << "Failed to find drm render node path.";

      DrmRenderNodeHandle handle;
      if (!handle.Initialize(drm_node_path))
        LOG(FATAL) << "Failed to initialize drm render node handle.";

      std::unique_ptr<GbmDeviceLinux> gbm_device_(new GbmDeviceLinux());
      gbm_device_->InitializeGbmDevice(handle.PassFD().release());
      if (!gbm_device_->device())
        LOG(FATAL) << "Failed to initialize gbm device.";

      proxy_->set_gbm_device(std::move(gbm_device_));
    }

    surface_factory_.reset(new WaylandSurfaceFactory(proxy_.get()));
  }

  void AddInterfaces(
      service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>* registry) override {
    registry->AddInterface<ozone::mojom::WaylandConnectionClient>(
        base::BindRepeating(
            &OzonePlatformWayland::CreateWaylandConnectionClientBinding,
            base::Unretained(this)));
  }

  std::vector<gfx::BufferFormat> GetSupportedBufferFormats() override {
    CHECK(connection_.get());
    return connection_->GetSupportedBufferFormats();
  }

  void CreateWaylandConnectionClientBinding(
      ozone::mojom::WaylandConnectionClientRequest request,
      const service_manager::BindSourceInfo& source_info) {
    proxy_->AddBindingWaylandConnectionClient(std::move(request));
  }

 private:
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_;
  std::unique_ptr<StubOverlayManager> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  std::unique_ptr<WaylandConnectionProxy> proxy_;
  std::unique_ptr<WaylandConnectionConnector> connector_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
