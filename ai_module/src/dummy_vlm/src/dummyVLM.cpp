#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>
#include "rclcpp/rclcpp.hpp"

#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/pose2_d.hpp>
#include <visualization_msgs/msg/marker.hpp>

#include <tf2/transform_datatypes.h>
#include "tf2_ros/transform_broadcaster.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

using namespace std;

string question_file_dir;
string waypoint_file_dir;
string object_list_file_dir;
double waypointReachDis = 1.0;

vector<float> waypointX, waypointY, waypointHeading;

int objID;
float objMidX, objMidY, objMidZ, objL, objW, objH, objHeading;
string objLabel;

float vehicleX = 0, vehicleY = 0;

// reading question from file function
void readQuestionFile()
{
  ifstream question_file(question_file_dir.c_str());
  if (!question_file.is_open()) {
    printf ("\nCannot read input files, exit.\n\n");
    exit(1);
  }

  string line;
  getline(question_file, line);
  printf ("\n%s\n", line.c_str());
}

// reading waypoints from file function
void readWaypointFile()
{
  FILE* waypoint_file = fopen(waypoint_file_dir.c_str(), "r");
  if (waypoint_file == NULL) {
    printf ("\nCannot read input files, exit.\n\n");
    exit(1);
  }

  char str[50];
  int val, pointNum;
  string strCur, strLast;
  while (strCur != "end_header") {
    val = fscanf(waypoint_file, "%s", str);
    if (val != 1) {
      printf ("\nError reading input files, exit.\n\n");
      exit(1);
    }

    strLast = strCur;
    strCur = string(str);

    if (strCur == "vertex" && strLast == "element") {
      val = fscanf(waypoint_file, "%d", &pointNum);
      if (val != 1) {
        printf ("\nError reading input files, exit.\n\n");
        exit(1);
      }
    }
  }

  float x, y, heading;
  int val1, val2, val3;
  for (int i = 0; i < pointNum; i++) {
    val1 = fscanf(waypoint_file, "%f", &x);
    val2 = fscanf(waypoint_file, "%f", &y);
    val3 = fscanf(waypoint_file, "%f", &heading);

    if (val1 != 1 || val2 != 1 || val3 != 1) {
      printf ("\nError reading input files, exit.\n\n");
      exit(1);
    }

    waypointX.push_back(x);
    waypointY.push_back(y);
    waypointHeading.push_back(heading);
  }

  fclose(waypoint_file);
}

// reading objects from file function
void readObjectListFile()
{
  FILE* object_list_file = fopen(object_list_file_dir.c_str(), "r");
  if (object_list_file == NULL) {
    printf ("\nCannot read input files, exit.\n\n");
    exit(1);
  }
  
  char s[100], s2[100];
  int val1, val2, val3, val4, val5, val6, val7, val8, val9;
  val1 = fscanf(object_list_file, "%d", &objID);
  val2 = fscanf(object_list_file, "%f", &objMidX);
  val3 = fscanf(object_list_file, "%f", &objMidY);
  val4 = fscanf(object_list_file, "%f", &objMidZ);
  val5 = fscanf(object_list_file, "%f", &objL);
  val6 = fscanf(object_list_file, "%f", &objW);
  val7 = fscanf(object_list_file, "%f", &objH);
  val8 = fscanf(object_list_file, "%f", &objHeading);
  val9 = fscanf(object_list_file, "%s", s);
  
  if (val1 != 1 || val2 != 1 || val3 != 1 || val4 != 1 || val5 != 1 || val6 != 1 || val7 != 1 || val8 != 1 || val9 != 1) {
    exit(1);
  }

  while (s[strlen(s) - 1] != '"') {
    val9 = fscanf(object_list_file, "%s", s2);
      
    if (val9 != 1) break;

    strcat(s, " ");
    strcat(s, s2);
  }

  for (int i = 1; s[i] != '"' && i < 100; i++) objLabel += s[i];
}

// vehicle pose callback function
void poseHandler(const nav_msgs::msg::Odometry::ConstSharedPtr pose)
{
  vehicleX = pose->pose.pose.position.x;
  vehicleY = pose->pose.pose.position.y;
}

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  auto nh = rclcpp::Node::make_shared("dummyVLM");

  nh->declare_parameter<std::string>("question_file_dir", question_file_dir);
  nh->declare_parameter<std::string>("waypoint_file_dir", waypoint_file_dir);
  nh->declare_parameter<std::string>("object_list_file_dir", object_list_file_dir);
  nh->declare_parameter<double>("waypointReachDis", waypointReachDis);

  nh->get_parameter("question_file_dir", question_file_dir);
  nh->get_parameter("waypoint_file_dir", waypoint_file_dir);
  nh->get_parameter("object_list_file_dir", object_list_file_dir);
  nh->get_parameter("waypointReachDis", waypointReachDis);

  auto subPose = nh->create_subscription<nav_msgs::msg::Odometry>("/state_estimation", 5, poseHandler);

  auto pubWaypoint = nh->create_publisher<geometry_msgs::msg::Pose2D> ("/way_point_with_heading", 5);
  geometry_msgs::msg::Pose2D waypointMsgs;

  auto pubObjectMarker = nh->create_publisher<visualization_msgs::msg::Marker>("selected_object_marker", 5);
  visualization_msgs::msg::Marker objectMarkerMsgs;

  // read question from file
  readQuestionFile();

  // read waypoints from file
  readWaypointFile();

  int wayPointID = 0;
  int waypointNum = waypointX.size();

  if (waypointNum == 0) {
    printf ("\nNo waypoint available, exit.\n\n");
    exit(1);
  }

  // read waypoints from file
  readObjectListFile();

  sleep(2);

  // publish object marker
  objectMarkerMsgs.header.frame_id = "map";
  objectMarkerMsgs.header.stamp = nh->now();
  objectMarkerMsgs.ns = objLabel;
  objectMarkerMsgs.id = objID;
  objectMarkerMsgs.action = visualization_msgs::msg::Marker::ADD;
  objectMarkerMsgs.type = visualization_msgs::msg::Marker::CUBE;
  objectMarkerMsgs.pose.position.x = objMidX;
  objectMarkerMsgs.pose.position.y = objMidY;
  objectMarkerMsgs.pose.position.z = objMidZ;
  tf2::Quaternion quat_tf;
  quat_tf.setRPY(0, 0,  objHeading);
  geometry_msgs::msg::Quaternion geoQuat;
  tf2::convert(quat_tf, geoQuat);
  objectMarkerMsgs.pose.orientation = geoQuat;
  objectMarkerMsgs.scale.x = objL;
  objectMarkerMsgs.scale.y = objW;
  objectMarkerMsgs.scale.z = objH;
  objectMarkerMsgs.color.a = 0.5;
  objectMarkerMsgs.color.r = 0;
  objectMarkerMsgs.color.g = 0;
  objectMarkerMsgs.color.b = 1.0;
  pubObjectMarker->publish(objectMarkerMsgs);

  // publish fist waypoint
  waypointMsgs.x = waypointX[wayPointID];
  waypointMsgs.y = waypointY[wayPointID];
  waypointMsgs.theta = waypointHeading[wayPointID];
  pubWaypoint->publish(waypointMsgs);

  rclcpp::Rate rate(100);
  bool status = rclcpp::ok();
  while (status) {
    rclcpp::spin_some(nh);

    float disX = vehicleX - waypointX[wayPointID];
    float disY = vehicleY - waypointY[wayPointID];

    // move to the next waypoint and publish
    if (sqrt(disX * disX + disY * disY) < waypointReachDis && wayPointID < waypointNum - 1) {
      wayPointID++;

      waypointMsgs.x = waypointX[wayPointID];
      waypointMsgs.y = waypointY[wayPointID];
      waypointMsgs.theta = waypointHeading[wayPointID];
      pubWaypoint->publish(waypointMsgs);
    }

    status = rclcpp::ok();
    rate.sleep();
  }

  return 0;
}
