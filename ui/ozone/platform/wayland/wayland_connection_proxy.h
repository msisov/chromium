// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_PROXY_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_PROXY_H_

#include "base/macros.h"
#include "ui/ozone/common/gbm_device_base.h"
#include "ui/gfx/native_widget_types.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/ozone/public/interfaces/wayland_connection.mojom.h"

struct wl_shm;

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Provides a mojo connection to WaylandConnection object on 
// browser process side. This is used to create Wayland dmabufs
// and ask it to do commits. Forwards calls directly to WaylandConnection in single process mode.
class WaylandConnectionProxy : public ozone::mojom::WaylandConnectionClient{
 public:
  WaylandConnectionProxy(WaylandConnection* connection);
  ~WaylandConnectionProxy();

  void SetWaylandConnection(ozone::mojom::WaylandConnectionPtr wc_ptr) override {
    LOG(ERROR) << "omg " << base::PlatformThread::CurrentId();
    CHECK(task_runner_);
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    wc_ptr.Bind(wc_ptr.PassInterface());
    wc_ptr_ = std::move(wc_ptr);
  }

  // WaylandConnectionProxy overrides:
  void CreateZwpLinuxDmabuf(base::File file, uint32_t width, uint32_t height, uint32_t stride, uint32_t offset, uint32_t current_format, uint32_t modifier);

  void SchedulePageFlip(uint32_t handle);
  void SchedulePageFlipInternal();

  void set_gbm_device(scoped_refptr<GbmDeviceBase> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }

  scoped_refptr<GbmDeviceBase> gbm_device() { return gbm_device_; }

  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget);
  void ScheduleFlush();
  wl_shm* shm();
  intptr_t Display();

  bool using_mojo() { return !!gbm_device_; }  

  void AddBindingWaylandConnectionClient(ozone::mojom::WaylandConnectionClientRequest request);
  
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() { return task_runner_; }

 private:
  WaylandConnection* connection_ = nullptr;

   scoped_refptr<GbmDeviceBase> gbm_device_;
  mojo::BindingSet<ozone::mojom::WaylandConnectionClient> bindings_;
  ozone::mojom::WaylandConnectionPtr wc_ptr_; 

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  THREAD_CHECKER(my_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WaylandConnectionProxy); 
};

} // namespace ui

#endif // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_CONNECTION_PROXY_H_
