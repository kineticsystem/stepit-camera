# StepIt Camera

[![CI](https://github.com/kineticsystem/stepit-camera/actions/workflows/industrial_ci.yml/badge.svg)](https://github.com/kineticsystem/stepit-camera/actions/workflows/industrial_ci.yml)
[![Format](https://github.com/kineticsystem/stepit-camera/actions/workflows/ci-format.yml/badge.svg)](https://github.com/kineticsystem/stepit-camera/actions/workflows/ci-format.yml)
[![Linters](https://github.com/kineticsystem/stepit-camera/actions/workflows/ci-ros-lint.yml/badge.svg)](https://github.com/kineticsystem/stepit-camera/actions/workflows/ci-ros-lint.yml)
[![Test page](https://github.com/kineticsystem/stepit-camera/actions/workflows/web.yml/badge.svg)](https://github.com/kineticsystem/stepit-camera/actions/workflows/web.yml)

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
  - [The Web Server's API](#the-web-servers-api)
  - [Exposure](#exposure)
  - [Status](#status)
- [The Test Page](#the-test-page)
- [Parameters](#parameters)
- [Troubleshooting](#troubleshooting)
- [Running the Driver from Another Project](#running-the-driver-from-another-project)
- [Continuous Integration](#continuous-integration)

## Introduction

StepIt Camera is a ROS2 driver for a DSLR or mirrorless camera connected over USB. It does three things:

- it streams the live view of the camera, on request;
- it downloads every picture the camera takes, saves it, and publishes it;
- it sets the exposure: ISO, shutter speed, aperture and exposure compensation, and the white balance.

It comes with a test page, a web page that shows the live view, sets the exposure and takes test shots, to try the camera and the driver from a browser: see [The Test Page](#the-test-page).

The driver does not decide when to take a picture: whatever fires the camera, e.g. a device plugged into its remote shutter release socket, the driver notices each picture as soon as the camera reports it, and downloads it. A shot fired by a device keeps the timing of that device, which a command over USB cannot match. The one exception is a test shot, which the driver fires over USB on request, e.g. from the test page, to check the framing and the exposure.

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
- Optionally, a device that fires the camera through its remote shutter release socket, e.g. a remote or an intervalometer.

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
- Plug the USB cable into the computer, and the device that fires the camera, if any, into the remote shutter release socket.

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

- the web server of this package on port 8090, which serves the test page, <http://localhost:8090>, the saved pictures, e.g. <http://localhost:8090/pictures/IMG_0001.JPG>, and what the folder of pictures holds, see [The Web Server's API](#the-web-servers-api);
- [web_video_server](https://github.com/RobotWebTools/web_video_server) on port 8081, which streams the live view to a browser, e.g. <http://localhost:8081/stream?topic=/camera/preview&type=ros_compressed>;
- [rosbridge](https://github.com/RobotWebTools/rosbridge_suite) on port 9091, which lets a browser call the services, set the parameters and hear of the pictures. It is not on rosbridge's default port, 9090, so that it can run next to another rosbridge: a rosbridge only knows the messages installed next to it, and only this one knows the driver's messages, `stepit_camera_msgs`.

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

Every picture the camera takes is downloaded straight away, saved into the folder [`pictures`](pictures) of this repo, and published on `/camera/picture` (`stepit_camera_msgs/msg/Picture`). The message says where the file is, not what is in it: a node reads the file at `path`, and a web page loads it from the web server, at `/pictures/<relative_path>`, the file's path under `download_directory`. A RAW file of a 5D Mark II is about 30 MB, which would weigh on DDS and on every node sharing it, and which rosbridge would send to a browser as 40 MB of base64 text.

A shot in RAW+JPEG produces two files, and two messages.

```
ros2 topic echo /camera/picture --field path
```

Take a test shot, over USB. The picture comes like any other, on `/camera/picture`. This works with the fake camera too, which then takes a picture as a real camera would:

```
ros2 service call /camera/take_picture std_srvs/srv/Trigger
```

The live view pauses for the shot, for a second or so, and goes on by itself.

A picture never overwrites another one: if a file of the same name already exists, e.g. because the camera started numbering from `IMG_0001` again, it gets a suffix, e.g. `IMG_0001_1.JPG`.

The pictures can go into a subfolder of `download_directory`, which the parameter `folder` names and which can change while the driver runs, e.g. one folder per series of shots. The folder is created with the first picture in it. `relative_path` in the message then holds the folder too, e.g. `2026-10-06/angle_01/IMG_0042.CR2`:

```
ros2 param set /camera folder 2026-10-06/angle_01
ros2 param set /camera folder ""
```

A folder outside `download_directory`, absolute or with `..`, is refused: the web server only serves `download_directory`.

> [!IMPORTANT]
> The stamp of a picture is the time it was downloaded, a fraction of a second after the shot, not the time of the shot. For a precise time, use the time the camera was fired, e.g. as recorded by the device that fired it.

### The Web Server's API

The web server's API is described in OpenAPI, and shown on a Swagger page, to read and to try each request from the browser:

- <http://localhost:8090/docs>: the Swagger page. The browser loads Swagger itself from cdn.jsdelivr.net, so it needs the internet; nothing is installed for it.
- <http://localhost:8090/openapi.json>: the description, for other tools, e.g. a client generator. It is [`src/stepit_camera/src/openapi.json`](src/stepit_camera/src/openapi.json), compiled into the server.

| Request | What it gives |
|---|---|
| `GET /` | The test page, once it is built. |
| `GET /pictures/<path>` | A picture, at its path in `download_directory`. A `Range` header reads only a part of it. |
| `GET /pictures/` | What `download_directory` holds, as JSON. |
| `GET /pictures/<folder>/` | What a folder of it holds, as JSON. The path ends with a slash: without it, it names a file. |
| `GET /openapi.json`, `GET /docs` | The description of the API, and its Swagger page. |

A folder's listing gives its folders, and its files with their sizes, in bytes, and when they last changed, in UTC, all sorted by path:

```bash
curl http://localhost:8090/pictures/2026-10-06/
```

```json
{
  "folder": "2026-10-06",
  "folders": ["series_01"],
  "files": [
    { "path": "IMG_0042.CR2", "size": 25843210, "modified": "2026-10-06T15:20:04Z" }
  ]
}
```

`?recursive=1` lists everything below the folder in one answer, the paths relative to it, e.g. every picture the camera saved, with `/pictures/?recursive=1`. Hidden files and folders, whose name starts with a dot, are left out, e.g. a file a program is still writing under a temporary name. A folder outside `download_directory`, with `..` or through a link, is not found, as a file there is not.

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

### Status

The driver tells whether it talks to a camera on `/camera/status` (`stepit_camera_msgs/msg/CameraStatus`): `connected`, the camera's model in `device`, and in `message` why it is not connected, e.g. the error with which the camera was lost. The topic keeps its last message for a client that subscribes late, and comes again every second, so that a client that stops receiving it knows the driver has stopped:

```
ros2 topic echo /camera/status --qos-durability transient_local
```

## The Test Page

The test page, in [`web`](web), is a web page to try the camera and the driver from a browser. It is served by the driver's web server, on <http://localhost:8090>, once `build` has built it:

- **Live view** (left): what the camera sees, about 10 frames per second. It is off when the page opens: **Start** switches it on, and **Stop** switches it off again, which lowers the mirror. Once started, it starts again by itself if the driver restarts.
- **Settings** (right): the ISO, the shutter speed, the aperture, the white balance and the exposure compensation, each with the values the camera accepts right now. These depend on the mode dial and on the lens, see [Prepare the Camera](#prepare-the-camera); a setting the camera does not let us change is greyed out. A change made on the camera itself shows up within a few seconds.
- **Test shot**: releases the shutter and shows the picture, once saved. A JPEG is shown as it is; a RAW file is shown through the JPEG preview it carries, which the page reads from the file without loading the rest of it. The name of the file downloads it whole. The live view pauses for the shot, since the mirror moves: the last frame stays, greyed out, until the picture has come.

The gear at the top right holds the preferences of the browser: the theme, where the camera's rosbridge and web_video_server are, by default on the machine that serves the page, and the name of the camera node.

For working on the page itself, `dev` runs the Vite development server with hot reload, on <http://localhost:5174>, next to the driver. See [docs/WEB_PAGE.md](docs/WEB_PAGE.md) for how the page is built.

Other web pages can use the same servers. The web server lets pages of any origin load the pictures, e.g. a page served by another application.

## Parameters

| Parameter | Default | Description |
|---|---|---|
| `iso` | `""` | The ISO, e.g. `400` or `Auto`. |
| `shutter_speed` | `""` | The shutter speed, e.g. `1/125` or `2`. |
| `aperture` | `""` | The aperture, e.g. `8` or `5.6`. |
| `exposure_compensation` | `""` | The exposure compensation in stops, e.g. `-1` or `0.3`. It has no effect on M, unless the ISO is Auto. |
| `white_balance` | `""` | The white balance, e.g. `Auto`, `Daylight` or `Cloudy`. |
| `download_directory` | `~/ws/pictures` | Where to save the pictures, and where the web server finds them. It cannot be empty. |
| `folder` | `""` | The subfolder of `download_directory` the next pictures go into, e.g. `2026-10-06/angle_01`; empty for `download_directory` itself. It can change while the driver runs. |
| `keep_on_camera` | `true` | Store the pictures on the memory card too. When `false`, they only go to the camera's memory and are deleted once downloaded: the card never fills up, but a picture that cannot be downloaded is lost. |
| `stream_on_start` | `false` | Start the live view as soon as the camera connects. Set by the launch argument `stream`. |
| `preview_rate` | `10.0` | The frames of the live view per second, at most. |
| `reconnect_period` | `2.0` | How long to wait, in seconds, before trying to open the camera again. |
| `frame_id` | `camera` | The frame of the images. |
| `fake_camera` | `false` | Run a fake camera. Set by the launch argument `fake`. |

Only the five settings of the camera, from `iso` to `white_balance`, and `folder` can change while the driver runs; the others are read once, at start.

The web server, `/web_server`, reads `download_directory` from the same file, and has parameters of its own:

| Parameter | Default | Description |
|---|---|---|
| `web_directory` | `~/ws/web/dist` | The test page, as built by `build`. |
| `port` | `8090` | The HTTP port. Set by the launch argument `web_port`. |
| `host` | `0.0.0.0` | The address to listen on. |

| Launch argument | Default | Description |
|---|---|---|
| `params_file` | `camera.yaml` | A parameter file loaded after `camera.yaml`, by the camera and by the web server, e.g. by a robot that runs the camera with values of its own. The launch arguments `fake`, `stream` and `web_port` still win over it. |
| `fake` | `false` | Run a fake camera. |
| `stream` | `false` | Start the live view as soon as the camera connects. |
| `web` | `true` | Start the web server, for the test page and the pictures. |
| `web_port` | `8090` | The HTTP port of the web server. |
| `web_video` | `true` | Start web_video_server, to show the live view in web pages. |
| `web_video_port` | `8081` | The HTTP port of web_video_server. |
| `rosbridge` | `true` | Start rosbridge, for web pages to use the driver. |
| `rosbridge_port` | `9091` | The WebSocket port of rosbridge. |

## Troubleshooting

**The camera is never found**, the driver keeps _Waiting for a camera_, and `/camera/status` says it is not connected. Check that the host sees it:

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

## Running the Driver from Another Project

Another project can run the driver in its own Docker Compose file, without copying the container's definition: its service extends the `dev` service of [`docker/docker-compose.yml`](docker/docker-compose.yml), so the Dockerfile, the mounts and the network settings stay defined here. It only replaces the command, to start the driver rather than an idle container. For example, with this repo checked out in `modules/stepit-camera`:

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

The code has to be built in that container first, with `update.sh` and `build.sh`. Run one container or the other, not both, since both open the camera: remove the container made by this repo's `dock.sh` first, e.g. with `./docker/dock.sh stepit-camera clean`, and the other way round.

## Continuous Integration

Four GitHub Actions workflows run on every push and pull request, three of them as in StepIt Motors:

| Workflow | What it checks |
|---|---|
| [`industrial_ci.yml`](.github/workflows/industrial_ci.yml) | Builds and tests the packages with [Industrial CI](https://github.com/ros-industrial/industrial_ci), against the main and the testing ROS repositories. The tests use the fake camera: no camera is needed. |
| [`ci-format.yml`](.github/workflows/ci-format.yml) | The pre-commit hooks that need no ROS: clang-format, black, codespell, and the checks of whitespace and files. |
| [`ci-ros-lint.yml`](.github/workflows/ci-ros-lint.yml) | The ament linters of every package: copyright, lint_cmake and cpplint. |
| [`web.yml`](.github/workflows/web.yml) | The test page: type checks, tests and build, when `web` changes. |

The workflows run locally with [Nektos `act`](https://github.com/nektos/act), which reads the variables of [`.env`](.env), from a clean checkout: Industrial CI mounts the working tree, `build` and `install` included. See [How to run GitHub Actions locally](https://github.com/kineticsystem/stepit-motors#how-to-run-github-actions-locally) in the README of StepIt Motors.
