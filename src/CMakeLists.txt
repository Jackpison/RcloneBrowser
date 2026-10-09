set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
  main.cpp
  main_window.cpp main_window_shell.cpp main_window.h main_window.ui
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
else()
  set(TARGET_NAME rclone-browser)
endif()

add_executable(${TARGET_NAME} WIN32 ${SOURCES})

target_compile_definitions(${TARGET_NAME} PRIVATE
  RCLONE_BROWSER_VERSION="${RCLONE_BROWSER_VERSION}"
  QT_NO_DEPRECATED_WARNINGS_SINCE=0x050F00
)
if(WIN32)
  target_compile_definitions(${TARGET_NAME} PRIVATE
    _UNICODE UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

target_precompile_headers(${TARGET_NAME} PRIVATE pch.h)

# Every SVG in icons/fluent is compiled in automatically as :/fluent/<name>.svg
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/fluent/*.svg")
qt_add_resources(${TARGET_NAME} "fluent_icons"
  PREFIX "/fluent"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/fluent"
  FILES ${FLUENT_ICONS})

# Folder icon, pre-rendered at each size the app uses (crisp outline)
file(GLOB FOLDER_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/folder/*.png")
qt_add_resources(${TARGET_NAME} "folder_icons"
  PREFIX "/folder"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/folder"
  FILES ${FOLDER_ICONS})
file(GLOB DRIVE_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/drive/*.png")
qt_add_resources(${TARGET_NAME} "drive_icons"
  PREFIX "/drive"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/drive"
  FILES ${DRIVE_ICONS})

# Warnings are on, but never fatal by default: -Werror turned every new
# compiler release into a build break for packagers. Opt in with RB_WERROR=ON.
option(RB_WERROR "Treat compiler warnings as errors" OFF)
if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4 /wd4100 /wd4189 /utf-8 /MP)
  # Exploit mitigations: Control Flow Guard, CET shadow-stack compatibility,
  # high-entropy ASLR and DEP (the last two are MSVC defaults, set explicitly).
  target_compile_options(${TARGET_NAME} PRIVATE /guard:cf)
  target_link_options(${TARGET_NAME} PRIVATE /guard:cf /CETCOMPAT /DYNAMICBASE
                      /HIGHENTROPYVA /NXCOMPAT)
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
  Qt::Core Qt::Gui Qt::Widgets Qt::Network Qt::Svg)

if(WIN32)
  target_link_libraries(${TARGET_NAME} PRIVATE shell32 ole32 dwmapi advapi32)
endif()

# Linux install (used by the Flatpak build; the AppImage is assembled by CI).
if(UNIX AND NOT APPLE)
  include(GNUInstallDirs)
  set(PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/linux")
  install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES "${PKG_DIR}/rclone-browser.desktop"
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES "${PKG_DIR}/io.github.jackpison.RcloneBrowser.metainfo.xml"
          DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install(FILES "${PKG_DIR}/rclone-browser.png"
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
  file(GLOB REMOTE_LOGOS "${CMAKE_CURRENT_SOURCE_DIR}/../icons/remotes/*.png")
  install(FILES ${REMOTE_LOGOS}
          DESTINATION ${CMAKE_INSTALL_DATADIR}/rclone-browser/icons/remotes)
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
