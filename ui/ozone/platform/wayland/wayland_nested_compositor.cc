// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_nested_compositor.h"

#include <wayland-client-protocol.h>
#include <wayland-server-core.h>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_nested_surface.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

namespace {

// Default wayland socket name.
constexpr base::FilePath::CharType kSocketName[] =
    FILE_PATH_LITERAL("chromium-wayland-nested-compositor");

#if !defined(PFNEGLBINDWAYLANDDISPLAYWL)
typedef EGLBoolean (*PFNEGLBINDWAYLANDDISPLAYWL)(EGLDisplay,
                                                 struct wl_display*);
#endif

#if !defined(PFNEGLUNBINDWAYLANDDISPLAYWL)
typedef EGLBoolean (*PFNEGLUNBINDWAYLANDDISPLAYWL)(EGLDisplay,
                                                   struct wl_display*);
#endif

#if !defined(PFNEGLQUERYWAYLANDBUFFERWL)
typedef EGLBoolean (*PFNEGLQUERYWAYLANDBUFFERWL)(EGLDisplay,
                                                 struct wl_resource*,
                                                 EGLint attribute,
                                                 EGLint* value);
#endif

#if !defined(PFNEGLCREATEIMAGEKHRPROC)
typedef EGLImageKHR (*PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay,
                                                EGLContext,
                                                EGLenum target,
                                                EGLClientBuffer,
                                                const EGLint* attribList);
#endif

#if !defined(PFNEGLDESTROYIMAGEKHRPROC)
typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay, EGLImageKHR);
#endif

#if !defined(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target,
                                                    GLeglImageOES);
#endif

#if !defined(PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL)
typedef struct wl_buffer*(EGLAPIENTRYP PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL)(
    EGLDisplay dpy,
    EGLImageKHR image);
#endif

#ifndef EGL_WAYLAND_BUFFER_WL
#define EGL_WAYLAND_BUFFER_WL 0x31D5 /* eglCreateImageKHR target */
#endif

static PFNEGLBINDWAYLANDDISPLAYWL bind_display;
static PFNEGLUNBINDWAYLANDDISPLAYWL unbind_display;
static PFNEGLQUERYWAYLANDBUFFERWL query_buffer;
static PFNEGLCREATEIMAGEKHRPROC create_image;
static PFNEGLDESTROYIMAGEKHRPROC destroy_image;
static PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL create_wayland_buffer_from_image;

// TODO(msisov, tonikitoo, jkim): share these with exo server.
template <class T>
T* GetUserDataAs(struct wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data = base::WrapUnique(GetUserDataAs<T>(resource));
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// ----------------------------------------------------------------------------
// wl_surface_interface

void surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void surface_attach(wl_client* client,
                    wl_resource* resource,
                    wl_resource* buffer,
                    int32_t x,
                    int32_t y) {
  GetUserDataAs<WaylandNestedSurface>(resource)->AttachBuffer(buffer);
}

void surface_damage(wl_client* client,
                    wl_resource* resource,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) {}

uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks) {
  return (ticks - base::TimeTicks()).InMilliseconds();
}

void HandleSurfaceFrameCallback(wl_resource* resource,
                                base::TimeTicks frame_time) {
  if (!frame_time.is_null()) {
    wl_callback_send_done(resource, TimeTicksToMilliseconds(frame_time));
    // TODO(msisov, reveman, tonikitoo): Remove this potentially blocking flush
    // and instead watch the file descriptor to be ready for write without
    // blocking.
    wl_client_flush(wl_resource_get_client(resource));
  }
  wl_resource_destroy(resource);
}

void surface_frame(wl_client* client,
                   wl_resource* resource,
                   uint32_t callback) {
  wl_resource* callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback =
      std::make_unique<base::CancelableCallback<void(base::TimeTicks)>>(
          base::Bind(&HandleSurfaceFrameCallback,
                     base::Unretained(callback_resource)));

  GetUserDataAs<WaylandNestedSurface>(resource)->RequestFrameCallback(
      cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, std::move(cancelable_callback));
}

void surface_set_opaque_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {}

void surface_set_input_region(wl_client* client,
                              wl_resource* resource,
                              wl_resource* region_resource) {}

void surface_commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandNestedSurface>(resource)->Commit();
}

const struct wl_surface_interface surface_implementation = {
    surface_destroy,
    surface_attach,
    surface_damage,
    surface_frame,
    surface_set_opaque_region,
    surface_set_input_region,
    surface_commit,
};

// ----------------------------------------------------------------------------
// wl_compositor_interface

void compositor_create_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  WaylandNestedCompositor* compositor =
      GetUserDataAs<WaylandNestedCompositor>(resource);

  WaylandConnection* connection = compositor->connection();
  WaylandWindow* window = connection->GetLastWindow();
  CHECK(window);

  std::unique_ptr<WaylandNestedSurface> nested_surface =
      std::make_unique<WaylandNestedSurface>(compositor, window->surface());

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, 1 /*TODO fix version */, id);

  SetImplementation(surface_resource, &surface_implementation,
                    std::move(nested_surface));
}

void compositor_create_region(wl_client* client,
                              wl_resource* resource,
                              uint32_t id) {}

const struct wl_compositor_interface compositor_implementation = {
    compositor_create_surface, compositor_create_region};

constexpr uint32_t kMaxCompositorVersion = 3;

void bind_compositor(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_compositor_interface,
                         std::min(version, kMaxCompositorVersion), id);

  wl_resource_set_implementation(resource, &compositor_implementation, data,
                                 nullptr);
}

}  // namespace

WaylandNestedCompositor::WaylandNestedCompositor(WaylandConnection* connection)
    : connection_(connection) {
  CHECK(connection_);
}

WaylandNestedCompositor::~WaylandNestedCompositor() {
  wl_display_destroy(wl_display_);
}

bool WaylandNestedCompositor::Initialize() {
  wl_display_ = wl_display_create();
  if (!wl_display_) {
    LOG(ERROR) << "Failed to create Wayland display";
    return false;
  }

  const std::string socket_name(kSocketName);
  if (!AddSocket(socket_name.c_str())) {
    LOG(ERROR) << "Failed to create Wayland socket";
    return false;
  }

  wl_global_create(wl_display_, &wl_compositor_interface, kMaxCompositorVersion,
                   this, bind_compositor);

  if (InitializeEGL()) {
    bind_display(egl_display_, wl_display_);
    return true;
  }
  LOG(ERROR) << "Failed to initialize EGL.";

  return false;
}

int WaylandNestedCompositor::GetFileDescriptor() const {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_);
  DCHECK(event_loop);
  return wl_event_loop_get_fd(event_loop);
}

void WaylandNestedCompositor::Dispatch(base::TimeDelta timeout) {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_);
  DCHECK(event_loop);
  wl_event_loop_dispatch(event_loop, timeout.InMilliseconds());
}

void WaylandNestedCompositor::Flush() {
  wl_display_flush_clients(wl_display_);
}

wl_buffer* WaylandNestedCompositor::CreateWaylandBufferFromImage(
    EGLImageKHR image) {
  DCHECK(image);
  return create_wayland_buffer_from_image(egl_display_, image);
}

void WaylandNestedCompositor::DestroyImage(EGLImageKHR image) {
  DCHECK(image);
  destroy_image(egl_display_, image);
}

EGLImageKHR WaylandNestedCompositor::CreateEGLImageKHRFromResource(
    struct wl_resource* resource) {
  DCHECK(resource);
  return static_cast<EGLImageKHR*>(create_image(
      egl_display_, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, resource, nullptr));
}

bool WaylandNestedCompositor::InitializeEGL() {
  InitializeEGLBindings();
  InitializeEGLDisplay();

  gl_surface_ = gl::InitializeGLSurface(new gl::SurfacelessEGL(gfx::Size()));
  gl_context_ = gl::InitializeGLContext(
      new gl::GLContextEGL(nullptr), gl_surface_.get(), gl::GLContextAttribs());
  CHECK(gl_context_);
  make_current_.reset(
      new ui::ScopedMakeCurrent(gl_context_.get(), gl_surface_.get()));

  // TODO(msisov, tonikitoo): use extensions.
  // const char* extensions = eglQueryString(egl_display_, EGL_EXTENSIONS);

  // TODO: move these.
  create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
      eglGetProcAddress("eglCreateImage"));
  destroy_image = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
      eglGetProcAddress("eglDestroyImage"));

  if (!create_image || !destroy_image) {
    CHECK(false) << "Failed to image make interfaces";
  }

  bind_display = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(
      eglGetProcAddress("eglBindWaylandDisplayWL"));
  unbind_display = reinterpret_cast<PFNEGLUNBINDWAYLANDDISPLAYWL>(
      eglGetProcAddress("eglUnbindWaylandDisplayWL"));
  query_buffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL>(
      eglGetProcAddress("eglQueryWaylandBufferWL"));

  if (!bind_display || !unbind_display || !query_buffer) {
    CHECK(false) << "Failed to make display and buffer interfaces";
  }

  create_wayland_buffer_from_image =
      reinterpret_cast<PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL>(
          eglGetProcAddress("eglCreateWaylandBufferFromImageWL"));
  CHECK(create_wayland_buffer_from_image);

  return true;
}

void WaylandNestedCompositor::InitializeEGLBindings() {
  setenv("EGL_PLATFORM", "wayland", 0);
  LoadDefaultEGLGLES2Bindings(gl::kGLImplementationEGLGLES2);
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);
  gl::InitializeStaticGLBindingsGL();
  gl::InitializeStaticGLBindingsEGL();
}

void WaylandNestedCompositor::InitializeEGLDisplay() {
  if (egl_display_ != EGL_NO_DISPLAY)
    return;

  EGLNativeDisplayType native_display =
      reinterpret_cast<intptr_t>(connection_->display());
  egl_display_ = gl::GLSurfaceEGL::InitializeDisplay(native_display);
  if (egl_display_ == EGL_NO_DISPLAY)
    CHECK(false) << "Cannot get default EGL display";
}

bool WaylandNestedCompositor::AddSocket(const std::string socket_name) {
  DCHECK(!socket_name.empty());
  return !wl_display_add_socket(wl_display_, socket_name.c_str());
}

}  // namespace ui
