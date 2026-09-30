#!/bin/bash -e

# The scripts act on the workspace root, so move there. This lets them be
# called from anywhere, e.g. via the aliases in the container.
cd "$(dirname "$(readlink -f "$0")")/.."

# Run the test page with hot reload, on http://localhost:5174, for working on
# the page itself. The camera driver and its servers must run too.
cd web
exec pnpm run dev
