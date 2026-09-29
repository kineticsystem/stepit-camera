#! /bin/bash -e

# Use this script to create, start, stop and remove the docker container that
# builds and runs the StepIt Macro.
#
# The container itself is defined in docker-compose.yml. This script only adds
# what compose cannot express: the host uid/gid and the container name passed
# on the command line.

function container_exists() {
    local name="$1"
    [[ $(docker ps -aq --filter name=^/${name}$) ]]
}

function image_exists() {
    local name="$1"
    [[ $(docker images -q $name) ]]
}

# Whether the container was created from this compose file, and can therefore
# be managed by compose.
function compose_owns_container() {
    local name="$1"
    local project
    project=$(docker inspect "$name" \
      --format '{{index .Config.Labels "com.docker.compose.project"}}' 2>/dev/null)
    [[ "$project" == "$name" ]]
}

function display_usage() {
    echo -e "\nUsage: ./dock.sh <container-name> <command>\n
    Commands:
    build   Build a container without starting it
            Usage: ./dock.sh container-name build
    start   Start the container and open an interactive terminal
            Usage: ./dock.sh container-name start
    serve   Start the container, install, build and run the UI
            on http://localhost:\${MACRO_PORT:-8090}
            Usage: ./dock.sh container-name serve
    stop    Stop the container
            Usage: ./dock.sh container-name stop
    clean   Stop the container and clean everything including images
            Usage: ./dock.sh container-name clean\n"
}

# Start the container, creating it if needed.
function up() {
    local name="$1"
    if container_exists $name && ! compose_owns_container $name; then
        echo "Warning: container '$name' was not created by docker-compose.yml; using it as is." >&2
        docker start $name > /dev/null
    else
        docker compose up --detach
    fi
}

# Compose resolves the paths in docker-compose.yml against the directory that
# holds it, so every command has to run from there.
cd "$(dirname "$0")"

# Check for at least two arguments (container name and command).
if [ "$#" -lt 2 ]; then
    echo "Missing required arguments."
    display_usage
    exit 1
fi

name="$1"
command="$2"

# The container name doubles as the compose project name, so several
# differently named containers can coexist from this same compose file.
export CONTAINER_NAME="$name"
export COMPOSE_PROJECT_NAME="$name"

# Build args for the Dockerfile: a container user matching the host user.
export USER_UID=$(id -u)
export USER_GID=$(id -g)

case "$command" in
    build)
        if container_exists $name && ! compose_owns_container $name; then
            echo "Removing container '$name', which was not created by docker-compose.yml"
            docker rm --force $name > /dev/null
        fi
        # Rebuilds only the layers that the Dockerfile changed since last time,
        # so there is no need to clean first.
        docker compose build
        docker compose create
        ;;
    start)
        up $name
        echo "Opening interactive terminal into $name"
        docker exec -it $name bash
        ;;
    serve)
        up $name
        docker exec $name update.sh
        docker exec $name build.sh
        echo "Starting the UI on http://localhost:${MACRO_PORT:-8090}"
        docker exec -it $name serve.sh
        ;;
    stop)
        docker compose stop
        ;;
    clean)
        docker compose down --rmi local --remove-orphans
        if container_exists $name; then
            echo "Removing container '$name', which was not created by docker-compose.yml"
            docker rm --force $name > /dev/null
        fi
        if image_exists $name:latest; then
            echo "Removing image: $name:latest"
            docker rmi $name:latest > /dev/null
        fi
        ;;
    *)
        echo "Unknown parameter: $command"
        display_usage
        ;;
esac
