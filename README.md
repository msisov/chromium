# Chromium for Wayland

The goal of this project is to enable
[Chromium browser](https://www.chromium.org/) to run on
[Wayland](https://wayland.freedesktop.org/). Note that contrary to
[01.org/ozone-wayland](https://github.com/01org/ozone-wayland), the idea is
to keep it very close to upstream developments as well as aligned on Google's own
plans. In particular, this fork is rebased against
[Chromium ToT](https://chromium.googlesource.com/chromium/src.git) each week
and patches are upstreamed as soon as possible.

The implementation also relies on actively developed Chromium technologies:

* [Aura](https://www.chromium.org/developers/design-documents/aura/aura-overview) for the user interface and windowing.
* [Ozone](https://chromium.googlesource.com/chromium/src/+/master/docs/ozone_overview.md) as a platform abstraction layer together with the [upstream Wayland backend](https://chromium.googlesource.com/chromium/src.git/+/master/ui/ozone/platform/wayland/).
* [Mojo](https://chromium.googlesource.com/chromium/src/+/master/mojo) to perform IPC communication.

Notice that the effort done here is also useful to run Chromium with Ozone on
Linux Desktop for X11/Wayland.

# What is Chromium browser?

![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

[Chromium](https://www.chromium.org) is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the Web.

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory Structure
](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

# Building Chromium

General information is provided by the upstream documentation for
[Chromium on Linux](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md)
and
[Ozone](https://chromium.googlesource.com/chromium/src/+/master/docs/ozone_overview.md).
Here is the summary of commands to build and run Chrome for Wayland:

```
gn args out/Ozone --args="use_ozone=true use_xkbcommon=true use_system_minigbm=true"
ninja -C out/Ozone chrome
./out/Ozone/chrome --ozone-platform=wayland

Please note that Ozone/Wayland also supports GpuMemoryBuffers and in order to enable them, the --enable-native-gpu-buffers must be passed.

Note that GN defaults to debug builds, which naturally take longer to finish and produce slower binaries at runtime. The 'is_debug=false' GN arguments disables it.

Also note that some touch oriented Web pages like Google Maps, work better when the Touch Event API is explicitly enabled
in chrome://flags or a command line argument --touch-events=enabled is passed.

It is also possible to enable proprietary codecs (so that mp4, h264 medias play) with the following GN args: 'proprietary_codecs=true ffmpeg_branding=\"Chrome\"'.
```

By default, the `headless`, `x11` and `wayland` Ozone backends are
compiled and X11 is selected when `--ozone-platform` is not specified.
Please refer to the
[GN Configuration notes](https://chromium.googlesource.com/chromium/src/+/master/docs/ozone_overview.md#GN-Configuration-notes) for details on how to change
that behavior.

# Running Tests

To be added.

# Rebase Strategy

The fork is rebased every week against Chromium ToT.
The goal is to be as close as possible to the latest code, which is
constantly receiving performance and stability fixes.

Here is the current process:

* Every week, a member of the Igalia Chromium team takes the rebase shift.

* Commits that are complementary of each other, receive a "fixup!" prefix on
the commit title, and keep the rest of original commit title unchanged.

For example:

```
$ git log --oneline
commit 1
commit 2
commit 3
fixup! commit 1
fixup! commit 2
commit 4
fixup! commit 2
(..)
```

This allows an easy identification of "fixup" commits, which should be squashed into
their original counterpart commit as part of the next rebase cycle. That way we keep
our Git history clean, and commits as atomic as possible, for when upstreaming.

Git has [an optimized flow for this](http://fle.github.io/git-tip-keep-your-branch-clean-with-fixup-and-autosquash.html) as well.

* We always keep the 'ozone-wayland-dev' branch as our primarily development branch.

This means that force pushes will happen. So every time one of the team members
rebases our branch, the developer should first back up the existing ozone-wayland-dev
browser, with the following naming: ozone-wayland-dev-rXXXX, where XXXX is the respective
Chromium baseline of the branch.

* Branch acceptance criteria

Make sure that the patches meet the [coding style](https://www.chromium.org/developers/coding-style) criteria.

* Keep [our internal buildbot](https://build-chromium.igalia.com/) green.
