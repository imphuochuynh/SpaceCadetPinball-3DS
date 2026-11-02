#!/bin/bash
# Stand-in for makerom inside the build container. Prebuilt makerom needs a
# newer glibc than the devkitarm image has, so the container only records the
# exact arguments and CI replays them with the real makerom on the host runner.
printf '%q ' "$@" > "$(dirname "$0")/../makerom.args"
