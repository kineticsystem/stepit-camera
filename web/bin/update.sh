#!/bin/bash -e

# The scripts act on the workspace root, so move there. This lets them be
# called from anywhere, e.g. via the aliases in the container.
cd "$(dirname "$(readlink -f "$0")")/.."

# Install the JavaScript dependencies at the versions of pnpm-lock.yaml. When
# node_modules was installed by another pnpm (e.g. on the host rather than in
# the container), pnpm has to rebuild it: allow that without a prompt.
pnpm install --frozen-lockfile --config.confirmModulesPurge=false
