cmake_minimum_required(VERSION 3.16)
project(stepit_camera)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

find_package(ament_cmake REQUIRED)
find_package(JPEG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(stepit_camera_msgs REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GPHOTO2 REQUIRED IMPORTED_TARGET libgphoto2)

######################################################
# The driver, as a library, so that it can be tested. Shared, so that its
# private dependencies, libgphoto2 and libjpeg, stay out of its interface.

add_library(stepit_camera SHARED
  src/camera_driver.cpp
  src/camera_node.cpp
  src/fake_camera.cpp
  src/gphoto_camera.cpp
  src/settings.cpp)

target_include_directories(stepit_camera PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(stepit_camera
  PUBLIC
  rclcpp::rclcpp
  ${sensor_msgs_TARGETS}
  ${std_srvs_TARGETS}
  ${stepit_camera_msgs_TARGETS}
  Threads::Threads
  PRIVATE
  PkgConfig::GPHOTO2
  JPEG::JPEG)

######################################################
# The node.

add_executable(stepit_camera_node src/main.cpp)
set_target_properties(stepit_camera_node PROPERTIES OUTPUT_NAME camera_node)
target_link_libraries(stepit_camera_node PRIVATE stepit_camera)

######################################################
# INSTALL

install(DIRECTORY include/ DESTINATION include)

install(TARGETS stepit_camera
  EXPORT export_stepit_camera
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS stepit_camera_node DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME}/)

ament_export_targets(export_stepit_camera HAS_LIBRARY_TARGET)
ament_export_dependencies(
  rclcpp
  sensor_msgs
  std_srvs
  stepit_camera_msgs)

ament_package()
