#!/bin/bash -e

# The scripts act on the workspace root, so move there. This lets them be
# called from anywhere, e.g. via the aliases in the container.
cd "$(dirname "$(readlink -f "$0")")/.."

# Run the UI with hot reload, on port 5173 of the container, published on
# http://localhost:${DEV_PORT:-5174} of the host, for working on the UI itself.
exec pnpm run dev
