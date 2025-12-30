# ./build/scratch/CI-SGC/ns3-dev-ci-sgc-simulator-default \
#     --nVehicle=20 \
#     --maxVelocity=10 \
#     --initPosMin=300 \
#     --initPosMax=1000 \
#     --securityLevel=80 \
#     --maxGroupSize=10 \
#     --maxGroupNum=3 \
#     --groupSize=10 \
#     --stopTime=600 \
#     --hbInterval=1000 \
#     --keyUpdInterval=3000


# test V2V and V2I communication delay
# ./build/scratch/CI-SGC/ns3-dev-ci-sgc-simulator-default \
#     --nVehicle=2 \
#     --maxVelocity=10 \
#     --initPosMin=50 \
#     --initPosMax=100 \
#     --securityLevel=80 \
#     --maxGroupSize=10 \
#     --maxGroupNum=3 \
#     --groupSize=10 \
#     --stopTime=10 \
#     --hbInterval=1000 \
#     --keyUpdInterval=3000

#!/usr/bin/env bash

set -euo pipefail

BIN=./build/scratch/CI-SGC/ns3-dev-ci-sgc-simulator-default
OUT_DIR=/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/tmplog
TOOLS_BIN=/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/tools/
TRIM_BIN=${TOOLS_BIN}/trim

MAX_GROUP_SIZE=50
SECURITY_LEVEL=128

OUT_FILE=${OUT_DIR}/test_total_time_security${SECURITY_LEVEL}_size${MAX_GROUP_SIZE}.log

mkdir -p ${OUT_DIR}

    ${BIN} \
            --nVehicle=200 \
            --maxVelocity=10 \
            --initPosMin=10 \
            --initPosMax=2000 \
            --securityLevel=${SECURITY_LEVEL} \
            --maxGroupSize=${MAX_GROUP_SIZE} \
            --groupSizeStep=${MAX_GROUP_SIZE} \
            --maxGroupNum=5 \
            --groupSize=50 \
            --stopTime=600 \
            --hbInterval=1000 \
            --keyEncapInterval=4000 \
            --KeyUpdInterval=2000 \
            --KeyUpdThreshold=2000 
            # | ${TRIM_BIN} "${OUT_FILE}"