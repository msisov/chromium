// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_display_fetcher_ozone.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/memory/protected_memory_cfi.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

float GetDeviceScaleFactor() {
  float device_scale_factor = 1.0f;
  // TODO(jkim) : Get device scale factor using scale factor and resolution like
  // 'GtkUi::GetRawDeviceScaleFactor'.
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  return device_scale_factor;
}

display::Display GetFallbackDisplay() {
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  gfx::Size physical_size(WidthMMOfScreen(screen), HeightMMOfScreen(screen));

  gfx::Rect bounds_in_pixels(0, 0, width, height);
  display::Display fallback_display(0, bounds_in_pixels);
  if (!display::Display::HasForceDeviceScaleFactor() &&
      !display::IsDisplaySizeBlackListed(physical_size)) {
    const float device_scale_factor = GetDeviceScaleFactor();
    DCHECK_LE(1.0f, device_scale_factor);
    fallback_display.SetScaleAndBounds(device_scale_factor, bounds_in_pixels);
  }

  return fallback_display;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// X11DisplayFetcherOzone, public:

X11DisplayFetcherOzone::X11DisplayFetcherOzone(
    X11DisplayFetcherOzone::Delegate* delegate)
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(0),
      xrandr_event_base_(0),
      delegate_(delegate) {
  DCHECK(delegate);
  // We only support 1.3+. There were library changes before this and we should
  // use the new interface instead of the 1.2 one.
  int randr_version_major = 0;
  int randr_version_minor = 0;
  if (XRRQueryVersion(xdisplay_, &randr_version_major, &randr_version_minor)) {
    xrandr_version_ = randr_version_major * 100 + randr_version_minor;
  }
  // Need at least xrandr version 1.3.
  if (xrandr_version_ < 103) {
    delegate_->AddDisplay(GetFallbackDisplay(), true);
    return;
  }

  int error_base_ignored = 0;
  XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  XRRSelectInput(xdisplay_, x_root_window_,
                 RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                     RRCrtcChangeNotifyMask);

  const std::vector<display::Display> displays = BuildDisplaysFromXRandRInfo();
  for (auto& display : displays)
    delegate_->AddDisplay(display, display.id() == primary_display_index_);
}

X11DisplayFetcherOzone::~X11DisplayFetcherOzone() {
  if (xrandr_version_ >= 103 && ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

bool X11DisplayFetcherOzone::CanDispatchEvent(const ui::PlatformEvent& event) {
  // TODO(msisov, jkim): implement this.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

uint32_t X11DisplayFetcherOzone::DispatchEvent(const ui::PlatformEvent& event) {
  // TODO(msisov, jkim): implement this.
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::POST_DISPATCH_NONE;
}

typedef XRRMonitorInfo* (*XRRGetMonitors)(::Display*, Window, bool, int*);
typedef void (*XRRFreeMonitors)(XRRMonitorInfo*);

PROTECTED_MEMORY_SECTION base::ProtectedMemory<XRRGetMonitors>
    g_XRRGetMonitors_ptr;
PROTECTED_MEMORY_SECTION base::ProtectedMemory<XRRFreeMonitors>
    g_XRRFreeMonitors_ptr;

std::vector<display::Display>
X11DisplayFetcherOzone::BuildDisplaysFromXRandRInfo() {
  DCHECK(xrandr_version_ >= 103);
  std::vector<display::Display> displays;
  gfx::XScopedPtr<
      XRRScreenResources,
      gfx::XObjectDeleter<XRRScreenResources, void, XRRFreeScreenResources>>
      resources(XRRGetScreenResourcesCurrent(xdisplay_, x_root_window_));
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays. Falling back to Root Window.";
    displays.push_back(GetFallbackDisplay());
    return displays;
  }

  std::map<RROutput, int> output_to_monitor;
  if (xrandr_version_ >= 105) {
    void* xrandr_lib = dlopen(NULL, RTLD_NOW);
    if (xrandr_lib) {
      static base::ProtectedMemory<XRRGetMonitors>::Initializer get_init(
          &g_XRRGetMonitors_ptr, reinterpret_cast<XRRGetMonitors>(
                                     dlsym(xrandr_lib, "XRRGetMonitors")));
      static base::ProtectedMemory<XRRFreeMonitors>::Initializer free_init(
          &g_XRRFreeMonitors_ptr, reinterpret_cast<XRRFreeMonitors>(
                                      dlsym(xrandr_lib, "XRRFreeMonitors")));
      if (*g_XRRGetMonitors_ptr && *g_XRRFreeMonitors_ptr) {
        int nmonitors = 0;
        XRRMonitorInfo* monitors = base::UnsanitizedCfiCall(
            g_XRRGetMonitors_ptr)(xdisplay_, x_root_window_, false, &nmonitors);
        for (int monitor = 0; monitor < nmonitors; monitor++) {
          for (int j = 0; j < monitors[monitor].noutput; j++) {
            output_to_monitor[monitors[monitor].outputs[j]] = monitor;
          }
        }
        base::UnsanitizedCfiCall(g_XRRFreeMonitors_ptr)(monitors);
      }
    }
  }

  RROutput primary_display_id = XRRGetOutputPrimary(xdisplay_, x_root_window_);

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

  bool has_work_area = false;
  gfx::Rect work_area_in_pixels;
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window_, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    work_area_in_pixels = gfx::Rect(value[0], value[1], value[2], value[3]);
    has_work_area = true;
  }

  // As per-display scale factor is not supported right now,
  // the X11 root window's scale factor is always used.
  const float device_scale_factor = GetDeviceScaleFactor();
  for (int i = 0; i < resources->noutput; ++i) {
    RROutput output_id = resources->outputs[i];
    gfx::XScopedPtr<XRROutputInfo,
                    gfx::XObjectDeleter<XRROutputInfo, void, XRRFreeOutputInfo>>
        output_info(XRRGetOutputInfo(xdisplay_, resources.get(), output_id));

    bool is_connected = (output_info->connection == RR_Connected);
    if (!is_connected)
      continue;

    bool is_primary_display = output_id == primary_display_id;

    if (output_info->crtc) {
      gfx::XScopedPtr<XRRCrtcInfo,
                      gfx::XObjectDeleter<XRRCrtcInfo, void, XRRFreeCrtcInfo>>
          crtc(XRRGetCrtcInfo(xdisplay_, resources.get(), output_info->crtc));

      int64_t display_id = -1;
      if (!display::EDIDParserX11(output_id).GetDisplayId(
              static_cast<uint8_t>(i), &display_id)) {
        // It isn't ideal, but if we can't parse the EDID data, fallback on the
        // display number.
        display_id = i;
      }

      gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
      display::Display display(display_id, crtc_bounds);

      if (!display::Display::HasForceDeviceScaleFactor()) {
        display.SetScaleAndBounds(device_scale_factor, crtc_bounds);
      }

      if (has_work_area) {
        gfx::Rect intersection_in_pixels = crtc_bounds;
        if (is_primary_display) {
          intersection_in_pixels.Intersect(work_area_in_pixels);
        }
        // SetScaleAndBounds() above does the conversion from pixels to DIP for
        // us, but set_work_area does not, so we need to do it here.
        display.set_work_area(gfx::Rect(
            gfx::ScaleToFlooredPoint(intersection_in_pixels.origin(),
                                     1.0f / display.device_scale_factor()),
            gfx::ScaleToFlooredSize(intersection_in_pixels.size(),
                                    1.0f / display.device_scale_factor())));
      }

      switch (crtc->rotation) {
        case RR_Rotate_0:
          display.set_rotation(display::Display::ROTATE_0);
          break;
        case RR_Rotate_90:
          display.set_rotation(display::Display::ROTATE_90);
          break;
        case RR_Rotate_180:
          display.set_rotation(display::Display::ROTATE_180);
          break;
        case RR_Rotate_270:
          display.set_rotation(display::Display::ROTATE_270);
          break;
      }

      if (is_primary_display)
        explicit_primary_display_index = display_id;

      auto monitor_iter = output_to_monitor.find(output_id);
      if (monitor_iter != output_to_monitor.end() && monitor_iter->second == 0)
        monitor_order_primary_display_index = display_id;

      gfx::ColorSpace color_space;
      if (!display::Display::HasForceDisplayColorProfile()) {
        gfx::ICCProfile icc_profile = ui::GetICCProfileForMonitor(
            monitor_iter == output_to_monitor.end() ? 0 : monitor_iter->second);
        icc_profile.HistogramDisplay(display_id);
        color_space = icc_profile.GetColorSpace();
      } else {
        color_space = display::Display::GetForcedDisplayColorProfile();
      }

      displays.push_back(display);
    }
  }

  if (explicit_primary_display_index != -1) {
    primary_display_index_ = explicit_primary_display_index;
  } else if (monitor_order_primary_display_index != -1) {
    primary_display_index_ = monitor_order_primary_display_index;
  }

  if (displays.empty())
    displays.push_back(GetFallbackDisplay());

  return displays;
}

}  // namespace ui
