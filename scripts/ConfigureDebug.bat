#!/bin/bash

cmake -S .. -B ../build -DCMAKE_BUILD_TYPE="Debug" -DFLOURISH_BUILD_TESTS=1 -DFLOURISH_ENABLE_LOGGING=1 -G Ninja
