MOTION DETECTION WITH RECORDING AND OBJECT IDENTIFICATION ON AN IP CAMERA SUPPORTING ONVIF (PROFILE S).

Required files
..............
mycMotDetRecPy                  is the C++ program to start on Raspberry Pi 4 /(64Bit Bookworm) in a terminal with ./mycMotDetRecPy, with suffix Ubuntu it stands for Ubuntu 64 bit 23.10)
mycMotDetRecPy_config.ini       contains all settings to control the program and access the camera stream and optional an AI Object Detection server
mycMotDetRecPy.cpp              contains the C++ source code
myTapoEvents.py                 contains the ONVIF based access to the Camera to subscribe for Event messages. 
                                Can be run separately as well in a terminal window: python3 myTapoEvents.py
test_mask.png                   a mask that is for testing purposes
INIReader_DUMMY.h               a dummy, download the original INIReader.h from here https://github.com/benhoyt/inih
INIReader.h                     a missing file! required to download yourself, see line above

Capabilities
............
- 2 Possible methods for Motion Detection
  Method 1: consumes more power, motion detection is done by detection of changes between frames using a Background subtraction method (choose between "KNN" or "MOG2")
            you configure once the sensitivity of detection and a Mask to exclude certain areas of the camera picture for Motion detection. 
  Method 2: uses Python for ONVIF event queries (message subscription) to the IP camera, The camera is doing the heavy lifting (motion detection) and signals a motion
  
- When a motion is signalled the recording of the stream to a file will be triggered. You specify for how long recording lasts. A maximum per file can be specified to avoid huge files
  You specify the codec if you want H264 or X264 or MPEG or XVID to mention a few
  The file location for the recording can be defined
  
- Optional you can use an AI Object Server (CodeProject AI Server and Deep Stack have been tested) to detect a person, car, dog or other supported object.
  When an object is detected a jpg Picture will be saved in the location you defined in the ini file
  
- You can set many options to Yes or No. Example are: show a Display Window of the Stream of the Stream with Mask, of a Full Pixels Screen to create a Mask
  Messages and/or rectangles can displayed in the Stream on Screen and Pictures of the objects and also on Console (terminal)

- Get familiar with the many options by playing and trying to dtermine the best settings for your situation.
  Start with the defaults
  
- You can compile the source code also on Ubuntu or other Unix platforms (Debian based and perhaps others as well)

- This was created on Raspberry PI 4 (64 bit Bookworm) running with 4GB RAM. 1080x720 streams work really good. I tried also with 2560*1440. 
  The results are pretty good when you use the Python Method 2 option (see above), which is checking the IP camera for motion events. This is valid for the PI 4 processor (not overclocked!)
Method 1 consumes too much power and the results for stream 1 (2560x1440) pixels will be not perfect too some times poor...

The following "B" versions support ONLY Method 2 with the embedded Python interpreter. The mycMotDetRecPyB_config.ini is therefore stripped of some unused parameters! 
The difference here is that the Timing for recording is based on the parsed Time from the ONVIF XML messages send by the camera.
The examples above use for recording timing the clock of the PC/ Raspberry Pi 4.
mycMotDetRecPyB                  is the C++ program to start on Raspberry Pi 4 (64Bit Bookworm) in a terminal with ./mycMotDetRecPy
mycMotDetRecPyB_config.ini       contains all settings to control the program and access the camera stream and optional an AI Object Detection server
mycMotDetRecPyB.cpp              contains the C++ source code



Some usage guidelines:
- First note that the configuration settings in mycMotDetRecPy_config.ini MUST be adapted to your situation else it won't work!
- To get the correct Recording settings for your camera and connection speed to the computer where you run this program you need to experiment with the following values:
    buffer_before_motion = 3                   ; Int default 5 s
    record_duration = 10                       ; Int default 10 s
    extra_record_time = 5                      ; Int default 5 s
    before_record_duration_is_passed = 5       ; Int default 5 s.  
- Start with lower values and increase them step by step. Often the first seconds the camera signals new motions, so it may be wise to set before_record_duration_is_passed somewhat higher at the start.
  e.g record_duration = 10 and before_record_duration_is_passed = 9. Step by step you reduce the before_record_duration_is_passed to avoid to much extra recording time at the end!
- You can alos reduce the extra_record_time.  The behaviour on the Raspberry PI 4 or an Ubuntu Server can lead to diffference due to processor speed and perhaps network connectivity
- Play with it till you get results you like. Usually a few rounds will be enough 
- Set parameter show_timing_for_recording = Yes  during this testing

Compilation guideline 
.....................
(copied from top of the source code file mycMotDetRecPy.cpp)
/* PUT INIReader.h in same directory as this C++ program mycMotDetRec.cpp
 * You find INIReader.h at https://github.com/benhoyt/inih/blob/master/cpp/INIReader.h
 * 
 * PUT the MASK file "test_mask.png" (or your own mask file with any other name like "mask_stream2.png" also in the same directory
 * 
 * INSTALL the necessary compilation libraries for g++ and others
 * sudo apt update
 * sudo apt upgrade
 * sudo apt-get install build-essential cmake pkg-config libjpeg-dev libtiff5-dev libjasper-dev libpng-dev libavcodec-dev libavformat-dev libswscale-dev libv4l-dev libxvidcore-dev libx264-dev libfontconfig1-dev libcairo2-dev libgdk-pixbuf2.0-dev libpango1.0-dev libgtk2.0-dev libgtk-3-dev libatlas-base-dev gfortran libhdf5-dev libhdf5-serial-dev libhdf5-103 python3-pyqt5 python3-dev -y
 * sudo apt-get install libcurl4-gnutls-dev
 * sudo apt-get install libjsoncpp-dev
 * sudo ln -s /usr/include/jsoncpp/json/ /usr/include/json
 * sudo apt-get install libcurl4-openssl-dev
 * 
 * COMPILE NOW THIS PROGRAM WITH THIS COMMAND, ASSUMING YOU HAVE INSTALLED ALL LIBs op opencv, libcur and jsonccp
 * g++  mycMotDetRec.cpp -o mycMotDetRec   -I/usr/include/opencv4 -I/usr/include -lopencv_videoio -lopencv_video -lopencv_videostab -lopencv_core -lopencv_imgproc -lopencv_objdetect -lopencv_highgui -lopencv_imgcodecs -ljsoncpp  -lcurl
 *
*/
