// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_

#include <map>

#include "ui/gfx/buffer_types.h"

#include "base/files/file.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_data_device.h"
#include "ui/ozone/platform/wayland/wayland_data_source.h"
#include "ui/ozone/platform/wayland/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_output.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/wayland_touch.h"
#include "ui/ozone/public/clipboard_delegate.h"

#include "ui/ozone/public/interfaces/wayland_connection.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace ui {

class WaylandWindow;

class WaylandConnection : public PlatformEventSource,
                          public ClipboardDelegate,
                          public ozone::mojom::WaylandConnection,
                          public base::MessagePumpLibevent::FdWatcher {
 public:
  WaylandConnection();
  ~WaylandConnection() override;

  bool Initialize();
  bool StartProcessingEvents();

  void SayHello() override {
    CHECK(false) << "HELLO";
  }
  void CreateZwpLinuxDmabuf(base::File file, uint32_t width, uint32_t height, uint32_t stride, uint32_t offset, uint32_t format, uint32_t modifier) override;
  
  mojo::Binding<ozone::mojom::WaylandConnection> binding_;

  // Schedules a flush of the Wayland connection.
  void ScheduleFlush();

  wl_display* display() { return display_.get(); }
  wl_compositor* compositor() { return compositor_.get(); }
  wl_subcompositor* subcompositor() { return subcompositor_.get(); }
  wl_shm* shm() { return shm_.get(); }
  xdg_shell* shell() { return shell_.get(); }
  zxdg_shell_v6* shell_v6() { return shell_v6_.get(); }
  wl_seat* seat() { return seat_.get(); }
  wl_data_device* data_device() { return data_device_->data_device(); }
  zwp_text_input_manager_v1* text_input_manager_v1() {
    return text_input_manager_v1_.get();
  }
  zwp_linux_dmabuf_v1* zwp_linux_dmabuf() { return zwp_linux_dmabuf_; }

  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget);
  WaylandWindow* GetCurrentFocusedWindow();
  WaylandWindow* GetCurrentKeyboardFocusedWindow();
  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);

  const std::vector<std::unique_ptr<WaylandOutput>>& GetOutputList() const;
  WaylandOutput* PrimaryOutput() const;

  void set_serial(uint32_t serial) { serial_ = serial; }
  uint32_t serial() { return serial_; }

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& location);

  int GetKeyboardModifiers();
  void DispatchUiEvent(Event* event);

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() { return pointer_.get(); }

  // Resets flags and keyboard modifiers.
  //
  // This method is specially handy for cases when the WaylandPointer state is
  // modified by a POINTER_DOWN event, but the respective POINTER_UP event is
  // not delivered.
  void ResetPointerFlags();

  // Clipboard implementation.
  ClipboardDelegate* GetClipboardDelegate();
  void DataSourceCancelled();
  void SetClipboardData(const std::string& contents,
                        const std::string& mime_type);

  // ClipboardDelegate.
  void OfferClipboardData(
      const ClipboardDelegate::DataMap& data_map,
      ClipboardDelegate::OfferDataClosure callback) override;
  void RequestClipboardData(
      const std::string& mime_type,
      ClipboardDelegate::DataMap* data_map,
      ClipboardDelegate::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardDelegate::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner() override;

  void SchedulePageFlip(uint32_t handle) override;

 private:
  void CreateZwpLinuxDmabufInternal(base::File file, uint32_t width, uint32_t height, uint32_t stride, uint32_t offset, uint32_t format, uint32_t modifier);

  void Flush();

  // PlatformEventSource
  void OnDispatcherListChanged() override;

  // base::MessagePumpLibevent::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // wl_registry_listener
  static void Global(void* data,
                     wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version);
  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // wl_seat_listener
  static void Capabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void Name(void* data, wl_seat* seat, const char* name);

  // zxdg_shell_v6_listener
  static void PingV6(void* data, zxdg_shell_v6* zxdg_shell_v6, uint32_t serial);

  // xdg_shell_listener
  static void Ping(void* data, xdg_shell* shell, uint32_t serial);

  // zwp_linux_dmabuf_v1_listener 
  static void Modifiers(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf,
    uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo);
  static void Format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf, uint32_t format);

  static void CreateSucceeded(void *data, struct zwp_linux_buffer_params_v1 *params, struct wl_buffer *new_buffer);
  static void CreateFailed(void *data, struct zwp_linux_buffer_params_v1 *params);
 
  std::map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  wl::Object<wl_display> display_;
  wl::Object<wl_data_device_manager> data_device_manager_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<wl_seat> seat_;
  wl::Object<wl_shm> shm_;
  wl::Object<xdg_shell> shell_;
  wl::Object<zxdg_shell_v6> shell_v6_;
  wl::Object<zwp_text_input_manager_v1> text_input_manager_v1_;
  // TODO: use wl::Object
  struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_ = nullptr;
  // TODO: each window with new params.
  std::map<uint32_t, wl::Object<wl_buffer>> buffers_;
  std::map<struct zwp_linux_buffer_params_v1*, uint32_t> handle_to_params_map_;

  std::unique_ptr<WaylandDataDevice> data_device_;
  std::unique_ptr<WaylandDataSource> data_source_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandTouch> touch_;

  bool scheduled_flush_ = false;
  bool watching_ = false;
  base::MessagePumpLibevent::FdWatchController controller_;

  uint32_t serial_ = 0;

  std::vector<std::unique_ptr<WaylandOutput>> output_list_;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  ClipboardDelegate::DataMap* data_map_ = nullptr;

  // Stores the callback to be invoked upon data reading from clipboard.
  RequestDataClosure read_clipboard_closure_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnection);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_H_
