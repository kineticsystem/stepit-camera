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
`build`, `test` and `update`, usable from any directory. The scripts `cd` to the workspace
root themselves.

## Commands

```bash
update            # ./bin/update.sh -- rosdep install; run once after a dependency changes
build             # ./bin/build.sh  -- colcon build, Debug, --symlink-install
test              # ./bin/test.sh   -- colcon test + colcon test-result --all --verbose

# One test target (targets are named after the files in src/stepit_camera_tests/tests)
colcon test --packages-select stepit_camera_tests --ctest-args -R test_camera_driver \
  --event-handlers console_direct+

source install/setup.bash
ros2 launch stepit_camera camera.launch.py fake:=true   # no hardware needed
ros2 service call /camera/start_streaming std_srvs/srv/Trigger
ros2 service call /camera/take_picture std_srvs/srv/Trigger
ros2 param set /camera iso 800

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

| Package | Rule |
|---|---|
| `stepit_camera_msgs` | Interfaces only, no code. |
| `stepit_camera` | All the code, as a shared library, plus the node, config and launch file. |
| `stepit_camera_tests` | All tests, against `FakeCamera`; the other packages carry none. |

## Conventions

- Every source file carries the MIT copyright header (`ament_copyright` enforces it).
- Packages compile with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`; C++17.
- `cpplint` runs with `--linelength=121`; `clang-format` uses the repo `.clang-format`.
- Update the README's parameter table and `docs/ARCHITECTURE.md` when the interface changes.
