set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
  main.cpp
  main_window.cpp main_window.h main_window.ui
  remote_widget.cpp remote_widget.h remote_widget.ui
  transfer_dialog.cpp transfer_dialog.h transfer_dialog.ui
  export_dialog.cpp export_dialog.h export_dialog.ui
  progress_dialog.cpp progress_dialog.h progress_dialog.ui
  job_widget.cpp job_widget.h job_widget.ui
  mount_widget.cpp mount_widget.h mount_widget.ui
  stream_widget.cpp stream_widget.h stream_widget.ui
  preferences_dialog.cpp preferences_dialog.h preferences_dialog.ui
  icon_cache.cpp icon_cache.h
  item_model.cpp item_model.h
  rclone_output.cpp rclone_output.h
  rclone_updater.cpp rclone_updater.h
  theme.cpp theme.h
  utils.cpp utils.h
  job_options.cpp job_options.h
  list_of_job_options.cpp list_of_job_options.h
  pch.h
  resources.qrc
)

if(WIN32)
  set(TARGET_NAME RcloneBrowser)
  list(APPEND SOURCES resources.rc)
elseif(APPLE)
  set(TARGET_NAME rclone-browser)
  list(APPEND SOURCES osx_helper.h osx_helper.mm icon.icns)
  set_source_files_properties(icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  # Objective-C++ file must not use the C++ precompiled header.
  set_source_files_properties(osx_helper.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
else()
  set(TARGET_NAME rclone-browser)
endif()

add_executable(${TARGET_NAME} WIN32 MACOSX_BUNDLE ${SOURCES})

target_compile_definitions(${TARGET_NAME} PRIVATE
  RCLONE_BROWSER_VERSION="${RCLONE_BROWSER_VERSION}"
  QT_NO_DEPRECATED_WARNINGS_SINCE=0x050F00
)
if(WIN32)
  target_compile_definitions(${TARGET_NAME} PRIVATE
    _UNICODE UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

target_precompile_headers(${TARGET_NAME} PRIVATE pch.h)

# Warnings are on, but never fatal by default: -Werror turned every new
# compiler release into a build break for packagers. Opt in with RB_WERROR=ON.
option(RB_WERROR "Treat compiler warnings as errors" OFF)
if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4 /wd4100 /wd4189 /utf-8 /MP)
  if(RB_WERROR)
    target_compile_options(${TARGET_NAME} PRIVATE /WX)
  endif()
else()
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
  if(RB_WERROR)
    target_compile_options(${TARGET_NAME} PRIVATE -Werror)
  endif()
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
  Qt::Core Qt::Gui Qt::Widgets Qt::Network)

if(WIN32)
  target_link_libraries(${TARGET_NAME} PRIVATE shell32 ole32)
elseif(APPLE)
  find_library(COCOA_LIB Cocoa REQUIRED)
  target_link_libraries(${TARGET_NAME} PRIVATE ${COCOA_LIB})
  set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
else()
  include(GNUInstallDirs)
  install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  set(ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
  install(FILES "${ASSETS}/rclone-browser.svg"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
  foreach(size 32 64 128 256 512)
    install(FILES "${ASSETS}/rclone-browser-${size}x${size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
            RENAME "rclone-browser.png")
  endforeach()
  install(FILES "${ASSETS}/rclone-browser.desktop"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES "${ASSETS}/rclone-browser.appdata.xml"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
endif()

# Parser unit tests (no GUI, no rclone needed).
option(RB_BUILD_TESTS "Build unit tests" ON)
if(RB_BUILD_TESTS)
  find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test)
  if(Qt${QT_VERSION_MAJOR}Test_FOUND)
    enable_testing()
    add_executable(test_rclone_output ../tests/test_rclone_output.cpp rclone_output.cpp rclone_output.h)
    target_include_directories(test_rclone_output PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_rclone_output PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_rclone_output PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME rclone_output COMMAND test_rclone_output)
  endif()
endif()
