/* PUT INIReader.h in same directory as this C++ program mycMotDetRecPy.cpp
 * You find INIReader.h at https://github.com/benhoyt/inih/blob/master/cpp/INIReader.h
 * 
 * PUT the MASK file "test_mask.png" (or your own mask file with any other name like "mask_stream2.png" also in the same directory
 * 
 * INSTALL the necessary compilation libraries for g++ and others
 * 
 * sudo apt update
 * sudo apt upgrade
 * sudo apt-get install build-essential cmake pkg-config libjpeg-dev libtiff5-dev libjasper-dev libpng-dev libavcodec-dev libavformat-dev libswscale-dev libv4l-dev libxvidcore-dev libx264-dev libfontconfig1-dev libcairo2-dev libgdk-pixbuf2.0-dev libpango1.0-dev libgtk2.0-dev libgtk-3-dev libatlas-base-dev gfortran libhdf5-dev libhdf5-serial-dev libhdf5-103 python3-pyqt5 python3-dev -y
 * sudo apt-get install libcurl4-gnutls-dev
 * sudo apt-get install libjsoncpp-dev
 * sudo ln -s /usr/include/jsoncpp/json/ /usr/include/json
 * sudo apt-get install libcurl4-openssl-dev
 * Probably needed as well to suppress messages Install only when they appear: sudo apt-get install libatk-adaptor libgail-common
 * 
 * COMPILE NOW THIS PROGRAM WITH THIS COMMAND, ASSUMING YOU HAVE INSTALLED ALL LIBs op opencv, libcur and jsonccp
 * COMPILE WITH THIS WITH g++  mycMotDetRecPyB.cpp -o mycMotDetRecPyB $(python3-config --ldflags --cflags --embed)  -I/usr/include/opencv4 -I/usr/include -lopencv_videoio -lopencv_video -lopencv_videostab -lopencv_core -lopencv_imgproc -lopencv_objdetect -lopencv_highgui -lopencv_imgcodecs -ljsoncpp  -lcurl 
 * 
 * */

#define PY_SSIZE_T_CLEAN
#define _GLIBCXX_USE_NANOSLEEP
#include <future>
#include <thread>
#include <Python.h>
#include <iostream>
#include <string>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include <opencv4/opencv2/videoio.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/video.hpp>
#include "./INIReader.h"
#include <curl/curl.h>
#include <json/json.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <semaphore.h>
#include <signal.h>
#include <queue>
#include <chrono>

using namespace cv;
using namespace std;

VideoWriter outputVideo;


// Define the function to be called when ctrl-c (SIGINT) is sent to process
void signal_callback_handler(int signum) {
   cout << "Caught signal CTRL-c. Program stops."  << endl;
   // Terminate program
   exit(signum);
}

// Function to split a string by delimiter
vector<string> splitString(const string& input, char delimiter) {
  vector<string> tokens;
  string token;
  istringstream tokenStream(input);
  while (getline(tokenStream, token, delimiter)) {
      tokens.push_back(token);
  }
  return tokens;
}


// Structure to hold object detection result
struct DetectionResult {
    std::string label;
    std::string confidence;
    cv::Rect boundingBox;
};


const int BUFFER_SIZE = 2048;

struct SharedMemory {
    char message[BUFFER_SIZE];
    sem_t mutex;
};

void getTapoMessages(SharedMemory* sharedMemory) {

      PyObject *pModuleName, *pModule, *pFunc;
      PyObject *pArgs, *pTupleValue, *pValue;
      const char*  pFunctionName;
      if (Py_IsInitialized() == false) {      
        Py_Initialize();
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append(\".\")");
      }

      /* Error checking of pName left out */

      pModuleName = PyUnicode_DecodeFSDefault("myTapoEvents");
      pFunctionName = "getTapoEventMessage";
      
      pModule = PyImport_Import(pModuleName);
      Py_DECREF(pModuleName);
      const char* savedString;

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, pFunctionName);
        /* pFunc is a new reference */
        pArgs = PyTuple_New(1);  // 1 means the length of the tuple!
         
        if (pFunc && PyCallable_Check(pFunc)) {

            // here we define a first tuple value, we pass the tuple to the function
            pTupleValue = PyUnicode_DecodeFSDefault("my test message");

            PyTuple_SetItem(pArgs, 0, pTupleValue);

            while (true) {
              pValue = PyObject_CallObject(pFunc, pArgs);
            
              if (pValue != NULL) {
                  // Convert the result to a C string
                  const char* resultString = PyUnicode_AsUTF8(pValue);
                  if (resultString) {
                      // Save the string value in a C++ variable
                      savedString = resultString;
                      // Print the saved string value
                      // std::cout << "Saved string value: " << savedString << std::endl;
                      //std::cout << savedString << std::endl;
                      // Write result to shared memory
                      sem_wait(&sharedMemory->mutex);
                      //snprintf(sharedMemory->message, BUFFER_SIZE, "AsyncTask1: IntValue = %d, StringValue = %s", intValue, stringValue.c_str());
                      snprintf(sharedMemory->message, BUFFER_SIZE, "%s", savedString);
                      sem_post(&sharedMemory->mutex); 
                      //sleep(1);   // slow down the speed 0.3 seconds, too fast leads to too much errors                   
                  }
                  Py_DECREF(pValue);
              }
              else {
                  Py_DECREF(pFunc);
                  Py_DECREF(pModule);
                  PyErr_Print();
                  fprintf(stderr,"Call failed\n");
                  //return;
              }
            } // END of while (true)
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function getTapoEventMessage\n");
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load myTapoEvents\n");
        //return;
    }
   
}

size_t write_data(void *ptr, size_t size, size_t nmemb, string *data) {
    data->append((char *)ptr, size * nmemb);
    return size * nmemb;
}

void postImageAndGetResponse(SharedMemory* sharedMemory, string& AIserverUrl, string& min_confidence, Mat& frame, string& show_AIResponse_message, string& show_AIObjDetectionResult, string& curl_debug_message_on) {
    CURL *curl;
    CURLcode res;
    string response_data;

    curl = curl_easy_init();
    if (curl) {
        if (curl_debug_message_on == "Yes") {
              curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }      
        // Convert the frame to JPEG format
        vector<uchar> buffer;
        //buffer for coding 
        //vector<int> param(2);
        //param[0] = cv::IMWRITE_JPEG_QUALITY; param[1] = 100; //default(95) 0-100 
        // cv::imencode(".jpg", frame, buffer, param);         
        cv::imencode(".jpg", frame, buffer);         
        // Save the frame into a file
        // imwrite("./temp_frame.jpg", frame); //
        // Mat frame = imread("./temp_frame.jpg"); // Load your image here.

        // string AIserverUrl = AIserverUrl; // set in the config file and passsed by main()
        // string min_confidence = "0.4";  // set in the config file and passsed by main()
        string output_obj_detection_filename = "picture_for_obj_detection.jpg";

        stringstream image_stream;
        for (const auto &byte : buffer) {
            image_stream << byte;
        }

        curl_mime *mime;
        curl_mimepart *part;

        mime = curl_mime_init(curl);

        part = curl_mime_addpart(mime);
        curl_mime_name(part, "min_confidence");
        curl_mime_data(part, min_confidence.c_str(), CURL_ZERO_TERMINATED);

        part = curl_mime_addpart(mime);
       // curl_mime_name(part, "typedfile");  // works for Code Project AI, not for Deepstack
        curl_mime_name(part, "image");   // works for both Code Project AI and Deepstack
        curl_mime_data(part, image_stream.str().c_str(), buffer.size());
        curl_mime_filename(part, output_obj_detection_filename.c_str());
        curl_mime_type(part, "image/jpeg");

        curl_easy_setopt(curl, CURLOPT_URL, AIserverUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        res = curl_easy_perform(curl);
        
        char time_now_buf[21];
        time_t now;
        time(&now);
        strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
            
        if (res != CURLE_OK) {
            cerr << "Access to AI Object Detection service failed: " << curl_easy_strerror(res) << "\nCheck URL; Is server running?" << endl;
            }    

      } // END if (curl)

      curl_easy_cleanup(curl);

      // Simulate some time-consuming operation
      // sleep(3);
      // Write result to shared memory
      sem_wait(&sharedMemory->mutex);
      // snprintf(sharedMemory->message, BUFFER_SIZE, "AsyncTask2: IntValue = %d, StringValue = %s", intValue, stringValue.c_str());
      snprintf(sharedMemory->message, BUFFER_SIZE, "%s", response_data.c_str());
      sem_post(&sharedMemory->mutex);
}

// Function to parse date string
std::chrono::system_clock::time_point parseDate(const std::string& dateStr) {
    std::tm tm = {};
    std::istringstream ss(dateStr.substr(22)); // Extract date part from text+date string
    // std::cout << dateStr.substr(22) << "\n\n";
    char delim;
    ss >> tm.tm_year >> delim >> tm.tm_mon >> delim >> tm.tm_mday >> delim
       >> tm.tm_hour >> delim >> tm.tm_min >> delim >> tm.tm_sec;
    tm.tm_year -= 1900; // Years since 1900
    tm.tm_mon -= 1;     // Month index starts from 0
    std::time_t t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

int main() {
    // Register signal and signal handler. Used to check if CTRL-c has been pressed in console!
    signal(SIGINT, signal_callback_handler);  
    
    // Create shared memory for getTapoMessages
    int shm_fd1 = shm_open("/my_shared_memory1", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd1, sizeof(SharedMemory));
    SharedMemory* sharedMemory1 = (SharedMemory*)mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0);
    sem_init(&sharedMemory1->mutex, 1, 1); // Initialize semaphore for getTapoMessages

    // Create shared memory for postImageAndGetResponse
    int shm_fd2 = shm_open("/my_shared_memory2", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd2, sizeof(SharedMemory));
    SharedMemory* sharedMemory2 = (SharedMemory*)mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
    sem_init(&sharedMemory2->mutex, 1, 1); // Initialize semaphore for postImageAndGetResponse

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child process (getTapoMessages)
        getTapoMessages(sharedMemory1);
        return 0;  // required!
    } else if (pid1 > 0) {
        // Parent process (main)
        // Do other work while getTapoMessages is running...
   
        
        INIReader reader("mycMotDetRecPyB_config.ini");
        if (reader.ParseError() < 0) {
            cout << "Error: Cannot load config.ini" << endl;
            return 1;
        }
        // CAMERA
        // Read variables from the .ini file
        // Full rtsp URL of IP camera inluding user name and password when required
        string url = reader.Get("camera", "url", "");
        
        // MOTION DETECTION
        // Path to the mask file
        string mask_path = reader.Get("motion_detection", "mask_path", "");
        // Warm up time. The time to be passed after start of program. Hereafter motion dectection will start.
        int warmup_time = reader.GetInteger("motion_detection", "warmup_time", 3);
        // Simulate a motion Default No. (used for test purposes)
        string simulate_a_motion = reader.Get("motion_detection", "simulate_a_motion", "No");
        // Show the display window in a resized frame (but not with a mask)
        string show_display_window = reader.Get("motion_detection", "show_display_window", "No");
        // Show the display window in its original frame (not resized)
        string show_display_window_not_resized = reader.Get("motion_detection", "show_display_window_not_resized", "No");
        // Show indicator that motion has been detected on display window       
        string show_motion_detected_msg_on_display_window = reader.Get("motion_detection", "show_motion_detected_msg_on_display_window", "No");
        // Show the fps and the date and time on the display window
        string show_motion_fps_date_msg_on_display_window = reader.Get("motion_detection", "show_motion_fps_date_msg_on_display_window", "No");
        // Show indicator that motion has been detected on display console (terminal window)
        string show_motion_detected_msg_on_display_console = reader.Get("motion_detection", "show_motion_detected_msg_on_display_console", "No");
        // Show the fps and the date and time on the display  console (terminal window)
        string show_motion_fps_date_msg_on_display_console = reader.Get("motion_detection", "show_motion_fps_date_msg_on_display_console", "No");
        
        // VIDEO RECORDING
        string output_video_path = reader.Get("video_recording", "output_video_path", "./");
        // Output prefix video file
        string prefix_output_video = reader.Get("video_recording", "prefix_output_video", "Vid_");
        // Extension of the video file
        string extension_of_video = reader.Get("video_recording", "extension_of_video", ".avi");
        // The Frames per second that your camera uses
        int fps = reader.GetInteger("video_recording", "fps", 15);
        // The codec to be used for writing the videos
        string codecString = reader.Get("video_recording", "codec", "XVID");
        float maximum_recording_time = reader.GetFloat("video_recording", "maximum_recording_time", 2.5);
        // Record duration in seconds
        int record_duration = reader.GetInteger("video_recording", "record_duration", 10);
        // Buffer x seconds the frames before a (new) motion is detected
        int buffer_before_motion = reader.GetInteger("video_recording", "buffer_before_motion", 5);
        // Extra time to add to the record duration if new motion is detected
        int extra_record_time = reader.GetInteger("video_recording", "extra_record_time", 5);
        // Pre-motion recording duration in second. The time before recording should stop wiil be used to detect uf new motion happened.
        int before_record_duration_is_passed = reader.GetInteger("video_recording", "before_record_duration_is_passed", 3);
        // Output video parameters
        string show_timing_for_recording = reader.Get("video_recording", "show_timing_for_recording", "No");
        
        // OBJECT DETECTION    
        // AI Object Detection Service URL
        string AIserverUrl = reader.Get("object_detection", "AIserverUrl", "http://localhost:80/v1/vision/detection");
        // Only when an AI object Detection service is installed Object Detection will happen
        string AIobject_detection_service = reader.Get("object_detection", "AIobject_detection_service", "No");
        // Thresholds for object detection: this is the minimum percentage (fraction) when an object signaled as recognised
        string min_confidence = reader.Get("motion_detection", "min_confidence", "0.4");  
        // Try to find the defined objects and signal them when as recognised
        string string_of_objects_for_detection = reader.Get("object_detection", "string_of_objects_for_detection", "person");    
        // Split the comma-separated string string_of_objects_for_detection into individual values
        vector<string> objects_for_detection = splitString(string_of_objects_for_detection, ',');
        // Try to find the defined objects and signal them when as recognised
        string draw_object_rectangles = reader.Get("object_detection", "draw_object_rectangles", "person");    
        // Object detection will repeat after x seconds (default 5) when motion has been detected
        int object_detection_time = reader.GetInteger("object_detection", "object_detection_time", 3);
       // Output picture parameters
        string output_obj_picture_path = reader.Get("object_detection", "output_obj_picture_path", "./");
       // Output prefix picture file
        string prefix_output_picture = reader.Get("object_detection", "prefix_output_picture", "Pic_");
        // Show the full json response message from the AI Object Detection Service
        string show_AIResponse_message = reader.Get("object_detection", "show_AIResponse_message", "No");
        // Show the result(s) from the response message from the AI Object Detection Service
        string show_AIObjDetectionResult = reader.Get("object_detection", "show_AIObjDetectionResult", "No");    // When No motion rectangles will be drawn on the screen around the moving objects
        // Show the result(s) from the response message from the AI Object Detection Service
        string curl_debug_message_on = reader.Get("object_detection", "curl_debug_message_on", "No");    // When No motion rectangles will be drawn on the screen around the moving objects

        // Load the DUMMY mask. The black areas's in the mask are always excluded from Motion detection!
        vector<Point> my_poly = {Point(0, 0), Point(2560,0), Point(2560, 1440), Point(0, 1440)};
        Mat mask  = Mat::zeros(1, 1, CV_8U);
        fillPoly(mask, my_poly, Scalar::all(1));
        // Invert the mask so that black becomes non-transparent and white becomes transparent
        mask = 255 - mask;    

        // Fork a new process for CURL asyncTask called postImageAndGetResponse
        pid_t pid2 = fork();
        if (pid2 == 0) {
          if (AIobject_detection_service == "Yes") {
            postImageAndGetResponse(sharedMemory2,  AIserverUrl,  min_confidence,  mask,  show_AIResponse_message,  show_AIObjDetectionResult, curl_debug_message_on);                      
          }
          return 0; // required!

        } else if (pid2 > 0) {
          // Parent process (main)
          } else {
            // Fork for postImageAndGetResponse failed
            std::cerr << "Fork for postImageAndGetResponse failed!" << std::endl;
            return 1;
          } 
        
        // Initialize camera
        VideoCapture cap(url);
        if (!cap.isOpened()) {
            cout << "Error: Cannot open the camera." << endl;
            return -1;
        }
        cap.set(cv::CAP_PROP_BUFFERSIZE, fps);  // do buffer frames
        int frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        int frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        
        Size frameSize = Size(frame_width, frame_height);
        char time_now_buf[21];
        time_t now;
        time(&now);
        strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
        cout << "Camera started @ " << time_now_buf << endl;
        cout << "Videos are saved @ " << output_video_path << endl;
        cout << "Pictures are saved @ " << output_obj_picture_path << endl;
        if (AIobject_detection_service == "Yes") {
          cout << "Detect Objects, but save ONLY pictures of: " << string_of_objects_for_detection << endl;
        }

        VideoWriter outputVideo;


        // Initialize variables for recording
        auto timeCamera = parseDate( "Motion detected: Yes @ 2001-01-01 01:01:01");     
        auto start_timeCamera = parseDate( "Motion detected: Yes @ 2001-01-01 01:01:01");  
        
        int frameCounter = 0;
        int teller = 0;
        int obj_detection_each_x_frames = fps * object_detection_time;
        
         
        bool recording_on = false;
        int new_record_duration = record_duration;
        
        int bufferSeconds = buffer_before_motion;
        // Set buffer size in frames
        int bufferSize = bufferSeconds * fps;
        queue<Mat> frameBuffer;        
        
        while (true) {
            // Read frame from the camera
            Mat frame;
            Mat frame_original;
            bool motion_detected;
            cap.read(frame);
            if (frame.empty()) {
                break;
            }
            frame_original = frame.clone();
            frameCounter += 1;
            string str_frameCounter = to_string(frameCounter);             
            time(&now);
            strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
                
            // Add frame to buffer
            frameBuffer.push(frame.clone());
            // Maintain buffer size
            if (frameBuffer.size() > bufferSize) {
                frameBuffer.pop();  // removes first frame from buffer 
            }            
            
          

                // Read result from shared memory for getTapoMessages
                sem_wait(&sharedMemory1->mutex);
                std::string messages_data(sharedMemory1->message);
                sem_post(&sharedMemory1->mutex);
                
                      
                if (show_motion_fps_date_msg_on_display_console == "Yes") {
                  // put frame number and time on screen
                  // [xK Erases part of the line. If n is 0 (or missing), 
                  // clear from cursor to the end of the line. 
                  // If n is 1, clear from cursor to beginning of the line. 
                  // If n is 2, clear entire line. Cursor position does not change. 
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
                  cout << "@" << str_frameCounter << " " << time_now_buf  << ". Camera: " << messages_data << "\r"; 
                }
                if (show_motion_fps_date_msg_on_display_window == "Yes") {
                  // put frame number and time on screen
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
                  putText(frame_original, '@'+str_frameCounter + " " + time_now_buf, Point(10,frame_height - 20), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0,255,88),2);
                }
                       
                motion_detected = false;
                
                // Check for motion after a warm up time to avoid initial writing of a file
                if (frameCounter >= warmup_time * fps) {
                      // Print the result for getTapoMessages
                      // std::cout << "frameCounter:" << frameCounter << " Result from getTapoMessages asynchronous task: " << messages_data << std::endl;

                      if (messages_data.rfind("Motion detected: Yes @", 0) == 0) { // pos=0 limits the search to the prefix
                         motion_detected = true; 
                         timeCamera = parseDate(messages_data);                    
                      }
                      if (messages_data.rfind("Motion detected: No @", 0) == 0) { 
                         timeCamera = parseDate(messages_data); 
                      }

                    // Print the result
                    // std::cout << "Result from asynchronous task: " << messages_data << std::endl;
                }  // END of if (frameCounter >= warmup_time * fps) 

            // When motion has been detected and displays are required
            if (motion_detected) {
              if (show_motion_detected_msg_on_display_window == "Yes") {          
                //    cout << "Motion detected" << endl;
                // put frame number and time on screen
                putText(frame_original, "motion detected", Point(10, frame_height - 40), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0,255,88),2);
              }
              if (show_motion_detected_msg_on_display_console == "Yes") {          
                //    cout << "Motion detected" << endl;
                // put frame number and time on screen
                cout << "motion detected                        \033[K\r";
              } 
            } // END of if (motion_detected) {

 
          // This simulation is meant for test purposes e.g. to verify the AI Object detection and saving of pictures                 
          if (simulate_a_motion == "Yes") {
              if (frameCounter > 100 and frameCounter < 300 or frameCounter > 400 and frameCounter < 600) {motion_detected = true;} else {motion_detected = false;}
          }


          // If recording is on, then check if extra record time must be added and check if record duration has been passed 
          if (recording_on) {
                // Calculate delta in seconds
                std::chrono::duration<double> delta_rec = timeCamera - start_timeCamera ;
                if (motion_detected) {
                  if (delta_rec.count() >= new_record_duration - before_record_duration_is_passed) {
                    new_record_duration = new_record_duration + extra_record_time;
                  }
                }
                if (delta_rec.count() >= maximum_recording_time * 60) {
                  new_record_duration = record_duration;
                }
                if (show_timing_for_recording == "Yes") {
                  cout << "motion:" <<motion_detected << " timeCamera - start_timeCamera: " << delta_rec.count() << " new_record_duration: " << new_record_duration << " before_record_duration_is_passed: " << before_record_duration_is_passed << " extra time: " << extra_record_time << endl;
                }                
          }  // END of First! if (recording_on)           
                          
          // If motion detected, start recording and set the record duration
          if (motion_detected and not recording_on) {
                  recording_on = true;
                  start_timeCamera = timeCamera;
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));              
                  cout << "Recording started @ " + string(time_now_buf) << endl;
                  // set the codec and create VideoWriter object
                  outputVideo.open(output_video_path + prefix_output_video + time_now_buf + extension_of_video,  VideoWriter::fourcc(codecString[0], codecString[1], codecString[2], codecString[3]), fps, frameSize, true);                
                  cout << "To be saved in: " << output_video_path + prefix_output_video + time_now_buf + extension_of_video << endl;
          } // END of if (motion_detected and not recording_on) 

          // If recording is on, check if standard record duration plus applicable extra record time has been passed. 
          if (recording_on) {
              // Check if it's time to stop recording
              // Calculate delta in seconds
              std::chrono::duration<double> delta_rec = timeCamera - start_timeCamera;
              // cout << delta_rec.count() << " > " << new_record_duration << " ?" << endl;
              if (delta_rec.count() > new_record_duration) {
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));          
                  cout << "Recording stopped @ " + string(time_now_buf) << "\033[K\n\n";
                  recording_on = false;
                  new_record_duration = record_duration; // reset to start value
                  // Release the VideoWriter object
                  if (!outputVideo.isOpened()) {
                      cerr << "Could not open the output video file for release\n";
                      return -1;
                  }    
                  outputVideo.release();
              } // END of if chrono::system_clock::now() >= end_time)
              else {          
                  // Write frame to the output video  
                  if (!outputVideo.isOpened()) {
                      cerr << "Could not open the output video file for write\n";
                      return -1;
                  }  

                  // Write buffered frames to file
                  if (!frameBuffer.empty()) {
                      while (!frameBuffer.empty()) {
                          teller += 1;
                          outputVideo.write(frameBuffer.front());
                          frameBuffer.pop();
                      } 
                      // cout << frameCounter << " <= Total frame counter | # written frames from buffer => " << teller << endl;
                  }
                  else {          
                  outputVideo.write(frame_original);
                  }
              } // END of else branch of if (time(0) >= end_time)
          } // END of Second! if (recording_on)
          
          if (recording_on == true and AIobject_detection_service == "Yes") {
            // Initialize variables for a new object detection, object_detection_time is defined in config
            int modulo_result_AI = frameCounter % obj_detection_each_x_frames;
            if (modulo_result_AI == 0) {  
              // Define vector for the extraction of  object detection results
              vector<DetectionResult> detectionResults;   
              // Child process (postImageAndGetResponse)
              // Post the image to the AI Object Detection Service and get the detectionResults from the response
              postImageAndGetResponse(sharedMemory2,  AIserverUrl,  min_confidence,  frame,  show_AIResponse_message,  show_AIObjDetectionResult, curl_debug_message_on);                      

              // Read result from shared memory for postImageAndGetResponse
              sem_wait(&sharedMemory2->mutex);
              std::string response_data(sharedMemory2->message);
              sem_post(&sharedMemory2->mutex);
              
              // check if there is some content in the shared buffer
              if (strlen(sharedMemory2->message) > 0) {
                // Print the result for postImageAndGetResponse
                // std::cout << "Result from postImageAndGetResponse asynchronous task: " << response_data << std::endl;
                
                // Parsing JSON response
                Json::Value jsonData;
                Json::Value root {};
                Json::Reader jsonReader;
                if (jsonReader.parse(response_data, jsonData)) {
                  if (show_AIResponse_message == "Yes") {
                    time(&now);
                    strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));              
                    cout << time_now_buf << " Successfully parsed JSON data" << endl;
                    cout << time_now_buf << " JSON data received:" << endl;
                    cout << time_now_buf << jsonData.toStyledString() << endl;
                    //  const string aiPredictions(jsonData["predictions"].asString());
                  }  // END  if (show_AIResponse_message == "Yes")
                  if (show_AIObjDetectionResult == "Yes") {
                    const string aiMessage(jsonData["message"].asString());                  
                    const string aiSuccess(jsonData["success"].asString());                   
                    // Extract object detection results
                    for (const auto& prediction : jsonData["predictions"]) {
                      DetectionResult result;
                      result.label = prediction["label"].asString();
                      result.confidence = prediction["confidence"].asString();
                      // prepare confidence fraction (e.g.0.8334) for a percentage (e.g. 83.34%) in cout
                      float conf;
                      conf = prediction["confidence"].asFloat() * 100.0;
                      time(&now);
                      strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));                      
                      cout << time_now_buf << " AI Object Detection service message: Success is " << aiSuccess << ". Found: " << result.label << ". Confidence: " << setprecision(3) << conf << "%" << endl; 
                      // Drawing a rectangle in OpenCV with C++ behaves differently than in Python
                      // In C++ you need Rect((x,y), (x+width_offset, y+height_offset)
                      result.boundingBox = Rect(prediction["x_min"].asInt(), prediction["y_min"].asInt(),
                                           prediction["x_max"].asInt() - prediction["x_min"].asInt() , prediction["y_max"].asInt() - prediction["y_min"].asInt());
                      detectionResults.push_back(result);
                    } // END of for (const auto& prediction : jsonData["predictions"])
                  } // END of if (show_AIObjDetectionResult == "Yes")
                } // END of if (jsonReader.parse(response_data, jsonData))
                else 
                {
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
                  cout  << time_now_buf << " Could not parse HTTP data as JSON" << endl;
                  cout  << time_now_buf << " HTTP data was:\n" << response_data << endl;
                } // END of else branch of if (jsonReader.parse(response_data, jsonData))
              } // END of if (strlen(sharedMemory2->message) > 0)

              // Clean up
              sem_destroy(&sharedMemory1->mutex);
              sem_destroy(&sharedMemory2->mutex);
              shm_unlink("/my_shared_memory1");
              shm_unlink("/my_shared_memory2");
      
              // evaluate the detection results of the AI Object Detection service
              for (const auto& result : detectionResults) {
                // Verify if result.label is equal to one of the values defined in objects_for_detection
                // if found then we will draw rectangles and label. In Python it would be: if "car" in ['car', person', 'bicycle']
                bool found = false;
                for (const string& value : objects_for_detection) {
                  if (result.label == value) { 
                    found = true;
                    break;
                  }
                } // END for (const string& value : objects_for_detection)  
                if (found) {
                  // Draw rectangles and labels for each detected object if required
                  if (draw_object_rectangles == "Yes") {
                    // Draw a rectangle around the detected object
                    rectangle(frame_original, result.boundingBox, Scalar(0, 255, 0), 2);
                    // Put the label at the top left corner of the rectangle
                    if(result.boundingBox.y - 10 < 10) {
                      putText(frame_original, result.label, Point(result.boundingBox.x, result.boundingBox.y + 10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2); 
                      }
                      else {
                    putText(frame_original, result.label, Point(result.boundingBox.x, result.boundingBox.y - 10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);
                    }
                  }
                  time(&now);
                  strftime(time_now_buf, 21, "%Y_%m_%d_%H_%M_%S", localtime(&now));
                  imwrite(output_obj_picture_path + prefix_output_picture + time_now_buf + "_" + result.label + ".jpg", frame_original);  
                  cout << "Saved: " << output_obj_picture_path + prefix_output_picture + time_now_buf + "_" + result.label + ".jpg" << endl;
                  } // END if (found) 
              }  // END for (const auto& result : detectionResults)                  
            } // END of if (modulo_result_AI == 0)  
          } // END  if (recording_on == false and AIobject_detection_service == "Yes") 
          
          if (show_display_window_not_resized == "Yes") {
              // Display the resulting frame
              imshow("Motion Detection Original Format", frame_original);
          }
         
          if (show_display_window == "Yes") {
              Mat frame_original_resized;
              resize(frame_original, frame_original_resized, Size(640,370));     
              // Display the resulting frame
              imshow("Motion Detection", frame_original_resized);
          }

          // Check for key press to exit
          if (waitKey(1) == 'q') {
                break;
          }
        } // END of  while (true) 

        Py_FinalizeEx();
        // Release the camera
        cap.release();
        destroyAllWindows();

        // return 0;

    } else {
        // Fork for getTapoMessages failed
        std::cerr << "Fork for getTapoMessages failed!" << std::endl;
        return 1;
    }
    Py_FinalizeEx();

    return 0;
}
