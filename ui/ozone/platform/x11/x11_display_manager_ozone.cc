// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_display_manager_ozone.h"

#include "base/logging.h"
#include "base/memory/protected_memory_cfi.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

#include <dlfcn.h>

namespace ui {

namespace {

constexpr int default_refresh = 60;

std::unique_ptr<display::DisplaySnapshot> CreateSnapshot(
    int64_t display_id,
    gfx::Rect bounds,
    gfx::ColorSpace color_space) {
  display::DisplaySnapshot::DisplayModeList modes;
  std::unique_ptr<display::DisplayMode> display_mode =
      std::make_unique<display::DisplayMode>(
          gfx::Size(bounds.width(), bounds.height()), false, default_refresh);
  modes.push_back(std::move(display_mode));
  const display::DisplayMode* mode = modes.back().get();

  return std::make_unique<display::DisplaySnapshot>(
      display_id, gfx::Point(bounds.x(), bounds.y()),
      gfx::Size(bounds.width(), bounds.height()),
      display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE, false,
      false, false, false, color_space, "", base::FilePath(), std::move(modes),
      std::vector<uint8_t>(), mode, mode, 0, 0, gfx::Size());
}

std::vector<std::unique_ptr<display::DisplaySnapshot>>
BuildFallbackDisplayList() {
  std::vector<std::unique_ptr<display::DisplaySnapshot>> snapshots;
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  int64_t display_id = 0;
  gfx::Rect bounds(0, 0, width, height);
  std::unique_ptr<display::DisplaySnapshot> snapshot =
      CreateSnapshot(display_id, bounds, gfx::ColorSpace());
  snapshots.push_back(std::move(snapshot));
  return snapshots;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// X11DisplayManagerOzone, public:

X11DisplayManagerOzone::X11DisplayManagerOzone()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(0),
      xrandr_event_base_(0) {
  // We only support 1.3+. There were library changes before this and we should
  // use the new interface instead of the 1.2 one.
  int randr_version_major = 0;
  int randr_version_minor = 0;
  if (XRRQueryVersion(xdisplay_, &randr_version_major, &randr_version_minor)) {
    xrandr_version_ = randr_version_major * 100 + randr_version_minor;
  }
  // Need at least xrandr version 1.3.
  if (xrandr_version_ < 103) {
    snapshots_ = BuildFallbackDisplayList();
    return;
  }

  int error_base_ignored = 0;
  XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  XRRSelectInput(xdisplay_, x_root_window_,
                 RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                     RRCrtcChangeNotifyMask);
  BuildDisplaysFromXRandRInfo();
  return;
}

X11DisplayManagerOzone::~X11DisplayManagerOzone() {
  if (xrandr_version_ >= 103 && ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11DisplayManagerOzone::SetObserver(Observer* observer) {
  observer_ = observer;
  if (snapshots_.size() > 0)
    observer_->OnOutputReadyForUse();
}

void X11DisplayManagerOzone::GetDisplaysSnapshot(
    display::GetDisplaysCallback callback) {
  std::vector<display::DisplaySnapshot*> snapshots;
  for (const auto& snapshot : snapshots_)
    snapshots.push_back(snapshot.get());
  std::move(callback).Run(snapshots);
}

bool X11DisplayManagerOzone::CanDispatchEvent(const ui::PlatformEvent& event) {
  // TODO(msisov, jkim): implement this.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

uint32_t X11DisplayManagerOzone::DispatchEvent(const ui::PlatformEvent& event) {
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

void X11DisplayManagerOzone::BuildDisplaysFromXRandRInfo() {
  DCHECK(xrandr_version_ >= 103);
  gfx::XScopedPtr<
      XRRScreenResources,
      gfx::XObjectDeleter<XRRScreenResources, void, XRRFreeScreenResources>>
      resources(XRRGetScreenResourcesCurrent(xdisplay_, x_root_window_));
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays. Falling back to Root Window.";
    snapshots_ = BuildFallbackDisplayList();
    return;
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

  primary_display_index_ = 0;
  RROutput primary_display_id = XRRGetOutputPrimary(xdisplay_, x_root_window_);

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

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
      if (is_primary_display)
        explicit_primary_display_index = display_id;

      auto monitor_iter = output_to_monitor.find(output_id);
      if (monitor_iter != output_to_monitor.end() && monitor_iter->second == 0)
        monitor_order_primary_display_index = display_id;

      gfx::ColorSpace color_space;
      if (!display::Display::HasForceColorProfile()) {
        gfx::ICCProfile icc_profile = ui::GetICCProfileForMonitor(
            monitor_iter == output_to_monitor.end() ? 0 : monitor_iter->second);
        icc_profile.HistogramDisplay(display_id);
        color_space = icc_profile.GetColorSpace();
      } else {
        color_space = display::Display::GetForcedColorProfile();
      }

      std::unique_ptr<display::DisplaySnapshot> snapshot =
          CreateSnapshot(display_id, crtc_bounds, color_space);
      snapshots_.push_back(std::move(snapshot));
    }
  }

  if (explicit_primary_display_index != -1) {
    primary_display_index_ = explicit_primary_display_index;
  } else if (monitor_order_primary_display_index != -1) {
    primary_display_index_ = monitor_order_primary_display_index;
  }
}

}  // namespace ui
