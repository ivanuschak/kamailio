# https://github.com/SOCI/soci/blob/master/cmake/modules/FindOracle.cmake
###############################################################################
#
# CMake module to search for Oracle client library (OCI)
#
# On success, the macro sets the following variables:
# ORACLE_FOUND       = if the library found
# ORACLE_LIBRARY     = full path to the library
# ORACLE_LIBRARIES   = full path to the library
# ORACLE_INCLUDE_DIR = where to find the library headers also defined,
#                       but not for general use are
# ORACLE_VERSION     = version of library which was found, e.g. "1.2.5"
#
# Copyright (c) 2009-2013 Mateusz Loskot <mateusz@loskot.net>
#
# Developed with inspiration from Petr Vanek <petr@scribus.info>
# who wrote similar macro for TOra - http://torasql.com/
#
# Module source: http://github.com/mloskot/workshop/tree/master/cmake/
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

# First check for CMAKE  variable
if(NOT ORACLE_HOME)
  # If ORACLE_HOME is not defined check for env var and if exists set from env var
  if(EXISTS $ENV{ORACLE_HOME})
    set(ORACLE_HOME $ENV{ORACLE_HOME})
  endif()
  if(EXISTS $ENV{ORACLE_INCLUDE_DIR})
    set(ORACLE_INCLUDE_DIR $ENV{ORACLE_INCLUDE_DIR})
  endif()
endif()

message(STATUS "ORACLE_HOME=${ORACLE_HOME}")

find_path(
  ORACLE_INCLUDE_DIR
  NAMES oci.h
  PATHS ${ORACLE_HOME}/rdbms/public
        ${ORACLE_HOME}/include
        ${ORACLE_HOME}/sdk/include # Oracle SDK
        ${ORACLE_HOME}/OCI/include # Oracle XE on Windows
        # instant client from rpm
        /usr/include/oracle/*/client${LIB_SUFFIX}
)

set(ORACLE_VERSIONS
    21
    20
    19
    18
    12
    11
    10
)
set(ORACLE_OCI_NAMES clntsh libclntsh oci) # Dirty trick might help on OSX, see issues/89
set(ORACLE_OCCI_NAMES libocci occi)
set(ORACLE_NNZ_NAMES ociw32)
foreach(loop_var IN LISTS ORACLE_VERSIONS)
  set(ORACLE_OCCI_NAMES ${ORACLE_OCCI_NAMES} oraocci${loop_var})
  set(ORACLE_NNZ_NAMES ${ORACLE_NNZ_NAMES} nnz${loop_var} libnnz${loop_var})
endforeach(loop_var)

set(ORACLE_LIB_DIR
    ${ORACLE_HOME}
    ${ORACLE_HOME}/lib
    ${ORACLE_HOME}/sdk/lib # Oracle SDK
    ${ORACLE_HOME}/sdk/lib/msvc
    ${ORACLE_HOME}/OCI/lib/msvc # Oracle XE on Windows
    # Instant client from rpm
    /usr/lib/oracle/*/client${LIB_SUFFIX}/lib
)

find_library(
  ORACLE_OCI_LIBRARY
  NAMES ${ORACLE_OCI_NAMES}
  PATHS ${ORACLE_LIB_DIR}
)
find_library(
  ORACLE_OCCI_LIBRARY
  NAMES ${ORACLE_OCCI_NAMES}
  PATHS ${ORACLE_LIB_DIR}
)
find_library(
  ORACLE_NNZ_LIBRARY
  NAMES ${ORACLE_NNZ_NAMES}
  PATHS ${ORACLE_LIB_DIR}
)

set(ORACLE_LIBRARY ${ORACLE_OCI_LIBRARY} ${ORACLE_OCCI_LIBRARY} ${ORACLE_NNZ_LIBRARY})

if(NOT WIN32)
  set(ORACLE_LIBRARY ${ORACLE_LIBRARY} ${ORACLE_CLNTSH_LIBRARY})
endif(NOT WIN32)

set(ORACLE_LIBRARIES ${ORACLE_LIBRARY})

# Handle the QUIETLY and REQUIRED arguments and set ORACLE_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Oracle DEFAULT_MSG ORACLE_LIBRARY ORACLE_INCLUDE_DIR)

if(NOT ORACLE_FOUND)
  message(
    STATUS
      "None of the supported Oracle versions (${ORACLE_VERSIONS}) could be found, consider updating ORACLE_VERSIONS if the version you use is not among them."
  )
endif()

mark_as_advanced(ORACLE_INCLUDE_DIR ORACLE_LIBRARY)
