#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sdb::libsdb" for configuration ""
set_property(TARGET sdb::libsdb APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(sdb::libsdb PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libsdb.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS sdb::libsdb )
list(APPEND _IMPORT_CHECK_FILES_FOR_sdb::libsdb "${_IMPORT_PREFIX}/lib/libsdb.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
