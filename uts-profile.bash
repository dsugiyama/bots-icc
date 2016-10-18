#!/usr/bin/env bash
stack_size_kb=$((16 * 1024))
ulimit -s $stack_size_kb
export OMP_STACKSIZE=$stack_size_kb
export OMP_NUM_THREADS=$2

if [[ $3 == tied ]]; then
    exe=uts.icc.omp-tasks-tied
else
    exe=uts.icc.omp-tasks
fi

amplxe-cl -collect hotspots -result-dir $1 -- \
    bin/$exe -f inputs/uts/small.input
