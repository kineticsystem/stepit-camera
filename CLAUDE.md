# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working environment

Everything is built, tested and run **inside the Docker container**, never on the host: the
ament linters, `colcon`, libgphoto2 and the ROS 2 Jazzy environment only exist there.

```bash
./docker/dock.sh stepit-camera build   # create image + container (also picks up Dockerfile changes)
./docker/dock.sh stepit-camera start   # start it and open a shell
```

The repo is bind-mounted at `~/ws`, so host edits are visible immediately and no rebuild is
needed for code changes. `~/ws/bin` is on the `PATH` and `docker/bashrc` defines the aliases
`build`, `test`, `update` and `dev`, usable from any directory. The scripts `cd` to the workspace
root themselves.

## Commands

```bash
update            # ./bin/update.sh -- rosdep install + pnpm install in web/; after a dependency changes
build             # ./bin/build.sh  -- colcon build, Debug, --symlink-install; then the test page into web/dist
test              # ./bin/test.sh   -- the page's typecheck + vitest, then colcon test + test-result
dev               # ./bin/dev.sh    -- Vite with hot reload for the test page, http://localhost:5174

# One test target (targets are named after the files in src/stepit_camera_tests/tests)
colcon test --packages-select stepit_camera_tests --ctest-args -R test_camera_driver \
  --event-handlers console_direct+

source install/setup.bash
ros2 launch stepit_camera camera.launch.py fake:=true   # no hardware needed
ros2 service call /camera/start_streaming std_srvs/srv/Trigger
ros2 service call /camera/take_picture std_srvs/srv/Trigger
ros2 param set /camera iso 800
# The test page on http://localhost:8090, the pictures on http://localhost:8090/pictures/<name>

pre-commit run -a  # in the container; on the host: SKIP=ament_copyright,ament_lint_cmake,ament_cpplint
```

## Architecture

See `docs/ARCHITECTURE.md`. The points that are easy to break:

- **One thread owns the camera.** libgphoto2 is not thread safe. Only `CameraDriver`'s thread
  touches a `Camera`; everything else goes through `CameraDriver::run()`, which queues a task.
  Never call a `Camera` from a ROS callback directly.
- **Never read ROS parameters from the driver's thread.** `set_parameters` holds the
  parameters' lock while its callback waits in `run()`, so the driver's thread would deadlock
  on it. `CameraNode` keeps its own copy, `wanted_settings_`, for `onConnected`.
- **Fatal vs non-fatal `CameraError`** decides whether the driver reconnects. Only lost
  connections are fatal; a refused value or a busy camera is not.
- **The driver does not trigger the camera**: an external device does. The driver watches
  for the files the camera reports and downloads them. The one exception is the test shot,
  `~/take_picture`, whose picture comes the same way.
- **A shot during the live view breaks it** on a Canon EOS, until the viewfinder is switched
  off and on: `CameraDriver::stream()` does that after a failed frame. `FakeCamera` mimics it.
- `gphoto_camera.cpp` is the only file that includes libgphoto2, and it has no automated
  test: check it by hand with a real camera.
- A new camera setting is one line in `SETTINGS` (`settings.hpp`) plus its choices in
  `FakeCamera`.
- **A picture is published as a path, never as its content.** `~/picture` says where the
  saved file is; nodes read it, pages load it from `web_server` (`/pictures/<name>`), a RAW
  through two `Range` requests for its JPEG preview. Do not add the content to the message: a
  ~30 MB RAW would load DDS, which motor controllers may share, and rosbridge would send ~40 MB
  of base64. Hence `download_directory` cannot be empty.
- `web_server` (cpp-httplib) must keep `SO_REUSEADDR` only: httplib's default `SO_REUSEPORT` lets
  a leftover server share the port silently.

## The test page (`web/`)

See `docs/WEB_PAGE.md`. A React 19 + zustand + Vite page, TypeScript strict, tested with vitest;
not a ROS package (`web/COLCON_IGNORE`). It only tests the camera and the driver: keep it
free of anything beyond them.

- **The browser talks to the driver's servers directly**: `web_server` (8090) for the page
  and the pictures, rosbridge (9091) for services, parameters and `picture`,
  web_video_server (8081) for the live view.
- **Layers**: `ros/` knows rosbridge only; `camera/` knows the camera's ROS interface and the
  web server but not React; stores and components on top. Test with `tests/fakeSocket.ts` and
  a stubbed `fetch`.
- **web_video_server does not decode `%2F`**: write the topic unescaped in the stream URL.

| Package | Rule |
|---|---|
| `stepit_camera_msgs` | Interfaces only, no code. |
| `stepit_camera` | All the code, as a shared library, plus the node, the web server, config and launch file. |
| `stepit_camera_tests` | All tests, against `FakeCamera`; the other packages carry none. |

## CI

`.github/workflows`, as in StepIt Driver: `industrial_ci.yml` builds and tests (jazzy, main and
testing), `ci-format.yml` runs pre-commit without the ament hooks, `ci-ros-lint.yml` runs those per
package; a new package must be added to its `package-name` lists.

## Conventions

- Every source file carries the MIT copyright header (`ament_copyright` enforces it).
- Packages compile with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`; C++17.
- `cpplint` runs with `--linelength=121`; `clang-format` uses the repo `.clang-format`.
- Update the README's parameter table and `docs/ARCHITECTURE.md` when the interface changes,
  and `docs/WEB_PAGE.md` when the page changes.
- **The project stands on its own.** Docs, comments and code never mention the projects that
  use it, e.g. a rig or another UI: describe the camera, the driver and their interfaces only.
