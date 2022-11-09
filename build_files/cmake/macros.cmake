# same as above but generates the var name and output automatic.
function(data_to_c_simple
  file_from
  list_to_add
  )

  # remove ../'s
  get_filename_component(_file_from ${CMAKE_CURRENT_SOURCE_DIR}/${file_from}   REALPATH)
  get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.c REALPATH)

  list(APPEND ${list_to_add} ${_file_to})
  source_group(Generated FILES ${_file_to})
  list(APPEND ${list_to_add} ${file_from})
  set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

  get_filename_component(_file_to_path ${_file_to} PATH)

  add_custom_command(
    OUTPUT  ${_file_to}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
    COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
    DEPENDS ${_file_from} datatoc)

  set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

# Function for converting pixmap directory to a '.png' and then a '.c' file.
function(data_to_c_simple_icons
  path_from icon_prefix icon_names
  list_to_add
  )

  # Conversion steps
  #  path_from  ->  _file_from  ->  _file_to
  #  foo/*.dat  ->  foo.png     ->  foo.png.c

  get_filename_component(_path_from_abs ${path_from} ABSOLUTE)
  # remove ../'s
  get_filename_component(_file_from ${CMAKE_CURRENT_BINARY_DIR}/${path_from}.png   REALPATH)
  get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${path_from}.png.c REALPATH)

  list(APPEND ${list_to_add} ${_file_to})
  set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

  get_filename_component(_file_to_path ${_file_to} PATH)

  # Construct a list of absolute paths from input
  set(_icon_files)
  foreach(_var ${icon_names})
    list(APPEND _icon_files "${_path_from_abs}/${icon_prefix}${_var}.dat")
  endforeach()

  add_custom_command(
    OUTPUT  ${_file_from} ${_file_to}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
    # COMMAND python3 ${CMAKE_SOURCE_DIR}/source/blender/datatoc/datatoc_icon.py ${_path_from_abs} ${_file_from}
    COMMAND "$<TARGET_FILE:datatoc_icon>" ${_path_from_abs} ${_file_from}
    COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
    DEPENDS
      ${_icon_files}
      datatoc_icon
      datatoc
      # could be an arg but for now we only create icons depending on UI_icons.h
      ${CMAKE_SOURCE_DIR}/source/blender/editors/include/UI_icons.h
    )

  set_source_files_properties(${_file_from} ${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

macro(blender_precompile_headers target cpp header)
  if(MSVC)
    # get the name for the pch output file
    get_filename_component(pchbase ${cpp} NAME_WE)
    set(pchfinal "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${pchbase}.pch")

    # mark the cpp as the one outputting the pch
    set_property(SOURCE ${cpp} APPEND PROPERTY OBJECT_OUTPUTS "${pchfinal}")

    # get all sources for the target
    get_target_property(sources ${target} SOURCES)

    # make all sources depend on the pch to enforce the build order
    foreach(src ${sources})
      set_property(SOURCE ${src} APPEND PROPERTY OBJECT_DEPENDS "${pchfinal}")
    endforeach()

    target_sources(${target} PRIVATE ${cpp} ${header})
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "/Yu${header} /Fp${pchfinal} /FI${header}")
    set_source_files_properties(${cpp} PROPERTIES COMPILE_FLAGS "/Yc${header} /Fp${pchfinal}")
  endif()
endmacro()

macro(set_and_warn_dependency
  _dependency _setting _val)
  # when $_dependency is disabled, forces $_setting = $_val
  if(NOT ${${_dependency}} AND ${${_setting}})
    message(STATUS "'${_dependency}' is disabled: forcing 'set(${_setting} ${_val})'")
    set(${_setting} ${_val})
  endif()
endmacro()

macro(without_system_libs_begin)
  set(CMAKE_IGNORE_PATH "${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES};${CMAKE_SYSTEM_INCLUDE_PATH};${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES};${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")
  if(APPLE)
    # Avoid searching for headers in frameworks (like Mono), and libraries in LIBDIR.
    set(CMAKE_FIND_FRAMEWORK NEVER)
  endif()
endmacro()

macro(without_system_libs_end)
  unset(CMAKE_IGNORE_PATH)
  if(APPLE)
    # FIRST is the default.
    set(CMAKE_FIND_FRAMEWORK FIRST)
  endif()
endmacro()
