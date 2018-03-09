// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection.h"

#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {
namespace {
const uint32_t kMaxCompositorVersion = 4;
const uint32_t kMaxSeatVersion = 4;
const uint32_t kMaxShmVersion = 1;
const uint32_t kMaxXdgShellVersion = 1;
const uint32_t kMaxTextInputManagerVersion = 1;
}  // namespace

WaylandConnection::WaylandConnection() : controller_(FROM_HERE) {}

WaylandConnection::~WaylandConnection() {
  DCHECK(window_map_.empty());
}

bool WaylandConnection::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandConnection::Global, &WaylandConnection::GlobalRemove,
  };

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "Failed to connect to Wayland display";
    return false;
  }

  registry_.reset(wl_display_get_registry(display_.get()));
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry";
    return false;
  }

  wl_registry_add_listener(registry_.get(), &registry_listener, this);

  while (!PrimaryOutput() || PrimaryOutput()->Geometry().IsEmpty())
    wl_display_roundtrip(display_.get());

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!seat_) {
    LOG(ERROR) << "No wl_seat object";
    return false;
  }
  if (!shell_v6_ && !shell_) {
    LOG(ERROR) << "No xdg_shell object";
    return false;
  }

  return true;
}

bool WaylandConnection::StartProcessingEvents() {
  if (watching_)
    return true;

  DCHECK(display_);
  wl_display_flush(display_.get());

  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!base::MessageLoopForUI::current()->WatchFileDescriptor(
          wl_display_get_fd(display_.get()), true,
          base::MessagePumpLibevent::WATCH_READ, &controller_, this))
    return false;

  watching_ = true;
  return true;
}

void WaylandConnection::ScheduleFlush() {
  if (scheduled_flush_ || !watching_)
    return;
  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&WaylandConnection::Flush, base::Unretained(this)));
  scheduled_flush_ = true;
}

WaylandWindow* WaylandConnection::GetWindow(gfx::AcceleratedWidget widget) {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

WaylandWindow* WaylandConnection::GetLastWindow() {
  DCHECK(!window_map_.empty());
  return window_map_.rbegin()->second;
}

WaylandWindow* WaylandConnection::GetCurrentFocusedWindow() {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandConnection::GetCurrentKeyboardFocusedWindow() {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_keyboard_focus())
      return window;
  }
  return nullptr;
}

void WaylandConnection::AddWindow(gfx::AcceleratedWidget widget,
                                  WaylandWindow* window) {
  window_map_[widget] = window;
}

void WaylandConnection::RemoveWindow(gfx::AcceleratedWidget widget) {
  if (touch_)
    touch_->RemoveTouchPoints(window_map_[widget]);
  window_map_.erase(widget);
}

WaylandOutput* WaylandConnection::PrimaryOutput() const {
  if (!output_list_.size())
    return nullptr;
  return output_list_.front().get();
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!pointer_ || !pointer_->cursor())
    return;
  pointer_->cursor()->UpdateBitmap(bitmaps, location, serial_);
}

int WaylandConnection::GetKeyboardModifiers() {
  int modifiers = 0;
  if (keyboard_)
    modifiers = keyboard_->modifiers();
  return modifiers;
}

void WaylandConnection::ResetPointerFlags() {
  if (pointer_)
    pointer_->ResetFlags();
}

void WaylandConnection::SetupClipboardDataBridge(
    ClipboardDataBridge* data, ClipboardDelegate** delegate) {
  DCHECK(data && !*delegate);
  clipboard_backing_store_ = data;
  *delegate = this;
}

void WaylandConnection::WriteToWMClipboard(const std::vector<std::string>& mime_types) {
  if (!data_source_) {
    wl_data_source* data_source =
        wl_data_device_manager_create_data_source(data_device_manager_.get());
    data_source_.reset(new WaylandDataSource(data_source));
    data_source_->set_connection(this);

    DCHECK(!clipboard_backing_store_->data_map().empty());

    // TODO(tonikitoo,msisov): Do we need to call WriteToClipboard again
    // if clipboard mime types changes?
    data_source_->WriteToClipboard(mime_types);
  }
}

void WaylandConnection::ReadFromWMClipboard(const std::string& mime_type) {
  data_device_->RequestSelectionData(mime_type);
}

bool WaylandConnection::ShouldGetClipboardDataFromWMClipboard() {
  return !data_source_;
}

std::vector<std::string> WaylandConnection::GetAvailableMimeTypes() {
  return data_device_->GetAvailableMimeTypes();
}

void WaylandConnection::DataSourceCancelled() {
  SetClipboardData(base::nullopt);
  data_source_.reset();
}

void WaylandConnection::SetClipboardData(
    const base::Optional<ClipboardDataBridge::DataMap>& data) {
  clipboard_backing_store_->data_map() =
      data.value_or(ClipboardDataBridge::DataMap());
}

void WaylandConnection::GetClipboardData(const std::string& mime_type,
    base::Optional<std::vector<uint8_t>>* data) {
  if (ShouldGetClipboardDataFromWMClipboard())
    ReadFromWMClipboard(mime_type);

  auto it = clipboard_backing_store_->data_map().find(mime_type);
  if (it != clipboard_backing_store_->data_map().end()) {
    data->emplace(it->second);
    return;
  }
}

void WaylandConnection::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandConnection::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandConnection::DispatchUiEvent(Event* event) {
  PlatformEventSource::DispatchEvent(event);
}

void WaylandConnection::OnFileCanReadWithoutBlocking(int fd) {
  wl_display_dispatch(display_.get());
  for (const auto& window : window_map_)
    window.second->ApplyPendingBounds();
}

void WaylandConnection::OnFileCanWriteWithoutBlocking(int fd) {}

const std::vector<std::unique_ptr<WaylandOutput>>&
WaylandConnection::GetOutputList() const {
  return output_list_;
}

// static
void WaylandConnection::Global(void* data,
                               wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandConnection::Capabilities, &WaylandConnection::Name,
  };
  static const xdg_shell_listener shell_listener = {
      &WaylandConnection::Ping,
  };
  static const zxdg_shell_v6_listener shell_v6_listener = {
      &WaylandConnection::PingV6,
  };

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (!connection->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    connection->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    if (!connection->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!connection->subcompositor_ &&
             strcmp(interface, "wl_subcompositor") == 0) {
    connection->subcompositor_ = wl::Bind<wl_subcompositor>(registry, name, 1);
    if (!connection->subcompositor_)
      LOG(ERROR) << "Failed to bind to wl_subcompositor global";
  } else if (!connection->shm_ && strcmp(interface, "wl_shm") == 0) {
    connection->shm_ =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
    if (!connection->shm_)
      LOG(ERROR) << "Failed to bind to wl_shm global";
  } else if (!connection->seat_ && strcmp(interface, "wl_seat") == 0) {
    connection->seat_ =
        wl::Bind<wl_seat>(registry, name, std::min(version, kMaxSeatVersion));
    if (!connection->seat_) {
      LOG(ERROR) << "Failed to bind to wl_seat global";
      return;
    }
    wl_seat_add_listener(connection->seat_.get(), &seat_listener, connection);

    // TODO(tonikitoo,msisov): The connection passed to WaylandInputDevice must have a
    // valid data device manager. We should ideally be robust to the compositor
    // advertising a wl_seat first. No known compositor does this, fortunately.
    if (!connection->data_device_manager_) {
      LOG(ERROR) << "No data device manager. Clipboard won't be fully functional";
      return;
    }
    wl_data_device* data_device = wl_data_device_manager_get_data_device(
        connection->data_device_manager_.get(), connection->seat_.get());
    connection->data_device_.reset(
        new WaylandDataDevice(connection, data_device));
  } else if (!connection->shell_v6_ &&
             strcmp(interface, "zxdg_shell_v6") == 0) {
    // Check for zxdg_shell_v6 first.
    connection->shell_v6_ = wl::Bind<zxdg_shell_v6>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_v6_) {
      LOG(ERROR) << "Failed to  bind to zxdg_shell_v6 global";
      return;
    }
    zxdg_shell_v6_add_listener(connection->shell_v6_.get(), &shell_v6_listener,
                               connection);
  } else if (!connection->shell_v6_ && !connection->shell_ &&
             strcmp(interface, "xdg_shell") == 0) {
    connection->shell_ = wl::Bind<xdg_shell>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_) {
      LOG(ERROR) << "Failed to  bind to xdg_shell global";
      return;
    }
    xdg_shell_add_listener(connection->shell_.get(), &shell_listener,
                           connection);
    xdg_shell_use_unstable_version(connection->shell_.get(),
                                   XDG_SHELL_VERSION_CURRENT);
  } else if (base::EqualsCaseInsensitiveASCII(interface, "wl_output")) {
    wl::Object<wl_output> output = wl::Bind<wl_output>(registry, name, 1);
    if (!output) {
      LOG(ERROR) << "Failed to bind to wl_output global";
      return;
    }

    if (!connection->output_list_.empty())
      NOTIMPLEMENTED() << "Multiple screens support is not implemented";

    connection->output_list_.push_back(
        base::WrapUnique(new WaylandOutput(output.release())));
  } else if (!connection->text_input_manager_v1_ &&
             strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    connection->text_input_manager_v1_ = wl::Bind<zwp_text_input_manager_v1>(
        registry, name, std::min(version, kMaxTextInputManagerVersion));
    if (!connection->text_input_manager_v1_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v1 global";
      return;
    }
  } else if (!connection->data_device_manager_ &&
             strcmp(interface, "wl_data_device_manager") == 0) {
    connection->data_device_manager_ =
        wl::Bind<wl_data_device_manager>(registry, name, 1);
  }

  connection->ScheduleFlush();
}

// static
void WaylandConnection::GlobalRemove(void* data,
                                     wl_registry* registry,
                                     uint32_t name) {
  NOTIMPLEMENTED();
}

// static
void WaylandConnection::Capabilities(void* data,
                                     wl_seat* seat,
                                     uint32_t capabilities) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!connection->pointer_) {
      wl_pointer* pointer = wl_seat_get_pointer(connection->seat_.get());
      if (!pointer) {
        LOG(ERROR) << "Failed to get wl_pointer from seat";
        return;
      }
      connection->pointer_ = std::make_unique<WaylandPointer>(
          pointer, base::Bind(&WaylandConnection::DispatchUiEvent,
                              base::Unretained(connection)));
      connection->pointer_->set_connection(connection);
    }
  } else if (connection->pointer_) {
    connection->pointer_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!connection->keyboard_) {
      wl_keyboard* keyboard = wl_seat_get_keyboard(connection->seat_.get());
      if (!keyboard) {
        LOG(ERROR) << "Failed to get wl_keyboard from seat";
        return;
      }
      connection->keyboard_ = std::make_unique<WaylandKeyboard>(
          keyboard, base::Bind(&WaylandConnection::DispatchUiEvent,
                               base::Unretained(connection)));
      connection->keyboard_->set_connection(connection);
    }
  } else if (connection->keyboard_) {
    connection->keyboard_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
    if (!connection->touch_) {
      wl_touch* touch = wl_seat_get_touch(connection->seat_.get());
      if (!touch) {
        LOG(ERROR) << "Failed to get wl_touch from seat";
        return;
      }
      connection->touch_ = std::make_unique<WaylandTouch>(
          touch, base::Bind(&WaylandConnection::DispatchUiEvent,
                            base::Unretained(connection)));
      connection->touch_->set_connection(connection);
    }
  } else if (connection->touch_) {
    connection->touch_.reset();
  }
  connection->ScheduleFlush();
}

// static
void WaylandConnection::Name(void* data, wl_seat* seat, const char* name) {}

// static
void WaylandConnection::PingV6(void* data,
                               zxdg_shell_v6* shell_v6,
                               uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  zxdg_shell_v6_pong(shell_v6, serial);
  connection->ScheduleFlush();
}

// static
void WaylandConnection::Ping(void* data, xdg_shell* shell, uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  xdg_shell_pong(shell, serial);
  connection->ScheduleFlush();
}

}  // namespace ui
