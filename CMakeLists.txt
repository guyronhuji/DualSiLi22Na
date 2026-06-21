cmake_minimum_required(VERSION 3.16)
project(DualSiLi22Na VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE AND NOT DEFINED Geant4_DIR)
  if(EXISTS "$ENV{HOME}/Code/GEANT4/lib/cmake/Geant4/Geant4Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{HOME}/Code/GEANT4")
  elseif(EXISTS "/opt/homebrew/lib/cmake/Geant4/Geant4Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
  endif()
endif()

option(DUALSILI_ENABLE_VIS "Build with Geant4 UI and visualization support" OFF)
option(DUALSILI_ENABLE_QT "Build with the Geant4 Qt UI/visualization driver" OFF)

if(DUALSILI_ENABLE_QT)
  set(DUALSILI_ENABLE_VIS ON CACHE BOOL "Build with Geant4 UI and visualization support" FORCE)
endif()

function(dualsili_resolve_imported_locations out_var)
  set(_resolved)
  foreach(_lib IN LISTS ARGN)
    if(TARGET "${_lib}")
      get_target_property(_loc "${_lib}" IMPORTED_LOCATION_RELEASE)
      if(NOT _loc)
        get_target_property(_loc "${_lib}" IMPORTED_LOCATION)
      endif()
      if(_loc)
        list(APPEND _resolved "${_loc}")
      else()
        list(APPEND _resolved "${_lib}")
      endif()
    else()
      list(APPEND _resolved "${_lib}")
    endif()
  endforeach()
  set(${out_var} ${_resolved} PARENT_SCOPE)
endfunction()

if(DUALSILI_ENABLE_QT)
  find_package(Geant4 REQUIRED qt)
  foreach(_required_target
          Geant4::G4processes
          Geant4::G4processes_core
          Geant4::G4processes_hadronic
          Geant4::G4tools)
    if(TARGET "${_required_target}")
      list(APPEND Geant4_LIBRARIES "${_required_target}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES Geant4_LIBRARIES)
  # Geant4's macOS Qt package can export legacy AGL link flags via imported
  # target dependencies. Linking the dylib locations directly avoids that stale
  # framework while preserving the dylibs' own runtime dependencies.
  dualsili_resolve_imported_locations(DUALSILI_GEANT4_LIBRARIES ${Geant4_LIBRARIES})
elseif(DUALSILI_ENABLE_VIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
  set(DUALSILI_GEANT4_LIBRARIES ${Geant4_LIBRARIES})
else()
  find_package(Geant4 REQUIRED)
  set(DUALSILI_GEANT4_LIBRARIES ${Geant4_LIBRARIES})
  list(FILTER DUALSILI_GEANT4_LIBRARIES EXCLUDE REGEX
       "Geant4::G4(Tree|FR|GMocren|RayTracer|VRML|ToolsSG|vis_management|modeling|interfaces|mctruth|geomtext)$")
endif()

add_executable(DualSiLi22Na
  src/main.cc
  src/ActionInitialization.cc
  src/DetectorConstruction.cc
  src/EventAction.cc
  src/PhysicsList.cc
  src/PrimaryGeneratorAction.cc
  src/RunAction.cc
  src/SensitiveDetector.cc
  src/TrackingAction.cc
)

target_include_directories(DualSiLi22Na PRIVATE include)
target_link_libraries(DualSiLi22Na PRIVATE ${DUALSILI_GEANT4_LIBRARIES})
if(DUALSILI_ENABLE_VIS)
  target_compile_definitions(DualSiLi22Na PRIVATE DUALSILI_ENABLE_VIS=1)
endif()
if(DUALSILI_ENABLE_QT)
  target_compile_definitions(DualSiLi22Na PRIVATE DUALSILI_ENABLE_QT=1)
  target_compile_definitions(DualSiLi22Na PRIVATE
    G4LIB_BUILD_DLL
    G4UI_USE_QT
    G4VIS_USE_OPENGLQT
    G4VIS_USE_OPENGLX
    G4VIS_USE_TOOLSSG_QT_GLES
    G4VIS_USE_TOOLSSG_QT_ZB
    G4VIS_USE_TOOLSSG_X11_GLES
    G4VIS_USE_TOOLSSG_X11_ZB
  )
endif()

foreach(_dir macros analysis output)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${_dir}
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
