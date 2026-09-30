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
- [The Test Page](#the-test-page)
- [Parameters](#parameters)
- [Troubleshooting](#troubleshooting)
- [Running the Camera with the Whole Rig](#running-the-camera-with-the-whole-rig)

## Introduction

StepIt Camera is a ROS2 driver for a DSLR or mirrorless camera connected over USB. It does three things:

- it streams the live view of the camera, on request;
- it downloads every picture the camera takes, saves it, and publishes it;
- it sets the exposure: ISO, shutter speed, aperture and exposure compensation, and the white balance.

It comes with a test page, a web page that shows the live view, sets the exposure and takes test shots, to try the camera and the driver from a browser. It is not the application of the rig: see [The Test Page](#the-test-page).

The driver does not take pictures itself: an external precision device fires the camera through the remote shutter release cable. The driver notices each picture as soon as the camera reports it, and downloads it. The one exception is a test shot, which the driver fires over USB on request, e.g. from the test page, to check the framing and the exposure.

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

Build the project: the driver with Colcon, and the test page with pnpm, into `web/dist`.

```
build
```

Execute all tests, of the driver and of the test page. The driver's run against the fake camera, so they need no hardware.

```
test
```

## Prepare the Camera

> [!IMPORTANT]
> **Set _Auto power off_ to _Off_ in the camera's menu.** Otherwise the camera goes to sleep, as soon as a minute after the last use, and the driver loses it until it wakes up: the live view stops, and no picture is downloaded.

The driver can set the exposure only as far as the camera allows it. The modes below are named as on a Canon; Nikon and Sony call Av and Tv A and S.

- Turn the mode dial to **M**. On most cameras the mode dial is mechanical and cannot be changed over USB. On M, we can set the ISO, the shutter speed and the aperture. On Av, only the aperture; on Tv, only the shutter speed.
- Use a lens whose aperture the camera controls. A manual lens has no aperture setting.
- Enable the live view for stills in the camera's menu, if it has such a setting. Without it, the camera may refuse to stream.
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

The launch file also starts three servers for web pages, such as the [test page](#the-test-page):

- the web server of this package on port 8090, which serves the test page, <http://localhost:8090>, and the saved pictures, e.g. <http://localhost:8090/pictures/IMG_0001.JPG>;
- [web_video_server](https://github.com/RobotWebTools/web_video_server) on port 8081, which streams the live view to a browser, e.g. <http://localhost:8081/stream?topic=/camera/preview&type=ros_compressed>;
- [rosbridge](https://github.com/RobotWebTools/rosbridge_suite) on port 9091, which lets a browser call the services, set the parameters and hear of the pictures. It is not on port 9090, which belongs to the rosbridge of StepIt Commander: only a rosbridge running next to this driver knows its messages, `stepit_camera_msgs`.

Leave them out with `web:=false`, `web_video:=false` and `rosbridge:=false`, or move them with `web_port:=<port>`, `web_video_port:=<port>` and `rosbridge_port:=<port>`.

> [!IMPORTANT]
> The servers listen on all network interfaces, so anyone on the network can open the test page, control the camera and download the pictures.

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

Every picture the camera takes is downloaded straight away, saved into the folder [`pictures`](pictures) of this repo, and published on two topics, both `stepit_camera_msgs/msg/Picture`:

- `/camera/picture`, with its content, for a node that cannot read the file, e.g. on another computer;
- `/camera/saved_picture`, once saved, without its content: a node on the same computer reads the file at `path`, and a web page loads it from the web server, at `/pictures/<file name>`. A RAW file of a 5D Mark II is about 30 MB, too much for rosbridge, which would send it as 40 MB of base64 text.

A shot in RAW+JPEG produces two files, and two messages on each topic.

```
ros2 topic echo /camera/saved_picture --field path
```

Take a test shot, over USB. The picture comes like any other, on `/camera/picture` and `/camera/saved_picture`. This works with the fake camera too, which then takes a picture as the external device would:

```
ros2 service call /camera/take_picture std_srvs/srv/Trigger
```

The live view pauses for the shot, for a second or so, and goes on by itself.

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
ros2 param set /camera white_balance Daylight
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

## The Test Page

The test page, in [`web`](web), is a web page to try the camera and the driver from a browser. It is served by the driver's web server, on <http://localhost:8090>, once `build` has built it:

- **Live view** (left): what the camera sees, about 10 frames per second. **Stop** switches the live view off, which lowers the mirror; **Start** switches it on again. The page remembers the choice, and starts the live view when it opens.
- **Settings** (right): the ISO, the shutter speed, the aperture, the white balance and the exposure compensation, each with the values the camera accepts right now. These depend on the mode dial and on the lens, see [Prepare the Camera](#prepare-the-camera); a setting the camera does not let us change is greyed out. A change made on the camera itself shows up within a few seconds.
- **Test shot**: releases the shutter and shows the picture, once saved. A JPEG is shown as it is; a RAW file is shown through the JPEG preview it carries, which the page reads from the file without loading the rest of it. The name of the file downloads it whole. The live view pauses for the shot, since the mirror moves: the last frame stays, greyed out, until the picture has come.

The gear at the top right holds the preferences of the browser: the theme, where the camera's rosbridge and web_video_server are, by default on the machine that serves the page, and the name of the camera node.

For working on the page itself, `dev` runs the Vite development server with hot reload, on <http://localhost:5174>, next to the driver. See [docs/WEB_PAGE.md](docs/WEB_PAGE.md) for how the page is built.

The test page only controls the camera. The application of the whole rig, with the rails, the rotary stage and the lights, is [StepIt UI](https://github.com/kineticsystem/stepit-ui); it can load the pictures from the same web server.

## Parameters

| Parameter | Default | Description |
|---|---|---|
| `iso` | `""` | The ISO, e.g. `400` or `Auto`. |
| `shutter_speed` | `""` | The shutter speed, e.g. `1/125` or `2`. |
| `aperture` | `""` | The aperture, e.g. `8` or `5.6`. |
| `exposure_compensation` | `""` | The exposure compensation in stops, e.g. `-1` or `0.3`. It has no effect on M, unless the ISO is Auto. |
| `white_balance` | `""` | The white balance, e.g. `Auto`, `Daylight` or `Cloudy`. |
| `download_directory` | `~/ws/pictures` | Where to save the pictures, and where the web server finds them. Empty not to save them. |
| `keep_on_camera` | `true` | Store the pictures on the memory card too. When `false`, they only go to the camera's memory and are deleted once downloaded: the card never fills up, but a picture that cannot be downloaded is lost. |
| `stream_on_start` | `false` | Start the live view as soon as the camera connects. Set by the launch argument `stream`. |
| `preview_rate` | `10.0` | The frames of the live view per second, at most. |
| `reconnect_period` | `2.0` | How long to wait, in seconds, before trying to open the camera again. |
| `frame_id` | `camera` | The frame of the images. |
| `fake_camera` | `false` | Run a fake camera. Set by the launch argument `fake`. |

Only the five settings of the camera, from `iso` to `white_balance`, can change while the driver runs; the others are read once, at start.

The web server, `/web_server`, reads `download_directory` from the same file, and has parameters of its own:

| Parameter | Default | Description |
|---|---|---|
| `web_directory` | `~/ws/web/dist` | The test page, as built by `build`. |
| `port` | `8090` | The HTTP port. Set by the launch argument `web_port`. |
| `host` | `0.0.0.0` | The address to listen on. |

| Launch argument | Default | Description |
|---|---|---|
| `fake` | `false` | Run a fake camera. |
| `stream` | `false` | Start the live view as soon as the camera connects. |
| `web` | `true` | Start the web server, for the test page and the pictures. |
| `web_port` | `8090` | The HTTP port of the web server. |
| `web_video` | `true` | Start web_video_server, to show the live view in web pages. |
| `web_video_port` | `8081` | The HTTP port of web_video_server. |
| `rosbridge` | `true` | Start rosbridge, for web pages to use the driver. |
| `rosbridge_port` | `9091` | The WebSocket port of rosbridge. |

## Troubleshooting

**The camera is never found**, and the driver keeps _Waiting for a camera_. Check that the host sees it:

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

**The shutter button on the camera does nothing.** A 5D Mark II ignores its own shutter button while the live view runs over USB: stop the live view, or take a test shot. Whether it also ignores the remote shutter release cable during the live view is not tested yet.

**The white balance offers a single value.** Right after connecting, a Canon EOS may not have sent the list of its white balances yet, and libgphoto2 then offers the current one only. Read the settings again a moment later.

**The test page says _The test page is not built_.** Run `build`, which builds it into `web/dist`.

**A server says its port is already in use**, e.g. _Cannot listen on 0.0.0.0:8090_. Another driver is running, maybe left over from a launch that failed: stop it, or choose other ports with the launch arguments.

**The test page stays _Disconnected_.** The camera's rosbridge is not running, or not where the page's settings say. Check that nothing else took port 9091.

**The live view on the test page says _Cannot reach web_video_server_.** Check that the driver was started with `web_video:=true`, the default, and the _Video server_ setting of the page.

**The camera disconnects after a minute.** It switched itself off: set _Auto power off_ to _Off_ in the camera's menu.

## Running the Camera with the Whole Rig

The camera is a module of [StepIt Macro](https://github.com/kineticsystem/stepit-macro), which runs it together with the other parts of the focus stacking rig, under `modules/stepit-camera`. Its `stepit-camera` service extends the `dev` service of [`docker/docker-compose.yml`](docker/docker-compose.yml), so the Dockerfile, the mounts and the network settings stay defined here:

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

Run one or the other, not both: remove the container made by this repo's `dock.sh` before starting StepIt Macro, e.g. with `./docker/dock.sh stepit-camera clean`, and the other way round.
