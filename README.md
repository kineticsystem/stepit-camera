# StepIt Macro

## Table of Contents <!-- omit in toc -->

- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
- [Install the StepIt Macro](#install-the-stepit-macro)
  - [Check out the Git Repository](#check-out-the-git-repository)
  - [Build the Project](#build-the-project)
- [Running the Application](#running-the-application)
- [Using the UI](#using-the-ui)
  - [The Camera](#the-camera)
  - [Settings](#settings)
- [Troubleshooting](#troubleshooting)
- [Project Layout](#project-layout)

## Introduction

The StepIt Macro is the web control panel of the StepIt robot. It has one section per part of the robot. Today there is one, the **Camera**:

- it shows what the camera sees, live;
- it sets the ISO, the shutter speed, the aperture and the white balance;
- it takes a test shot, and shows it.

The macro rails, driven through [StepIt Commander](https://github.com/kineticsystem/stepit-commander), will come as another section.

The UI talks to the robot straight from the browser, so it needs no ROS itself:

- through [rosbridge](https://github.com/RobotWebTools/rosbridge_suite), a WebSocket that speaks JSON, to call the services, set the parameters and receive the pictures;
- through [web_video_server](https://github.com/RobotWebTools/web_video_server), which streams the live view as MJPEG into a plain `<img>`.

Both run next to the camera driver, [StepIt Camera](https://github.com/kineticsystem/stepit-camera), whose launch file starts them.

## Prerequisites

We need a computer with Ubuntu 24.04. The preferred way to run the UI is inside a Docker container. Please refer to the document [Install Docker Engine on Ubuntu](https://docs.docker.com/engine/install/ubuntu/).

If you want to run the UI on your host machine instead, you must install [Node.js](https://nodejs.org/) 24 and enable pnpm with `corepack enable`.

To see and control a camera, [StepIt Camera](https://github.com/kineticsystem/stepit-camera) must run, with a real camera or its fake one.

## Install the StepIt Macro

### Check out the Git Repository

```
git clone git@github.com:kineticsystem/stepit-macro.git
```

### Build the Project

The Docker container is defined in [`docker/docker-compose.yml`](docker/docker-compose.yml) and driven by the [`docker/dock.sh`](docker/dock.sh) script. See [docker/README.md](docker/README.md) for more details.

> [!IMPORTANT]
> The docker container provides a default user `developer` with password `developer`.

Build the image and create the container:

```
./docker/dock.sh stepit-macro build
```

Start the container with an interactive shell:

```
./docker/dock.sh stepit-macro start
```

Inside the container the scripts in [`bin`](bin) are on the `PATH` and aliased, so they can be called from any directory. Outside the container, call them by their path instead, e.g. `./bin/update.sh`.

Install all required dependencies.

```
update
```

Build the UI.

```
build
```

Execute all tests.

```
test
```

## Running the Application

Start the camera driver first, in the StepIt Camera container. Add `fake:=true` to try the UI without a camera:

```
ros2 launch stepit_camera camera.launch.py
```

Then, inside the UI container, start the UI and open <http://localhost:8090> in a browser.

```
serve
```

From outside the container, the same can be done in one step: this starts the container, installs the dependencies, builds and serves the UI. Stop it with `Ctrl+C`.

```
./docker/dock.sh stepit-macro serve
```

To use another port, choose it when starting the container, e.g. `MACRO_PORT=9000 ./docker/dock.sh stepit-macro serve`. For working on the UI itself, `dev` runs the Vite development server with hot reload, on <http://localhost:5174>.

> [!IMPORTANT]
> The server listens on all network interfaces, so anyone on the network can open it, and control the camera.

## Using the UI

### The Camera

- **Live view** (left): what the camera sees, about 10 frames per second. **Stop** switches the live view off, which lowers the mirror; **Start** switches it on again. The UI remembers the choice, and starts the live view when it opens.
- **Settings** (right): the ISO, the shutter speed, the aperture, the white balance and the exposure compensation, each with the values the camera accepts right now. These depend on the mode dial and on the lens: on M, all of them can change, except the exposure compensation; a setting the camera does not let us change is greyed out. A change made on the camera itself shows up within a few seconds.
- **Test shot**: releases the shutter and shows the picture, once downloaded. A JPEG is shown as it is; a RAW file is shown through the JPEG preview it carries. The picture is also saved by the driver, in the `pictures` folder of StepIt Camera, whose path is shown below it. The live view pauses for the shot, since the mirror moves: the last frame stays, greyed out, until the picture has come.

A RAW file of a 5D Mark II is about 29 MB, which takes a few seconds to reach the browser.

### Settings

The gear at the top right holds the preferences of this browser: the theme, and where the camera's servers are. By default, the UI looks for them on the machine that serves it:

| Setting | Default | What it is |
|---|---|---|
| rosbridge | `ws://<host>:9091` | The camera's rosbridge. |
| Video server | `http://<host>:8081` | The camera's web_video_server. |
| Camera node | `/camera` | The name of the camera driver's node. |

The status at the top right tells whether the UI reaches the rosbridge of the open section.

## Troubleshooting

**The status stays _Disconnected_.** The camera's rosbridge is not running, or not where the settings say. Check that the camera driver runs, with its rosbridge (the launch argument `rosbridge`, on by default), and that nothing else took port 9091.

**The settings say _The camera is not connected_.** The driver runs, but has no camera: it is off, asleep or unplugged. Set _Auto power off_ to _Off_ in the camera's menu. See the troubleshooting of StepIt Camera.

**The live view says _Cannot reach web_video_server_.** Check that the camera driver was started with `web_video:=true`, the default, and the _Video server_ setting.

**The live view is black.** The live view is on, but no frame comes: look at the output of the camera driver.

## Project Layout

To learn how the UI is built, start with [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
bin/               update, build, serve, dev and test scripts
docker/            the container
src/client/ros/    the rosbridge client, and the connections the sections share
src/client/camera/ the camera section: its ROS interface, its state, its formatting
src/client/components/ the React components
src/server/        the production server of the built UI
tests/             unit tests (vitest)
docs/              the architecture document
```
