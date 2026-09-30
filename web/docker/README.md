# Running the UI inside a docker container

The container user and password are:

**developer:developer**

## Prerequisites

First, you must install `docker`.

```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
```

## Build and start up a container

The container is defined in `docker-compose.yml`; `dock.sh` is a thin wrapper
that supplies the container name and the host uid/gid, then calls
`docker compose`.

Build the image and create the container. The image contains Node.js and pnpm:

```bash
./docker/dock.sh [container-name] build
```

This is also how you pick up changes to the `Dockerfile`: it rebuilds only the
layers that changed, so there is no need to `clean` first.

The quickest way to use the UI: install, build and serve it on
<http://localhost:8090>:

```bash
./docker/dock.sh [container-name] serve
```

Or start the container with an interactive shell:

```bash
./docker/dock.sh [container-name] start
```

Set `UI_PORT` (default 8090) or `DEV_PORT` (default 5174) to publish the UI on
other host ports. They differ from the StepIt Editor's, 8080 and 5173, so that
both can run at once.

Run this to stop the container:

```bash
./docker/dock.sh [container-name] stop
```

Finally, run this to remove container and image:

```bash
./docker/dock.sh [container-name] clean
```

## Working with the code

Inside the container, the repo is bind-mounted at `~/ws`. `~/ws/bin` is on the
`PATH` and the scripts are aliased, so these work from any directory:

```bash
update      # pnpm install
build       # type-check and bundle the UI into dist/
serve       # serve dist/ on port 8080, published on the host's UI_PORT
dev         # or: the Vite development server with hot reload, on DEV_PORT
test        # type-check and run the unit tests
```

From a non-interactive shell (e.g. `docker exec [container-name] build.sh`),
call the scripts by their full names: `update.sh`, `build.sh`, and so on.

The interactive shell setup -- the aliases -- lives in `docker/bashrc`, which
the image installs as `~/.bashrc.ui`. Variables belong in the `Dockerfile` as
`ENV` instead, so that they apply to non-interactive commands too.
