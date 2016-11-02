#!/usr/bin/env bash
stack_size_kb=$((16 * 1024))
ulimit -s $stack_size_kb
export OMP_STACKSIZE=$stack_size_kb
export OMP_WAIT_POLICY=ACTIVE

if [[ $2 = serial ]]; then
    exe=uts.icc.serial
    result_dir=${exe}_$1
else
    export OMP_NUM_THREADS=$2
    if [[ $3 = tied ]]; then
        exe=uts.icc.omp-tasks-tied
    else
        exe=uts.icc.omp-tasks
    fi
    result_dir=${exe}_$1_$2
fi

amplxe-cl -collect hotspots -result-dir $result_dir -- \
    bin/$exe -f inputs/uts/$1.input
