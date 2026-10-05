# Copyright 2026 Giovanni Remigi
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""
Start the camera driver. Pass fake:=true to run a fake camera, with no
hardware, and stream:=true to start the live view straight away.

For web pages, e.g. the test page of the camera, it also starts:

- the web server, which serves the test page and the pictures over HTTP on
  port web_port (default 8090);
- web_video_server, which serves the live view on port web_video_port
  (default 8081);
- rosbridge, which serves the services and topics on port rosbridge_port
  (default 9091).

Pass web:=false, web_video:=false or rosbridge:=false to leave them out.

This rosbridge runs here because only it knows the messages of the camera,
stepit_camera_msgs. Its port is not rosbridge's default, 9090, so that it can
run next to another rosbridge on the same machine.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    parameters = PathJoinSubstitution(
        [FindPackageShare("stepit_camera"), "config", "camera.yaml"]
    )

    camera = Node(
        package="stepit_camera",
        executable="camera_node",
        name="camera",
        output="screen",
        parameters=[
            parameters,
            LaunchConfiguration("params_file"),
            {
                "fake_camera": ParameterValue(
                    LaunchConfiguration("fake"), value_type=bool
                ),
                "stream_on_start": ParameterValue(
                    LaunchConfiguration("stream"), value_type=bool
                ),
            },
        ],
    )

    # The test page at http://<host>:8090, and each saved picture at
    # http://<host>:8090/pictures/<name>. It reads download_directory from the
    # same file as the camera node.
    web = Node(
        package="stepit_camera",
        executable="web_server",
        name="web_server",
        output="screen",
        parameters=[
            parameters,
            LaunchConfiguration("params_file"),
            {"port": ParameterValue(LaunchConfiguration("web_port"), value_type=int)},
        ],
        condition=IfCondition(LaunchConfiguration("web")),
    )

    # A page shows the live view with an <img> of
    # http://<host>:8081/stream?topic=/camera/preview&type=ros_compressed,
    # which passes the JPEG frames of the camera through, without decoding
    # them.
    web_video = Node(
        package="web_video_server",
        executable="web_video_server",
        name="web_video_server",
        output="screen",
        parameters=[
            {
                "port": ParameterValue(
                    LaunchConfiguration("web_video_port"), value_type=int
                )
            }
        ],
        condition=IfCondition(LaunchConfiguration("web_video")),
    )

    rosbridge = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(
            PathJoinSubstitution(
                [
                    FindPackageShare("rosbridge_server"),
                    "launch",
                    "rosbridge_websocket_launch.xml",
                ]
            )
        ),
        launch_arguments={"port": LaunchConfiguration("rosbridge_port")}.items(),
        condition=IfCondition(LaunchConfiguration("rosbridge")),
    )

    return LaunchDescription(
        [
            # Loaded after camera.yaml, and before the launch arguments: a
            # robot that runs the camera sets its own values, e.g.
            # download_directory, without changing this package.
            DeclareLaunchArgument(
                "params_file",
                default_value=parameters,
                description="A parameter file loaded after camera.yaml, for both the camera and the web server",
            ),
            DeclareLaunchArgument(
                "fake",
                default_value="false",
                description="Run a fake camera instead of the one connected over USB",
            ),
            DeclareLaunchArgument(
                "stream",
                default_value="false",
                description="Start the live view as soon as the camera connects",
            ),
            DeclareLaunchArgument(
                "web",
                default_value="true",
                description="Start the web server, for the test page and the pictures",
            ),
            DeclareLaunchArgument(
                "web_port",
                default_value="8090",
                description="The HTTP port of the web server",
            ),
            DeclareLaunchArgument(
                "web_video",
                default_value="true",
                description="Start web_video_server, to show the live view in web pages",
            ),
            DeclareLaunchArgument(
                "web_video_port",
                default_value="8081",
                description="The HTTP port of web_video_server",
            ),
            DeclareLaunchArgument(
                "rosbridge",
                default_value="true",
                description="Start rosbridge, for web pages to call the camera's services",
            ),
            DeclareLaunchArgument(
                "rosbridge_port",
                default_value="9091",
                description="The WebSocket port of rosbridge",
            ),
            camera,
            web,
            web_video,
            rosbridge,
        ]
    )
