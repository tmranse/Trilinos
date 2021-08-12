IF(TPL_ENABLE_GTest)
  set(MAYBE_GTEST_TPL GTest)
  set(MAYBE_GTEST_PACKAGE)
ELSE()
  set(MAYBE_GTEST_TPL)
  set(MAYBE_GTEST_PACKAGE Gtest)
ENDIF()

SET(LIB_REQUIRED_DEP_PACKAGES)
SET(LIB_OPTIONAL_DEP_PACKAGES KokkosCore TeuchosCore TeuchosNumerics TeuchosComm TeuchosKokkosComm)
SET(TEST_REQUIRED_DEP_PACKAGES ${MAYBE_GTEST_PACKAGE})
SET(TEST_OPTIONAL_DEP_PACKAGES KokkosContainers)
SET(LIB_REQUIRED_DEP_TPLS)
SET(LIB_OPTIONAL_DEP_TPLS)
SET(TEST_REQUIRED_DEP_TPLS ${MAYBE_GTEST_TPL})
SET(TEST_OPTIONAL_DEP_TPLS ADOLC ADIC)
