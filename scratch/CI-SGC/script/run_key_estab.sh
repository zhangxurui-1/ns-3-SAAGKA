#!/usr/bin/env bash

set -euo pipefail

BIN=/Users/zxr/workspace/ns-3-dev/build/scratch/CI-SGC/ns3-dev-ci-sgc-simulator-default
TOOLS_BIN=/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/tools/
TRIM_BIN=${TOOLS_BIN}/trim
AGG_BIN=${TOOLS_BIN}/aggregate
OUT_DIR=/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/log

rm -f ${OUT_DIR}/test_total_time_*.log

for SECURITY_LEVEL in 80 128; do
    for ((i=1; i<=10; i++)); do
        MAX_GROUP_SIZE=$((10 * i))
        N_VEHICLE=10
        if [ ${MAX_GROUP_SIZE} -ge 30 ]; then
            N_VEHICLE=20
        fi
        OUT_FILE=${OUT_DIR}/test_total_time_security${SECURITY_LEVEL}_size${MAX_GROUP_SIZE}.log

        echo "Running Experiment ${i}, maxGroupSize=${MAX_GROUP_SIZE}, securityLevel=${SECURITY_LEVEL}"

        ${BIN} \
            --nVehicle=${N_VEHICLE} \
            --maxVelocity=10 \
            --initPosMin=50 \
            --initPosMax=500 \
            --securityLevel=${SECURITY_LEVEL} \
            --maxGroupSize=${MAX_GROUP_SIZE} \
            --groupSizeStep=${MAX_GROUP_SIZE} \
            --maxGroupNum=3 \
            --groupSize=10 \
            --stopTime=600 \
            --hbInterval=1000 \
            --keyEncapInterval=4000 \
            --KeyUpdInterval=2000 \
            --KeyUpdThreshold=2000 \
            | ${TRIM_BIN} "${OUT_FILE}"
        
        echo "Experiment ${i} done"
    done

done

$AGG_BIN
