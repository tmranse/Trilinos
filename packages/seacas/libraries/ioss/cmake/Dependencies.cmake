IF(TPL_ENABLE_GTest)
  set(MAYBE_GTEST_TPL GTest)
  set(MAYBE_GTEST_PACKAGE)
ELSE()
  set(MAYBE_GTEST_TPL)
  set(MAYBE_GTEST_PACKAGE Gtest)
ENDIF()

if(CMAKE_PROJECT_NAME STREQUAL "SEACASProj")
TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
  LIB_OPTIONAL_PACKAGES SEACASExodus Zoltan ${MAYBE_GTEST_PACKAGE}
  LIB_OPTIONAL_TPLS HDF5 Pamgen CGNS ParMETIS Faodel Cereal DLlib Pthread ADIOS2 ${MAYBE_GTEST_TPL} Kokkos DataWarp fmt
)
else()
TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
  LIB_OPTIONAL_PACKAGES SEACASExodus Pamgen Zoltan Kokkos ${MAYBE_GTEST_PACKAGE}
  LIB_OPTIONAL_TPLS HDF5 CGNS ParMETIS Faodel Cereal DLlib Pthread DataWarp ADIOS2 ${MAYBE_GTEST_TPL}
)
endif()

TRIBITS_TPL_TENTATIVELY_ENABLE(DLlib)
