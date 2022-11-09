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
