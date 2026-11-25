cmake_minimum_required(VERSION 3.25)
project(EndfieldAnimationImporter LANGUAGES C CXX)
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Build with MSVC on Windows x64")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Configure with -A x64")
endif()
include(CTest)
add_library(eai_dependencies INTERFACE)
target_include_directories(eai_dependencies INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/third_party" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/better-endfield/include")
target_compile_features(eai_dependencies INTERFACE cxx_std_20)
target_compile_options(eai_dependencies INTERFACE /utf-8)
add_library(interaction SHARED src/diagnostic.cpp src/overlay_panel.cpp)
target_link_libraries(interaction PRIVATE eai_dependencies user32 gdi32 comdlg32 shell32 ole32)
target_compile_options(interaction PRIVATE /W4)
set_property(TARGET interaction PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_target_properties(interaction PROPERTIES OUTPUT_NAME "Endfield.Interaction" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
add_executable(eai_launcher WIN32 src/launcher.cpp)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/module.json" EAI_MANIFEST_JSON)
string(JSON EAI_VERSION GET "${EAI_MANIFEST_JSON}" version)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS module.json)
target_compile_definitions(eai_launcher PRIVATE "EAI_VERSION_W=L\"${EAI_VERSION}\"")
target_link_libraries(eai_launcher PRIVATE eai_dependencies bcrypt comctl32 comdlg32 ole32 shell32 user32 ws2_32 gdi32)
target_compile_options(eai_launcher PRIVATE /W4)
set_target_properties(eai_launcher PROPERTIES
  OUTPUT_NAME "EAI.Launcher"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
add_executable(eai_offline_host src/offline_host.cpp)
target_link_libraries(eai_offline_host PRIVATE eai_dependencies interaction)
target_compile_options(eai_offline_host PRIVATE /W4)
set_target_properties(eai_offline_host PROPERTIES
  OUTPUT_NAME "EAI.OfflineHost"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
add_executable(validate_animation tools/validate_animation.cpp)
add_executable(eai_animation_import src/native_animation_import.cpp third_party/ufbx/ufbx.c)
target_link_libraries(eai_animation_import PRIVATE eai_dependencies normaliz)
target_compile_definitions(eai_animation_import PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(eai_animation_import PROPERTIES
  OUTPUT_NAME "EAI.AnimationImport"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
add_executable(efmi_setup_tests EXCLUDE_FROM_ALL src/efmi_setup_tests.cpp)
target_link_libraries(efmi_setup_tests PRIVATE eai_dependencies)
set_property(TARGET efmi_setup_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(validate_animation PRIVATE eai_dependencies)
if(BUILD_TESTING)
  find_package(Python3 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME native_animation_import COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_animation_import_tests.py"
      "$<TARGET_FILE:eai_animation_import>" "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
  foreach(test handshake_motion pose_math runtime_simulation follow_metadata external_models efmi_bridge external_mesh free_camera free_camera_input breast_physics language pose_ownership eml_connection)
    add_executable(${test}_tests src/${test}_tests.cpp)
    target_link_libraries(${test}_tests PRIVATE eai_dependencies)
    if(test STREQUAL "handshake_motion")
      add_test(NAME ${test} COMMAND ${test}_tests "${CMAKE_CURRENT_SOURCE_DIR}/native/windows-x64/motions")
    else()
      add_test(NAME ${test} COMMAND ${test}_tests)
    endif()
  endforeach()
  set(EAI_EML_SDK_DIR "" CACHE PATH "Optional EML include directory for connection contract tests")
  if(EAI_EML_SDK_DIR)
    target_include_directories(eml_connection_tests BEFORE PRIVATE "${EAI_EML_SDK_DIR}")
  endif()
  target_compile_definitions(eml_connection_tests PRIVATE INTERACTION_NO_OVERLAY)
  target_compile_definitions(runtime_simulation_tests PRIVATE INTERACTION_NO_OVERLAY INTERACTION_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/examples/arm_rotation_test.interaction-animation.json")
endif()
