// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_nested_compositor_client.h"

#include <wayland-client.h>

#include "base/files/file_path.h"

namespace ui {

namespace {

constexpr uint32_t kMaxCompositorVersion = 3;

// Default wayland socket name.
constexpr base::FilePath::CharType kSocketName[] =
    FILE_PATH_LITERAL("chromium-wayland-nested-compositor");

}  // namespace

WaylandNestedCompositorClient::WaylandNestedCompositorClient() = default;

WaylandNestedCompositorClient::~WaylandNestedCompositorClient() = default;

bool WaylandNestedCompositorClient::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandNestedCompositorClient::Global,
      &WaylandNestedCompositorClient::GlobalRemove};

  const std::string socket_name(kSocketName);
  wl_display_.reset(wl_display_connect(socket_name.c_str()));

  if (!wl_display_) {
    LOG(ERROR) << "Failed to connect to Wayland display on " << socket_name
               << " socket.";
    return false;
  }

  registry_.reset(wl_display_get_registry(wl_display_.get()));
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry.";
    return false;
  }

  wl_registry_add_listener(registry_.get(), &registry_listener, this);
  wl_display_roundtrip(wl_display_.get());

  if (!compositor_) {
    LOG(ERROR) << "Failed to get wl_compositor object.";
    return false;
  }

  return true;
}

wl_surface* WaylandNestedCompositorClient::CreateOrGetSurface() {
  if (!wl_surface_) {
    wl_surface_.reset(wl_compositor_create_surface(compositor_.get()));
    if (!wl_surface_)
      CHECK(false) << "Failed to create wl_surface";
  }

  Flush();

  return wl_surface_.get();
}

void WaylandNestedCompositorClient::Flush() {
  // TODO: maybe use message pump instead.
  wl_display_flush(wl_display_.get());
}

// static
void WaylandNestedCompositorClient::Global(void* data,
                                           wl_registry* registry,
                                           uint32_t name,
                                           const char* interface,
                                           uint32_t version) {
  WaylandNestedCompositorClient* client =
      static_cast<WaylandNestedCompositorClient*>(data);
  if (!client->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    client->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    if (!client->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global.";
  }
}

// static
void WaylandNestedCompositorClient::GlobalRemove(void* data,
                                                 wl_registry* registry,
                                                 uint32_t name) {
  NOTIMPLEMENTED();
}

}  // namespace ui
