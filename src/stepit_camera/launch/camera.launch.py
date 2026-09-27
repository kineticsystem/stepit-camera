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
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
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

    return LaunchDescription(
        [
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
            camera,
        ]
    )
