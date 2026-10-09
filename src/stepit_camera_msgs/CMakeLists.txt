cmake_minimum_required(VERSION 3.16)
project(stepit_camera_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  msg/CameraStatus.msg
  msg/Picture.msg
  msg/Setting.msg
  srv/GetSettings.srv
  DEPENDENCIES std_msgs)

ament_export_dependencies(rosidl_default_runtime)

ament_package()
