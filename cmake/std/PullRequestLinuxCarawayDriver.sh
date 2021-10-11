#!/bin/bash -l

if [ "${Trilinos_CTEST_DO_ALL_AT_ONCE}" == "" ] ; then
  export Trilinos_CTEST_DO_ALL_AT_ONCE=TRUE
fi

set -x

srun -N1 -p -n 4 MI100 ${WORKSPACE}/Trilinos/cmake/std/PullRequestLinuxDriver.sh
