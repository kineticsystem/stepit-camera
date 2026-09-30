# Canon 5D Mark II Driver for ROS2 <!-- omit in toc -->

## Table of Contents <!-- omit in toc -->

- [Introduction](#introduction)
- [Requirements](#requirements)
- [Summary](#summary)
- [Existing ROS Packages](#existing-ros-packages)
- [Other Options](#other-options)
  - [Canon EDSDK](#canon-edsdk)
  - [digiCamControl](#digicamcontrol)
  - [gphoto2 as a Virtual Webcam](#gphoto2-as-a-virtual-webcam)
- [Licenses](#licenses)
- [Recommendation](#recommendation)
- [Try the Camera on Ubuntu](#try-the-camera-on-ubuntu)
- [Sources](#sources)

## Introduction

This document collects what we found when looking for a ROS2 package to control a Canon EOS 5D Mark II from ROS2. We searched GitHub and the web in September 2026.

Something similar already exists for Windows: [digiCamControl](https://digicamcontrol.com/). We want the same features inside ROS2.

## Requirements

In order of importance:

1. Stream what the camera sees (live view) as a ROS2 image topic.
2. Set the camera parameters: ISO, shutter speed and aperture.

Constraints:

- It must run on Ubuntu (Ubuntu 24.04 with ROS2 Jazzy, like StepIt).
- Our software is released under the MIT license, so every dependency must be compatible with it.

## Summary

There is no ROS2 package that does this today. We found several ROS1 packages built on [libgphoto2](https://github.com/gphoto/libgphoto2), none maintained, and none that streams live view and sets ISO, shutter speed and aperture at the same time.

The good news is that the hard part is already solved by libgphoto2. It officially supports the 5D Mark II with _Image Capture, Trigger Capture, Liveview, Configuration_, is packaged in Ubuntu, and has a rosdep key. A ROS2 driver on top of it is a small package.

Canon's own SDK (EDSDK) runs on Ubuntu now, but it does not support the 5D Mark II, so it is not an option.

## Existing ROS Packages

All the packages below are ROS1 (catkin). None of them builds on ROS2 without porting.

| Package | ROS | License | Live view | ISO / Shutter / Aperture | Last push | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| [wyca-robotics/gphoto2_ros](https://github.com/wyca-robotics/gphoto2_ros) | 1 | BSD-3 (Bosch) | No | Yes, generic `GetConfig`/`SetConfig` services | 2019 | Copy of the `photo` package from [bosch_drivers](http://wiki.ros.org/bosch_drivers). Clean C++ wrapper around libgphoto2. |
| [code-iai/iai_photo](https://github.com/code-iai/iai_photo) | 1 | None in repo (upstream is BSD) | No | Yes, same services | 2015 | Another catkinized copy of the Bosch `photo` package. |
| [Patrick-Lascombe/wyca_gphoto2](https://github.com/Patrick-Lascombe/wyca_gphoto2) | 1 | Unclear | No | Yes, `GetConfig`/`SetConfig` | 2019 | Adds focus and trigger actions and picture download. |
| [ivangr3/dslr_camera_ros](https://github.com/ivangr3/dslr_camera_ros) | 1 | **GPL-3.0** | Yes, timer calling `gp_camera_capture_preview` | No | 2024 | The only one that publishes the live view. Tiny, one file. |
| [BYU-AUVSI/a6000_ros](https://github.com/BYU-AUVSI/a6000_ros) | 1 | BSD-3 | No | Yes, `ConfigGet`/`ConfigSet` | 2019 | Written for a Sony a6000, but the driver code is generic gphoto2. |
| [florisvb/gphoto_canon_trigger](https://github.com/florisvb/gphoto_canon_trigger) | 1 | None | No | No | 2021 | Python. Triggers a capture only. Tested on a 5D Mark II. |
| [ikh-innovation/dslr_ros](https://github.com/ikh-innovation/dslr_ros) | 1 | MIT | Yes, via `/dev/video0` | No | 2021 | Shell script piping `gphoto2 --capture-movie` into a virtual webcam, then `usb_cam`. |
| [nargetdev/rpi_zenoh_ros2_daemon](https://github.com/nargetdev/rpi_zenoh_ros2_daemon) | 2 (over Zenoh) | None | No, still captures only | No | 2026 | Raspberry Pi + gphoto2 capture service for a Canon 6D, publishing `sensor_msgs/Image` over `rmw_zenoh`. Interesting architecture, not a camera driver. |

> [!IMPORTANT]
> A repository without a license is "all rights reserved": we can read it, but we cannot copy its code into an MIT project. We also cannot copy code from the GPL-3.0 package `dslr_camera_ros`. The Bosch `photo` package and `a6000_ros` are BSD and can be reused with attribution.

## Other Options

### Canon EDSDK

Canon's official SDK now has Linux builds: ARMv8 since version 13.17.10 and Intel 64-bit since version 13.18.30. Unfortunately, the 5D Mark II is not on the supported camera list of any 13.x release. The SDK versions that did support it (2.x) are Windows and macOS only.

The EDSDK is also proprietary: it requires registration with Canon and it cannot be redistributed, which makes it awkward for an open source MIT package anyway.

### digiCamControl

[digiCamControl](https://digicamcontrol.com/) does exactly what we want, but it is a Windows application based on .NET and the Canon SDK. It does not run on Ubuntu and it is not a library we can call from ROS2.

### gphoto2 as a Virtual Webcam

A well documented trick is to pipe the camera live view into a virtual webcam, then use a standard ROS2 camera driver like [v4l2_camera](https://index.ros.org/p/v4l2_camera/) or [usb_cam](https://github.com/ros-drivers/usb_cam).

```bash
sudo apt install gphoto2 ffmpeg v4l2loopback-dkms v4l2loopback-utils
sudo modprobe v4l2loopback exclusive_caps=1 max_buffers=2
gphoto2 --stdout --capture-movie | ffmpeg -i - -vf format=yuv420p -f v4l2 /dev/video0
```

This gives us streaming with zero code, and it is a good way to test the camera. However, the `gphoto2` process owns the USB connection for as long as it streams, so nothing else can change ISO, shutter speed or aperture at the same time. It fails our second requirement.

## Licenses

| Component | License | Fine with MIT? |
| --- | --- | --- |
| [libgphoto2](https://github.com/gphoto/libgphoto2) (C) | LGPL-2.1 | Yes, when dynamically linked, which is what `libgphoto2-dev` on Ubuntu gives us. |
| [python-gphoto2](https://github.com/jim-easterbrook/python-gphoto2) | LGPL-3.0 | Yes, importing it does not change the license of our code. |
| Bosch `photo` package, `a6000_ros` | BSD-3 | Yes, keep the copyright notice. |
| `dslr_camera_ros` | GPL-3.0 | No, do not copy code. Reading it for ideas is fine. |
| Canon EDSDK | Proprietary | No, and it does not support the camera anyway. |
| `v4l2loopback` | GPL-2.0 | Yes, it is a separate kernel module, not linked. |

## Recommendation

We should build a small ROS2 package on top of libgphoto2. There is nothing to fork that already runs on ROS2, but most of the work is known.

- One node owns the camera. Only one process can hold the USB connection, so streaming and configuration must live in the same node.
- A timer calls `gp_camera_capture_preview()` and publishes the returned JPEG as-is on a `sensor_msgs/CompressedImage` topic (`image_raw/compressed`) through `image_transport`. The camera already compresses the frame, so there is no need to decode it.
- ISO, shutter speed and aperture are ROS2 parameters (`iso`, `shutter_speed`, `aperture`). An `on_set_parameters` callback writes them to the camera between two preview frames with `gp_camera_set_single_config()`. We can then change them with `ros2 param set` or `rqt_reconfigure`.
- A `std_srvs/Trigger` service takes a full resolution picture.
- The Bosch `photo` package (BSD) is a good starting point for the C++ wrapper around libgphoto2.
- In `package.xml`, the rosdep key for the library is `libgphoto-dev` (without the `2`), which installs `libgphoto2-dev`. Ubuntu 24.04 ships libgphoto2 2.5.31.

If we prefer Python, `python3-gphoto2` is also in Ubuntu 24.04, but it is an old version (1.9.0). The current one (2.6.x) must be installed with `pip`.

> [!IMPORTANT]
> The 5D Mark II has a physical mode dial that we cannot change over USB. To set both shutter speed and aperture, the dial must be on `M`. In `Av` we can only set aperture, in `Tv` only shutter speed. The lens must be in a position where the camera controls the aperture, so manual lenses do not work.

Other things to take care of on the camera:

- Enable live view in the camera menu (_Live View/Movie func. set_), otherwise the preview does not start.
- Turn off _Auto power off_, otherwise the camera goes to sleep and the node loses the connection.
- Use the AC adapter: live view drains the battery quickly.

## Try the Camera on Ubuntu

Before writing any code, we can check that everything works with the `gphoto2` command line tool.

```bash
sudo apt install gphoto2
```

Ubuntu desktop mounts the camera automatically as a file system and then `gphoto2` fails with _Could not claim the USB device_. Unmount it first.

```bash
gio mount -s gphoto2
```

Check that the camera is detected and list its settings:

```bash
gphoto2 --auto-detect
gphoto2 --list-config
```

Read and change the three parameters we care about. Use `--get-config` to see the allowed values for each of them.

```bash
gphoto2 --get-config iso
gphoto2 --set-config iso=400
gphoto2 --set-config shutterspeed=1/125
gphoto2 --set-config aperture=8
```

Grab a single live view frame, then a full resolution picture:

```bash
gphoto2 --capture-preview
gphoto2 --capture-image-and-download
```

Stream the live view in a window for a few seconds:

```bash
gphoto2 --stdout --capture-movie | ffplay -
```

## Sources

- [libgphoto2 supported cameras](http://www.gphoto.org/proj/libgphoto2/support.php) and [camlibs/ptp2/library.c](https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/library.c): `Canon:EOS 5D Mark II` with `PTP_CAP_PREVIEW`.
- [gPhoto remote control documentation](http://www.gphoto.org/doc/remote/)
- [python-gphoto2](https://github.com/jim-easterbrook/python-gphoto2)
- [Canon EDSDK release notes](https://asia.canon/en/campaign/developerresources/camera/cap/edsdk-eos-digital-camera-sdk-release-note)
- [Canon press release: EDSDK extends Linux compatibility](https://www.canon-europe.com/press-centre/press-releases/2024/02/canons-updated-software-development-kit-extends-linux-compatibility/)
- [v4l2loopback wiki: gPhoto2](https://github.com/umlaeute/v4l2loopback/wiki/gPhoto2)
- [Using a Canon DSLR as a webcam with Debian/Ubuntu](https://maximevaillancourt.com/blog/canon-dslr-webcam-debian-ubuntu)
- [Robotics Stack Exchange: Still/DSLR camera with ROS](https://answers.ros.org/question/186941/stilldslr-camera-with-ros/)
- [rosdep base.yaml](https://github.com/ros/rosdistro/blob/master/rosdep/base.yaml)
