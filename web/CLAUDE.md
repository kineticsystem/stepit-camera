# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working environment

Everything is built, tested and run **inside the Docker container**, never on the host.

```bash
./docker/dock.sh stepit-ui build   # create image + container (also picks up Dockerfile changes)
./docker/dock.sh stepit-ui start   # start it and open a shell
./docker/dock.sh stepit-ui serve   # or: install, build and serve on http://localhost:8090
```

The repo is bind-mounted at `~/ws`. `~/ws/bin` is on the `PATH` and `docker/bashrc` defines the
aliases `update`, `build`, `test`, `serve` and `dev`, usable from any directory.

## Commands

```bash
update    # pnpm install --frozen-lockfile
build     # type-check and bundle into dist/
test      # type-check and vitest
serve     # serve dist/ on container port 8080, host port 8090
dev       # Vite with hot reload, host port 5174
```

To see the UI work, run StepIt Camera next to it: `ros2 launch stepit_camera camera.launch.py
fake:=true` in its container starts the driver, web_video_server (8081) and rosbridge (9091).

## Architecture

See `docs/ARCHITECTURE.md`. The points that are easy to break:

- **The browser talks to the robot directly**: rosbridge for services, parameters and
  pictures; web_video_server for the live view. The Node.js server only serves `dist/`.
- **One rosbridge per module.** A rosbridge only knows the messages installed next to it: the
  camera's is on 9091, StepIt Commander's on 9090. Each section in `App.tsx` names its own.
- **Layers**: `ros/` knows rosbridge only; `camera/camera.ts` knows the camera's ROS interface
  but not React; stores and components on top. Keep modules testable with `tests/fakeSocket.ts`.
- **Pictures are whole files**, e.g. a 29 MB RAW, sent by rosbridge in fragments. Subscribe to
  `/camera/picture` only while waiting for a test shot.
- **web_video_server does not decode `%2F`**: write the topic unescaped in the stream URL.

## Conventions

- TypeScript strict, React 19, zustand; the same tooling and style as the StepIt Editor.
- Update the README and `docs/ARCHITECTURE.md` when the interface or the ports change.
