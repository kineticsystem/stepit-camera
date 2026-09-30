cmake_minimum_required(VERSION 3.16)
project(stepit_camera_tests)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

find_package(ament_cmake REQUIRED)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(sensor_msgs REQUIRED)
  find_package(std_srvs REQUIRED)
  find_package(stepit_camera REQUIRED)
  find_package(stepit_camera_msgs REQUIRED)

  # Matching values to the camera's choices, and saving pictures.
  ament_add_gtest(test_settings tests/test_settings.cpp)
  target_link_libraries(test_settings stepit_camera::stepit_camera)

  # The fake camera the other tests run against.
  ament_add_gtest(test_fake_camera tests/test_fake_camera.cpp)
  target_link_libraries(test_fake_camera stepit_camera::stepit_camera)

  # The driver's thread: streaming, downloading, reconnecting.
  ament_add_gtest(test_camera_driver tests/test_camera_driver.cpp)
  target_link_libraries(test_camera_driver stepit_camera::stepit_camera)

  # The ROS2 interface, end to end against the fake camera.
  ament_add_gtest(test_camera_node tests/test_camera_node.cpp TIMEOUT 120)
  target_link_libraries(test_camera_node
    stepit_camera::stepit_camera
    rclcpp::rclcpp
    ${sensor_msgs_TARGETS}
    ${std_srvs_TARGETS}
    ${stepit_camera_msgs_TARGETS})

  # The test page and the pictures over HTTP.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(HTTPLIB REQUIRED IMPORTED_TARGET cpp-httplib)
  ament_add_gtest(test_web_server tests/test_web_server.cpp)
  target_link_libraries(test_web_server stepit_camera::stepit_camera PkgConfig::HTTPLIB)
endif()

ament_package()
