#!/bin/bash
# Host tests for the sketch's pure logic (FootAuto.h), built with g++ in the
# ros2d2-dev Docker image (any Linux g++ works too):
#
#   bash test/run_tests.sh
set -e
cd "$(dirname "$0")/.."
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/fw" ros2d2-dev \
    bash -c "g++ -std=c++11 -Wall -Wextra -Werror -I/fw /fw/test/FootAutoTest.cpp -o /tmp/t && /tmp/t"
