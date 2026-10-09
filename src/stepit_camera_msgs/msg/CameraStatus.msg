# Whether the node talks to a camera, published on ~/status, e.g.
# /camera/status. Latched: published when it changes and every second, so
# that a subscriber that stops receiving it knows that the node is gone.

# The stamp is the time it was published.
std_msgs/Header header

# True from the moment the camera is opened until it is lost, e.g. switched
# off or unplugged. The node then tries again every reconnect_period, and
# connects once it is back.
bool connected

# The model of the camera, e.g. "Canon EOS 600D"; empty when not connected.
string device

# Why it is not connected; empty when it is.
string message
