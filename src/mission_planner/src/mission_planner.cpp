#include <mission_planner.h>

Mission::Mission():
  n(ros::this_node::getName())
{
  request_sub = nh.subscribe("/parcel_dispatcher/next_product", 1, &Mission::request_callBack, this);

  //IS A hack (delete)
  //estimated_pose_sub = nh.subscribe("/kalman_filter/pose_estimation", 1, &Mission::pose_estimation_callback, this);
  estimated_pose_sub = nh.subscribe("/firefly/ground_truth/pose", 1, &Mission::pose_estimation_callback, this);
  //change the desired pose topic
  desired_pose_pub = n.advertise<geometry_msgs::PoseStamped>("/firefly/command/pose", 1000);
  feedback_pub = nh.advertise<final_aerial_project::ProductFeedback>("/firefly/product_feedback", 1000, true);
  aruco_ventral_client = nh.serviceClient<std_srvs::SetBool>("aruco_detect_ventral/enable_detections");
  aruco_frontal_client = nh.serviceClient<std_srvs::SetBool>("aruco_detect_frontal/enable_detections");
  planner_client = nh.serviceClient<mav_planning_msgs::PlannerService>("/firefly/voxblox_rrt_planner/plan");
  //plan_client = nh.serviceClient<std_srvs::SetBool>("aruco_detect_horizontal/enable_detections");
  //publish_plan_client = nh.serviceClient<std_srvs::SetBool>("aruco_detect_horizontal/enable_detections");
  search_sub = nh.subscribe("firefly/transform_pose/estimated_point", 1, &Mission::search_callBack, this);
  transform_pub = nh.advertise<mission_planner::transform_type>("firefly/transform_pose/mission", 1000, true);

  last_time = ros::Time::now().sec;

  base_pose.pose.position.x = 21.8276;
  base_pose.pose.position.y = 3.13698;
  base_pose.pose.position.z = 0;
  base_pose.pose.orientation.x = 0;
  base_pose.pose.orientation.y = -0;
  base_pose.pose.orientation.z = -0;
  base_pose.pose.orientation.w = 1.0;

  //filling the pose array_of_pose
  fill_array(3.5, 7.0, 0.707, 0.707);
  fill_array(9.5, 12.0, 0.383, 0.924);
  fill_array(15.0, 17.0, 0.383, 0.924);
  fill_array(22.0, 17.0, 0.0, 1.0);
  fill_array(30.0, 17.0, 0.0, 1.0);
  fill_array(30.0, 11.5, -0.707, 0.707);
  fill_array(25.0, 7.0, 1.0, 0.0);
  fill_array(21.0, 7.0, -0.707, 0.707);
  fill_array(30.0, 11.5, 0.383, 0.924);
  fill_array(30.0, 17.0, 0.707, 0.707);
  fill_array(22.0, 17.0, 1.0, 0.0);
  fill_array(15.0, 17.0, 1.0, 0.0);
  fill_array(15.0, 25.0, 0.707, 0.707);
  fill_array(10.0, 25.0, 1.0, 0.0);
  fill_array(10.0, 21.0, -0.707, 0.707);
  fill_array(3.0, 21.0, 1.0, 0.0);
  fill_array(3.0, 23.0, 0.0, 1.0);
  //activate_camera(1, false);
}

void Mission::fill_array(double x, double y, double z, double w)
{
  geometry_msgs::Pose pose_array;
  pose_array.position.x = x;
  pose_array.position.y = y;
  pose_array.position.z = 1.0;
  pose_array.orientation.x = 0.0;
  pose_array.orientation.y = 0.0;
  pose_array.orientation.z = z;
  pose_array.orientation.w = w;
  array_of_pose.push_back(pose_array);
}


void Mission::request_callBack(final_aerial_project::ProductInfo product_info)
{
  if(product_info.item_location == "SHELVE")
  {
    if(mission_code == -1) mission_code_stored = 1;
    else mission_code = 1;
  }
  else if(product_info.item_location == "HUSKY")
  {
    if(mission_code == -1) mission_code_stored = 2;
    else mission_code = 2;
  }
  else if(product_info.item_location == "END")
  {
    if(mission_code == -1) mission_code_stored = 3;
    else mission_code = 3;
  }
  target_id = product_info.marker_id;
  approximate_pose =  product_info.approximate_pose;
  ROS_INFO("POSrec X: %f", approximate_pose.pose.position.x);
  ROS_INFO("POSrec Y: %f", approximate_pose.pose.position.y);
  ROS_INFO("POSrec Z: %f", approximate_pose.pose.position.z);
}

void Mission::search_callBack(geometry_msgs::Point search_point)
{
  search_done = true;
  marker_position = search_point;
}

void Mission::pose_estimation_callback(geometry_msgs::Pose estimated_pose)
{
  estimated_pose_filter.pose = estimated_pose;
  estimated_pose_filter.header.stamp = ros::Time::now();
  if(equal_pose_with_tolerance(estimated_pose, array_of_pose[pos_array]))
  {
    ROS_INFO("PRUEBA REACHED");
    target_reached_array_pose = true;
    pos_array++;
  }
  if(equal_pose_with_tolerance(estimated_pose, actual_pose.pose)) target_reached_actual_pose = true;
  if(equal_pose_with_tolerance(estimated_pose, base_pose.pose)) target_reached_base_pose = true;
}

bool Mission::equal_pose_with_tolerance(geometry_msgs::Pose pose1, geometry_msgs::Pose pose2)
{
  double tolerance = 0.2;
  if(pose1.position.x-tolerance < pose2.position.x && pose2.position.x < pose1.position.x+tolerance)
  {
    //ROS_INFO("X IS OK");
    if(pose1.position.y-tolerance < pose2.position.y && pose2.position.y < pose1.position.y+tolerance)
    {
      //ROS_INFO("Y IS OK");
      if(pose1.position.z-tolerance < pose2.position.z && pose2.position.z < pose1.position.z+tolerance)
      {
        //ROS_INFO("Z IS OK");
        return true;
      }
      //is hack (delete)
      else
      {
        if(going_to_request_pose)
        {
          actual_pose.pose = approximate_pose.pose;
          actual_pose.header.stamp = ros::Time::now();
          desired_pose_pub.publish(actual_pose);
        }
        if(going_to_base_pose)
        {
          base_pose.pose.position.z = 0.8;
          desired_pose_pub.publish(base_pose);
        }
      }
    }
  }
  return false;
}


void Mission::do_mission()
{
  switch (mission_code)
  {
    case -1:
      //ROS_INFO("CASE -1");
      if(target_reached_array_pose && (ros::Time::now().sec - last_time) > 10)
      {
        ROS_INFO("TARGET REACHED");
        if(pos_array < max_pose_array)
        {
          ROS_INFO("MOVING");
          last_time = ros::Time::now().sec;
          actual_pose.pose = array_of_pose[pos_array];
          actual_pose.header.stamp = ros::Time::now();
          desired_pose_pub.publish(actual_pose);
          target_reached_array_pose = false;
        }
        else
        {
          mission_code = mission_code_stored;
        }
      }
    break;
    case 0:
    break;
    case 1:
      //ROS_INFO("FRONTAL SEARCH");
      activate_camera(1, true);
      actual_pose.pose = approximate_pose.pose;
      //this is a hack (delete)
      actual_pose.pose.position.z = 6;
      going_to_request_pose = true;
      //ROS_INFO("POS X: %f", actual_pose.pose.position.x);
      //ROS_INFO("POS Y: %f", actual_pose.pose.position.y);
      //ROS_INFO("POS Z: %f", actual_pose.pose.position.z);
      actual_pose.header.stamp = ros::Time::now();
      ask_planner(actual_pose);
      //desired_pose_pub.publish(actual_pose);
      mission_type.frontal_search = true;
      mission_type.ventral_search = false;
      mission_type.target_id = target_id;
      transform_pub.publish(mission_type);
      mission_code = 4;
    break;
    case 2:
      //ROS_INFO("VENTRAL SEARCH");
      //modify waiting parameteres
      actual_pose.pose = approximate_pose.pose;
      //this is a hack (delete)
      actual_pose.pose.position.z = 6;
      going_to_request_pose = true;
      actual_pose.header.stamp = ros::Time::now();
      ask_planner(actual_pose);
      //desired_pose_pub.publish(actual_pose);
      mission_type.frontal_search = false;
      mission_type.ventral_search = true;
      mission_type.target_id = target_id;
      transform_pub.publish(mission_type);
      mission_code = 5;
    break;
    case 3:
      //go start
    break;
    case 4:
      //waiting frontal status
      if(search_done)
      {
        ROS_INFO("SEARCH DONE");
        search_done = false;
        target_reached_base_pose = false;
        axis_x_iteration = 0;
        axis_z_iteration = 0;
        mission_code = 6;
        going_to_base_pose = true;
        base_pose.header.stamp = ros::Time::now();
        base_pose.pose.position.z = 3.0;
        //desired_pose_pub.publish(base_pose);
        ask_planner(base_pose);
        break;
      }
      if(target_reached_actual_pose) //SEND NEW POSE TO REALIZE A S
      {
        going_to_request_pose = false;
        if(axis_x_iteration < 6)
        {
          ROS_INFO("MOVING IN X");
          actual_pose.pose.position.x += x_added;
          axis_x_iteration++;
        }
        else
        {
          if(axis_z_iteration < 8)
          {
            ROS_INFO("MOVING IN Z");
            x_added = -x_added;
            actual_pose.pose.position.z += 0.5;
            axis_x_iteration = 0;
            axis_z_iteration++;
          }
          else
          {
            //COULDNT FIND THE MARKER
            ROS_INFO("COULDNT FIND THE OBJECT");
            axis_x_iteration = 0;
            axis_z_iteration = 0;
          }
        }
        actual_pose.header.stamp = ros::Time::now();
        //desired_pose_pub.publish(actual_pose);
        ask_planner(actual_pose);
        target_reached_actual_pose = false;
      }
    break;
    case 5:
      //waiting ventral status
      if(search_done)
      {
        ROS_INFO("SEARCH DONE");
        search_done = false;
        target_reached_actual_pose = false;
        axis_x_iteration = 0;
        axis_y_iteration = 0;
        mission_code = 6;
        base_pose.header.stamp = ros::Time::now();
        base_pose.pose.position.z = 6.0;
        going_to_base_pose = true;
        //desired_pose_pub.publish(base_pose);
        ask_planner(base_pose);
        break;
      }
      if(target_reached_actual_pose) //SEND NEW POSE TO REALIZE A S
      {
        going_to_request_pose = false;
        if(axis_y_iteration < 6)
        {
          actual_pose.pose.position.y -= y_added;
          axis_y_iteration++;
        }
        else
        {
          if(axis_x_iteration < 8)
          {
            y_added = -y_added;
            actual_pose.pose.position.x += 0.5;
            axis_y_iteration = 0;
            axis_x_iteration++;
          }
          else
          {
            //COULDNT FIND THE MARKER
            ROS_INFO("COULDNT FIND THE OBJECT");
            axis_x_iteration = 0;
            axis_y_iteration = 0;
          }
        }
        actual_pose.header.stamp = ros::Time::now();
        //desired_pose_pub.publish(actual_pose);
        ask_planner(actual_pose);
        target_reached_actual_pose = false;
      }
    break;
    case 6:
      //when reaching base publish feedback and mission 0
      if(target_reached_base_pose)
      {
        going_to_base_pose = false;
        ROS_INFO("PUBLISHING FEEDBACK");
        feedback.marker_id = target_id;
        feedback.approximate_position = marker_position;
        feedback_pub.publish(feedback);
        //CARE
        mission_code = 0;
        target_reached_base_pose = false;
      }
    break;
  }
}

void Mission::ask_planner(geometry_msgs::PoseStamped goalPose)
{
  planner_request.request.start_pose = estimated_pose_filter;
  planner_request.request.goal_pose = goalPose;
  planner_client.call(planner_request);
}

void Mission::activate_camera(int camera_id, bool activate)
{
  if (camera_id == 0)
  {
    ventral_request.request.data = activate;
    aruco_ventral_client.call(ventral_request);
  }
  else if(camera_id == 1)
  {
    frontal_request.request.data = activate;
    aruco_frontal_client.call(frontal_request);
  }
}
