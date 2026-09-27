# One setting of the camera, e.g. the ISO.

# The name of the ROS parameter that controls it, e.g. shutter_speed.
string name

# Its current value on the camera, e.g. 1/125.
string value

# The values the camera accepts right now. They depend on the mode dial and on
# the lens, e.g. the aperture cannot be set in Tv mode.
string[] choices
