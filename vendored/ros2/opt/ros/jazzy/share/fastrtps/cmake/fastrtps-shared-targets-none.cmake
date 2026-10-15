#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "fastrtps" for configuration "None"
set_property(TARGET fastrtps APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(fastrtps PROPERTIES
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/libfastrtps.so.2.14.6"
  IMPORTED_SONAME_NONE "libfastrtps.so.2.14"
  )

list(APPEND _cmake_import_check_targets fastrtps )
list(APPEND _cmake_import_check_files_for_fastrtps "${_IMPORT_PREFIX}/lib/libfastrtps.so.2.14.6" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
