#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "builtin_interfaces::builtin_interfaces__rosidl_generator_py" for configuration "None"
set_property(TARGET builtin_interfaces::builtin_interfaces__rosidl_generator_py APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(builtin_interfaces::builtin_interfaces__rosidl_generator_py PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "builtin_interfaces::builtin_interfaces__rosidl_generator_c;Python3::Python;builtin_interfaces::builtin_interfaces__rosidl_typesupport_c"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/libbuiltin_interfaces__rosidl_generator_py.so"
  IMPORTED_SONAME_NONE "libbuiltin_interfaces__rosidl_generator_py.so"
  )

list(APPEND _cmake_import_check_targets builtin_interfaces::builtin_interfaces__rosidl_generator_py )
list(APPEND _cmake_import_check_files_for_builtin_interfaces::builtin_interfaces__rosidl_generator_py "${_IMPORT_PREFIX}/lib/libbuiltin_interfaces__rosidl_generator_py.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
