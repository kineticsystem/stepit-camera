# Ubuntu 24.04 with user "developer", Node.js and pnpm.
#
# The UI is a Node.js application and needs no ROS: the browser talks to the
# robot through rosbridge and web_video_server, which run next to the ROS
# nodes.

FROM ubuntu:24.04 AS ui

LABEL name="develop:stepit-ui"

ARG DEBIAN_FRONTEND=noninteractive

# Each install refreshes the package lists in its own layer: a list cached in a
# layer of its own goes stale, and its packages vanish from the mirrors.
RUN apt-get update && apt-get -y install sudo
RUN echo 'root:docker' | chpasswd                      # Update user root password.

# Ubuntu 24.04 base images ship a default 'ubuntu' user at uid/gid 1000. Remove
# it so 'developer' can be created with a matching uid/gid to the host user
# (passed in via build args), which keeps the bind-mounted workspace writable.
RUN if id -u ubuntu >/dev/null 2>&1; then userdel -r ubuntu; fi
RUN if getent group ubuntu >/dev/null 2>&1; then groupdel ubuntu; fi

ARG USER_UID=1000
ARG USER_GID=1000

# Create the user developer with a uid/gid matching the host user so files
# created in the bind-mounted workspace (node_modules, dist) are writable and
# owned correctly on the host.
RUN groupadd -g ${USER_GID} developer
RUN useradd --create-home --shell /bin/bash --uid ${USER_UID} --gid ${USER_GID} developer
RUN echo 'developer:developer' | chpasswd              # Update user developer password.
RUN usermod -aG sudo developer                         # Add user to sudoers.

RUN apt-get update && apt-get -y upgrade \
 && apt-get -y install ca-certificates curl wget git unzip xz-utils

# Node.js from the official binaries, and pnpm through corepack, which ships
# with Node.js and pins the pnpm version declared in package.json.
ARG NODE_VERSION=24.15.0
RUN arch=$(dpkg --print-architecture | sed 's/amd64/x64/') \
 && curl -fsSL https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${arch}.tar.xz \
  | tar -xJ -C /usr/local --strip-components=1 \
 && corepack enable

# The container user is in the sudo group, but a password prompt makes the
# scripts in ./bin unusable from a non-interactive shell, e.g.
# `docker exec <container-name> serve.sh`.
RUN echo 'developer ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/developer \
 && chmod 0440 /etc/sudoers.d/developer

CMD ["bash"]

USER developer

ENV HOME=/home/developer

# Answer corepack's download prompt for pnpm, so that non-interactive commands
# do not hang on it.
ENV COREPACK_ENABLE_DOWNLOAD_PROMPT=0

# The repo is bind-mounted at ~/ws (see docker-compose.yml), so ./bin lives at
# a fixed path inside the container. Putting it on the PATH here rather than in
# the bashrc means it also applies to non-interactive shells, e.g.
# `docker exec <container-name> serve.sh`.
ENV PATH="/home/developer/ws/bin:${PATH}"

# Whatever genuinely needs an interactive shell -- the aliases -- lives in this
# file, which is versioned in the repo instead of being echoed line by line
# into ~/.bashrc. Source it from the stock ~/.bashrc rather than replacing that
# file, to keep the distribution's own settings.
COPY --chown=developer:developer bashrc /home/developer/.bashrc.ui
RUN echo "source ~/.bashrc.ui" >> ~/.bashrc

WORKDIR /home/developer/ws
