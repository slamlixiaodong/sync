#include<ros/ros.h>
#include<image_transport/image_transport.h>
#include<cv_bridge/cv_bridge.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "sensor_msgs/TimeReference.h"
#include "../include/cam/USBDev/USBDev.h"
#include <iostream>
#include "cam/hardsync.h"

using namespace std;
sensor_msgs::ImagePtr image_msg;
cv::Mat image(480,752,CV_8UC1);
uint32_t trigger_counter;
int flag = 0;
UINT32 loglevel = 2;
class SubcribleAndPublish
{
    public: SubcribleAndPublish()
    {
        ser_ = n_.serviceClient<cam::hardsync>("dji_sdk/set_hardsyc");
        cam::hardsync srv;
        srv.request.frequency = 20;
        srv.request.tag = 0;
        if(!ser_.call(srv))
        {
            ROS_ERROR("SERVICE IS FAILED!!!");
        }
        image_transport::ImageTransport it(n_);
        pub_ = it.advertise("cam/image_raw",1);
        sub_ = n_.subscribe("dji_sdk/trigger_time", 1000, &SubcribleAndPublish::imu_time_callback,this);
	DEV_OpenDevice();
        DEV_SetDeviceParameter(0xFFFF, SET_LOG_LEVEL, &loglevel);
        DEV_SetDeviceParameter(0xFFFF, SET_TRIGGER_MODE, &loglevel);
        DEV_SetDeviceParameter(0xFFFF, SET_TRIGGER_PIN_MODE, &loglevel);
        DEV_SetDeviceParameter(0xFFFF, SET_TRIGGER_ONCE, &loglevel); 
    }
    void imu_time_callback(const sensor_msgs::TimeReference& msg)
    {
    DEV_GetFrame(0, 0, 2, &frameInfo);
    image.data=frameInfo.pFrameBuffer;
    //cv::imshow("image",image);
    //cv::waitKey(30);
    image_msg = cv_bridge::CvImage(std_msgs::Header(),"mono8", image).toImageMsg();
    image_msg->header.frame_id="wolrd";
    image_msg->header.stamp = msg.header.stamp;    
    pub_.publish(image_msg);
    //trigger_counter = msg.header.seq; 
    }
    private:
    ros::NodeHandle n_;
    ros::ServiceClient ser_;
    ros::Subscriber sub_;
    image_transport::Publisher pub_;
    DEV_FRAME_INFO frameInfo;
};
int main(int argc,char **argv)
{
    ros::init(argc, argv, "sub_timeAndpub_image");
    SubcribleAndPublish image;    
    ros::spin();
    return 0;
}
