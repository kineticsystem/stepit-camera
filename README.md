# StepIt Camera

## Table of Contents <!-- omit in toc -->

- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
- [Install StepIt Camera](#install-stepit-camera)
  - [Check out the Git Repository](#check-out-the-git-repository)
  - [Pre-Commit Hooks](#pre-commit-hooks)
  - [Build the Project](#build-the-project)
- [Prepare the Camera](#prepare-the-camera)
- [Running the Application](#running-the-application)
  - [Streaming](#streaming)
  - [Pictures](#pictures)
  - [Exposure](#exposure)
- [Parameters](#parameters)
- [Troubleshooting](#troubleshooting)
- [Adding the Camera to the Workbench](#adding-the-camera-to-the-workbench)

## Introduction

StepIt Camera is a ROS2 driver for a DSLR or mirrorless camera connected over USB. It does three things:

- it streams the live view of the camera, on request;
- it downloads every picture the camera takes, saves it, and publishes it;
- it sets the exposure: ISO, shutter speed, aperture and exposure compensation.

The driver does not take pictures itself: an external precision device fires the camera through the remote shutter release cable. The driver notices each picture as soon as the camera reports it, and downloads it.

The camera is driven through [libgphoto2](http://www.gphoto.org/proj/libgphoto2/), so it works with the cameras that libgphoto2 [supports](http://www.gphoto.org/proj/libgphoto2/support.php) with _Liveview_ and _Configuration_, which includes most Canon EOS cameras. On Nikon and Sony cameras, libgphoto2 names the aperture `f-number` rather than `aperture`, so the driver cannot set their aperture yet. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how it is built.

## Prerequisites

To run StepIt Camera, we need a computer with Ubuntu 24.04 and Docker. Please refer to the document [Install Docker Engine on Ubuntu](https://docs.docker.com/engine/install/ubuntu/), or run:

```
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
```

We do not need a camera to try it out: the driver can run a fake camera, which streams a test pattern and takes pictures on request.

For a real application, we need:

- 1 x camera supported by libgphoto2 with _Liveview_ and _Configuration_.
- 1 x USB cable to connect the camera to the computer.
- 1 x AC adapter for the camera, because the live view drains the battery quickly.
- The external device triggering the camera through its remote shutter release socket.

## Install StepIt Camera

### Check out the Git Repository

```
git clone git@github.com:kineticsystem/stepit-camera.git
cd stepit-camera
```

### Pre-Commit Hooks

Additionally, you should install git pre-commit hooks using the following command:

```
pre-commit install
```

The ament linters used by the hooks only exist in the container. On a host without ROS, skip them with `SKIP=ament_copyright,ament_lint_cmake,ament_cpplint git commit ...`.

### Build the Project

StepIt Camera is built and run inside a Docker container. It is defined in [`docker/docker-compose.yml`](docker/docker-compose.yml) and driven by the [`docker/dock.sh`](docker/dock.sh) script. See [docker/README.md](docker/README.md) for more details.

> [!IMPORTANT]
> The docker container provides a default user `developer` with password `developer`. That user may run `sudo` without being asked for it, so that the scripts in `bin` also work from a non-interactive shell, e.g.
> `docker exec stepit-camera update.sh`.

Build the image and create the container. The script always mounts the repo it belongs to, so it can be called from anywhere:

```
./docker/dock.sh stepit-camera build
```

This is also how you pick up a change to the `Dockerfile`: it rebuilds only the layers that changed, so there is no need to clean first.

Start the container with an interactive shell:

```
./docker/dock.sh stepit-camera start
```

The commands below assume you are inside the container. The scripts in [`bin`](bin) are on the `PATH` and aliased, so they can be called from any directory: they always act on the workspace root.

Install all required dependencies.

```
update
```

Run Colcon to build the project.

```
build
```

Execute all tests. They run against the fake camera, so they need no hardware.

```
test
```

## Prepare the Camera

The driver can set the exposure only as far as the camera allows it. The modes below are named as on a Canon; Nikon and Sony call Av and Tv A and S.

- Turn the mode dial to **M**. On most cameras the mode dial is mechanical and cannot be changed over USB. On M, we can set the ISO, the shutter speed and the aperture. On Av, only the aperture; on Tv, only the shutter speed.
- Use a lens whose aperture the camera controls. A manual lens has no aperture setting.
- Enable the live view for stills in the camera's menu, if it has such a setting. Without it, the camera may refuse to stream.
- Set _Auto power off_ to _Off_. Otherwise the camera goes to sleep and the driver loses it until it wakes up.
- Plug the external device into the remote shutter release socket, and the USB cable into the computer.

## Running the Application

Start the driver with the fake camera:

```
source ~/ws/install/setup.bash
ros2 launch stepit_camera camera.launch.py fake:=true
```

Start it with the real camera. It waits for the camera to be switched on, and it reconnects if the camera is switched off or unplugged:

```
ros2 launch stepit_camera camera.launch.py
```

Add `stream:=true` to start the live view as soon as the camera connects.

Open a different terminal, attached to the same container with `./docker/dock.sh stepit-camera start`, to run the commands below.

### Streaming

Start and stop the live view:

```
ros2 service call /camera/start_streaming std_srvs/srv/Trigger
ros2 service call /camera/stop_streaming std_srvs/srv/Trigger
```

The frames are published as JPEG images on `/camera/preview/compressed` (`sensor_msgs/msg/CompressedImage`), exactly as the camera sends them, with no decoding. Their size depends on the camera. Watch them with:

```
ros2 run rqt_image_view rqt_image_view /camera/preview/compressed
```

Stopping the live view lowers the mirror again, so that the camera is ready to shoot through the viewfinder.

### Pictures

Every picture the camera takes is downloaded straight away, saved into the folder [`pictures`](pictures) of this repo, and published on `/camera/picture` (`stepit_camera_msgs/msg/Picture`) with its content. A shot in RAW+JPEG produces two files, and two messages.

```
ros2 topic echo /camera/picture --field path
```

With the fake camera, take a picture as the external device would:

```
ros2 service call /camera/simulate_shot std_srvs/srv/Trigger
```

A picture never overwrites another one: if a file of the same name already exists, e.g. because the camera started numbering from `IMG_0001` again, it gets a suffix, e.g. `IMG_0001_1.JPG`.

> [!IMPORTANT]
> The stamp of a picture is the time it was downloaded, a fraction of a second after the shot, not the time of the shot. For a precise time, use the time the external device fired the camera.

### Exposure

The exposure is set with ROS parameters. The values are those the camera shows:

```
ros2 param set /camera iso 400
ros2 param set /camera shutter_speed 1/125
ros2 param set /camera aperture 5.6
ros2 param set /camera exposure_compensation -0.3
```

A value the camera does not accept is rejected, with the values it does accept:

```
$ ros2 param set /camera iso 123
Setting parameter failed: The camera does not accept iso 123, only one of: Auto, 100, 125, 160, ...
```

Read the current settings, and the values each one accepts right now:

```
ros2 service call /camera/get_settings stepit_camera_msgs/srv/GetSettings
```

The values in [`camera.yaml`](src/stepit_camera/config/camera.yaml) are applied every time the camera connects. An empty value leaves the camera as it is.

## Parameters

| Parameter | Default | Description |
|---|---|---|
| `iso` | `""` | The ISO, e.g. `400` or `Auto`. |
| `shutter_speed` | `""` | The shutter speed, e.g. `1/125` or `2`. |
| `aperture` | `""` | The aperture, e.g. `8` or `5.6`. |
| `exposure_compensation` | `""` | The exposure compensation in stops, e.g. `-1` or `0.3`. It has no effect on M, unless the ISO is Auto. |
| `download_directory` | `~/ws/pictures` | Where to save the pictures. Empty not to save them. |
| `keep_on_camera` | `true` | Store the pictures on the memory card too. When `false`, they only go to the camera's memory and are deleted once downloaded: the card never fills up, but a picture that cannot be downloaded is lost. |
| `stream_on_start` | `false` | Start the live view as soon as the camera connects. Set by the launch argument `stream`. |
| `preview_rate` | `10.0` | The frames of the live view per second, at most. |
| `reconnect_period` | `2.0` | How long to wait, in seconds, before trying to open the camera again. |
| `frame_id` | `camera` | The frame of the images. |
| `fake_camera` | `false` | Run a fake camera. Set by the launch argument `fake`. |

Only the four settings of the exposure can change while the driver runs; the others are read once, at start.

## Troubleshooting

**The camera is never found.** Check that the host sees it:

```
lsusb
```

Then check, inside the container, that libgphoto2 can open it. Stop the driver first: only one program at a time can use the camera.

```
gphoto2 --auto-detect
gphoto2 --summary
```

If `gphoto2` says _Could not claim the USB device_, the host desktop has mounted the camera as a file system. Unmount it:

```
gio mount -s gphoto2
```

To stop Ubuntu from doing it again, on the host:

```
systemctl --user mask --now gvfs-gphoto2-volume-monitor.service
```

**A setting is rejected with no choices, or with fewer than expected.** The mode dial or the lens does not allow it, see [Prepare the Camera](#prepare-the-camera).

**The live view does not start.** Check that it is enabled in the camera's menu, and look for warnings in the output of the driver.

## Adding the Camera to the Workbench

The container follows the other StepIt projects: its `dev` service in [`docker/docker-compose.yml`](docker/docker-compose.yml) can be extended by the [StepIt Workbench](https://github.com/kineticsystem/stepit-workbench), once this repo is a submodule under `modules/stepit-camera`:

```yaml
  stepit-camera:
    extends:
      file: ../modules/stepit-camera/docker/docker-compose.yml
      service: dev
    image: stepit-camera:latest
    container_name: stepit-camera
    hostname: stepit-camera
    init: true
    stop_signal: SIGINT
    command:
      - bash
      - -c
      - '{ [ -f ~/.dependencies ] || { update.sh && touch ~/.dependencies; }; }
        && source /opt/ros/jazzy/setup.bash
        && source install/setup.bash
        && exec ros2 launch stepit_camera camera.launch.py'
```

Then add `stepit-camera` to `SERVICES` in the workbench's `dock.sh`.
