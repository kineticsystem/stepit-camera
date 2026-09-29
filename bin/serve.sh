#!/bin/bash -e

# The scripts act on the workspace root, so move there. This lets them be
# called from anywhere, e.g. via the aliases in the container.
cd "$(dirname "$(readlink -f "$0")")/.."

# Serve the UI built by build.sh, on port ${PORT:-8080} of the container,
# published on http://localhost:${UI_PORT:-8090} of the host.
exec pnpm run start
