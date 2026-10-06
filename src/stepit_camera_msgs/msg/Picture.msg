# A picture taken by the camera, downloaded as soon as the camera reported it,
# and saved. The message says where the file is, not what is in it: a RAW file
# is tens of megabytes, too heavy for DDS. Read the file at path, or load it
# from the web server of stepit_camera, at /pictures/<file name of path>.

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

# Where the picture was saved on the computer, e.g.
# /home/developer/ws/pictures/IMG_0042.CR2. The file name may differ from name:
# a file is never overwritten, so a name that comes back gets a suffix, e.g.
# IMG_0001_1.JPG.
string path

# The same file, relative to the camera's download_directory, with / between
# folders, e.g. 2026-10-06/angle_01/IMG_0042.CR2, in the subfolder the camera's
# parameter folder named: the web server serves it at /pictures/<relative_path>.
string relative_path
