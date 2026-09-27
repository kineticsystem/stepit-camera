# Developing inside a docker container

In this tutorial, we explain how to develop, build and run this project inside a docker container.

The container user and password are:

**developer:developer**

## Prerequisites

First, you must install `docker`.

```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
```

## Build and start up a container

The container is defined in `docker-compose.yml`; `dock.sh` is a thin wrapper that supplies the container name, the workspace to mount, the host uid/gid and the X server permission, then calls `docker compose`. You can drive compose directly if you prefer, but the wrapper is easier.

Run this script to create an image and a container. It always mounts the repo it is part of, so it can be called from anywhere:

```bash
./docker/dock.sh stepit-camera build
```

This is also how you pick up changes to the `Dockerfile`: it rebuilds only the layers that changed, so there is no need to `clean` first.

Run this to start the container with an interactive shell:

```bash
./docker/dock.sh stepit-camera start
```

Run this to stop the container:

```bash
./docker/dock.sh stepit-camera stop
```

Finally, run this to remove container and image:

```bash
./docker/dock.sh stepit-camera clean
```

## Working with the code

Inside the container, the repo is bind-mounted at `~/ws`, so every package under `~/ws/src` is a normal colcon package. `~/ws/bin` is on the `PATH` and the scripts are aliased, so `build`, `test` and `update` work from any directory (they always act on the workspace root):

```bash
update    # only once: rosdep install
build
source ~/ws/install/setup.bash
ros2 launch stepit_camera camera.launch.py fake:=true
```

Run the tests with:

```bash
test
```

The interactive shell setup -- sourcing the ROS2 environment and defining those aliases -- lives in `docker/bashrc`, which the image installs as `~/.bashrc.camera` and sources from `~/.bashrc`. Edit that file to change what a shell in the container gets; variables belong in the `Dockerfile` as `ENV` instead, so that they apply to non-interactive commands too. Either way, rebuild the container afterwards to pick the change up.

## Access to the camera

The container is privileged and mounts the host's `/dev`, so libgphoto2 finds the camera on the USB bus exactly as on the host, even after it is unplugged and plugged in again. Nothing needs installing on the host.

The one thing that can get in the way is the host itself: Ubuntu desktop mounts a camera as a file system as soon as it is plugged in, and then nothing else can open it. See [Troubleshooting](../README.md#troubleshooting).
