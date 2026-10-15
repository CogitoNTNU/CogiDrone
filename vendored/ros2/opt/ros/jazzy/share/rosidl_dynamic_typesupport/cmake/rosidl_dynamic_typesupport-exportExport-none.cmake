#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "rosidl_dynamic_typesupport::rosidl_dynamic_typesupport" for configuration "None"
set_property(TARGET rosidl_dynamic_typesupport::rosidl_dynamic_typesupport APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(rosidl_dynamic_typesupport::rosidl_dynamic_typesupport PROPERTIES
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/librosidl_dynamic_typesupport.so"
  IMPORTED_SONAME_NONE "librosidl_dynamic_typesupport.so"
  )

list(APPEND _cmake_import_check_targets rosidl_dynamic_typesupport::rosidl_dynamic_typesupport )
list(APPEND _cmake_import_check_files_for_rosidl_dynamic_typesupport::rosidl_dynamic_typesupport "${_IMPORT_PREFIX}/lib/librosidl_dynamic_typesupport.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
