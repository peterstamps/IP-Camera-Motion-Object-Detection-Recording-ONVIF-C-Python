# -*- coding: utf-8 -*-
from onvif import ONVIFCamera
from configparser import ConfigParser 
import asyncio
import datetime as dt
from datetime import datetime
from typing import Any, Callable
from dateutil import tz
import logging
from time import sleep

async def eventMsg(myinput=None):
  try:
    ini_file = 'mycMotDetRecPy_config.ini'
    configur = ConfigParser() 
    ret = configur.read(ini_file)
    if ret == []:
      print(f"myTapoEvents.py reports Error: the ini file is not found: {ini_file}")
    cameraIP  = configur.get('python_onvif','cameraIP')
    cameraUser  = configur.get('python_onvif','cameraUser')
    cameraPassw  = configur.get('python_onvif','cameraPassw')
    cameraOnvifPort   = configur.get('python_onvif','cameraOnvifPort',fallback=80)
    cameraOnvif_wsdl_dir = configur.get('python_onvif','cameraOnvif_wsdl_dir', fallback='./wsdl/')
    cameraPrintMessages = configur.get('python_onvif','cameraPrintMessages',fallback='No')
    cameraLogMessages = configur.get('python_onvif','cameraLogMessages', fallback='info').lower()
    if cameraLogMessages.lower()  == "debug":
      logging.getLogger("zeep").setLevel(logging.DEBUG)
      logging.getLogger("httpx").setLevel(logging.DEBUG)
    elif cameraLogMessages.lower()  == "info":
      logging.getLogger("zeep").setLevel(logging.INFO)
      logging.getLogger("httpx").setLevel(logging.INFO)
    elif cameraLogMessages.lower() == "critical":
      logging.getLogger("zeep").setLevel(logging.CRITICAL)
      logging.getLogger("httpx").setLevel(logging.CRITICAL)
    
    mycam = ONVIFCamera(
        cameraIP,
        int(cameraOnvifPort) ,
        cameraUser,
        cameraPassw,  
        cameraOnvif_wsdl_dir,
        )
    # Update xaddrs for services 
    await mycam.update_xaddrs() 
    #device_mgmt = await mycam.create_devicemgmt_service()
    #device_info = await device_mgmt.GetDeviceInformation()
    #print(device_info)
    #params = {'IncludeCapability': True }
    #services = await device_mgmt.GetServices(params)
    #print(device_info)
    #await mycam.update_xaddrs() 
    # Create a pullpoint manager. 
    interval_time = (dt.timedelta(seconds=15))
    pullpoint_mngr = await mycam.create_pullpoint_manager(interval_time, subscription_lost_callback = Callable[[], None],)
    # create the pullpoint  
    pullpoint = await mycam.create_pullpoint_service()
    
    # call SetSynchronizationPoint to generate a notification message too ensure the webhooks are working.
    #await pullpoint.SetSynchronizationPoint()
   
    # pull the cameraMessages from the camera, set the request parameters
    # by setting the pullpoint_req.Timeout you define the refreshment speed of the pulls
    pullpoint_req = pullpoint.create_type('PullMessages') 
    pullpoint_req.MessageLimit=10
    pullpoint_req.Timeout = (dt.timedelta(days=0,hours=0,seconds=1))
    cameraMessages = await pullpoint.PullMessages(pullpoint_req)

    if cameraMessages:
      # renew the subscription makes sense when looping over 
     # termination_time = (
     #  (dt.datetime.utcnow() + dt.timedelta(days=1,hours=1,seconds=1))
     #     .isoformat(timespec="seconds").replace("+00:00", "Z")
     # )
      # Auto-detect zones:
      from_zone = tz.tzutc()
      to_zone = tz.tzlocal()
      utc = f"{cameraMessages['CurrentTime'].strftime('%Y-%m-%d %H:%M:%S')}"
      # utc = datetime.utcnow()
      utc = datetime.strptime(utc, '%Y-%m-%d %H:%M:%S')
      #print(cameraMessages)
      # utc = datetime.utcnow()
      # Tell the datetime object that it's in UTC time zone since 
      # datetime objects are 'naive' by default
      utc = utc.replace(tzinfo=from_zone)
      # Convert time zone
      local_time = utc.astimezone(to_zone)
      if cameraMessages['NotificationMessage'] != []:
        ret_message = f"Motion detected @ {local_time.strftime('%Y-%m-%d %H:%M:%S')} "
      else:
        ret_message = f"No motion detected @ {local_time.strftime('%Y-%m-%d %H:%M:%S')}"   
    else:
      ret_message = f"No message from camera. Only {myinput}"     
    await pullpoint.close()
    await mycam.close()  
    return ret_message
  except:
     return "No message from camera. Maybe an error occurred 1." 

def getTapoEventMessage(myinput=None):
  try:
    loop = asyncio.get_event_loop()
    msg = loop.run_until_complete(eventMsg(myinput))
    #print(msg)
    return str(msg) 
  except:
    return "No message from camera. Maybe an error occurred 2." 
    

if __name__ == '__main__':
  while True:
    sleep(1)
    print(getTapoEventMessage(myinput=None))

  
