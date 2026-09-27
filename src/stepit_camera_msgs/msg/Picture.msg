# A picture taken by the camera, downloaded as soon as the camera reported it.

# The stamp is the time the picture was downloaded, not the time of the shot:
# it comes a fraction of a second after the shutter closes, more for a large
# RAW file.
std_msgs/Header header

# The name of the file on the camera, e.g. IMG_0042.JPG or IMG_0042.CR2. With
# RAW+JPEG, the camera reports two files for one shot, and each one gets its
# own message.
string name

# The folder of the file on the camera, e.g. /store_00010001/DCIM/100CANON.
string folder

# Where the picture was saved on the computer, or empty if it was not saved.
string path

# The content of the file, exactly as the camera stored it.
uint8[] data
