#!/bin/bash
# Host tests for the sketch's pure logic (FootAuto.h, VirtualPad.h, the controller layer), built with
# g++ in the ros2d2-dev Docker image (any Linux g++ works too):
#
#   bash test/run_tests.sh
set -e
cd "$(dirname "$0")/.."
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/fw" ros2d2-dev \
    bash -c 'set -e; for t in /fw/test/*.cpp; do g++ -std=c++11 -Wall -Wextra -Werror -I/fw/test/stubs -I/fw "$t" -o /tmp/t; /tmp/t; done'
