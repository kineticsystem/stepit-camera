#!/bin/bash -e

# Source the ROS 2 environment.
source /opt/ros/jazzy/setup.bash

# colcon and rosdep act on the current working directory, so move to the
# workspace root. This lets the script be called from anywhere, e.g. via the
# build/test/update aliases in the container.
cd "$(dirname "$(readlink -f "$0")")/.."

colcon build --cmake-args -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON --symlink-install --event-handlers log-

# The test page: type-check it and bundle it into web/dist, which the web
# server serves.
(cd web && pnpm run build)
