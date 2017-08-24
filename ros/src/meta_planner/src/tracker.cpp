/*
 * Copyright (c) 2017, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Authors: David Fridovich-Keil   ( dfk@eecs.berkeley.edu )
 */

///////////////////////////////////////////////////////////////////////////////
//
// Defines the Tracker class.
//
///////////////////////////////////////////////////////////////////////////////

#include <meta_planner/tracker.h>

namespace meta {

Tracker::Tracker()
  : initialized_(false),
    tf_listener_(tf_buffer_) {}

Tracker::~Tracker() {}

// Initialize this class with all parameters and callbacks.
bool Tracker::Initialize(const ros::NodeHandle& n) {
  name_ = ros::names::append(n.getNamespace(), "tracker");

  if (!LoadParameters(n)) {
    ROS_ERROR("%s: Failed to load parameters.", name_.c_str());
    return false;
  }

  if (!RegisterCallbacks(n)) {
    ROS_ERROR("%s: Failed to register callbacks.", name_.c_str());
    return false;
  }

  // Set control upper/lower bounds as Eigen::Vectors.
  VectorXd control_upper_vec(control_dim_);
  VectorXd control_lower_vec(control_dim_);
  for (size_t ii = 0; ii < control_dim_; ii++) {
    control_upper_vec(ii) = control_upper_[ii];
    control_lower_vec(ii) = control_lower_[ii];
  }

  // NOTE: Do these need to be relative dynamics?
  dynamics_ = NearHoverQuadNoYaw::Create(control_lower_vec, control_upper_vec);

  // Initialize state space. For now, use an empty box.
  // TODO: Parameterize this somehow and integrate with occupancy grid.
  space_ = BallsInBox::Create();

  // Set state space bounds.
  VectorXd state_upper_vec(state_dim_);
  VectorXd state_lower_vec(state_dim_);
  for (size_t ii = 0; ii < state_dim_; ii++) {
    state_upper_vec(ii) = state_upper_[ii];
    state_lower_vec(ii) = state_lower_[ii];
  }

  space_->SetBounds(dynamics_->Puncture(state_lower_vec),
                    dynamics_->Puncture(state_upper_vec));

  // Set the initial state and goal.
  const size_t kXDim = 0;
  const size_t kYDim = 2;
  const size_t kZDim = 4;
  const size_t kVxDim = 1;
  const size_t kVyDim = 3;
  const size_t kVzDim = 5;

  const double kSmallNumber = 1.5;

  state_ = 0.5 * (state_lower_vec + state_upper_vec);
  state_(kVxDim) = 0.0;
  state_(kVyDim) = 0.0;
  state_(kVzDim) = 0.0;

  goal_ = state_upper_vec - VectorXd::Constant(state_dim_, kSmallNumber);
  goal_(kVxDim) = 0.0;
  goal_(kVyDim) = 0.0;
  goal_(kVzDim) = 0.0;

  first_time_ = true;

  // Create planners.
  for (size_t ii = 0; ii < value_directories_.size(); ii++) {
    // NOTE: Assuming the 6D quadrotor model and geometric planner in 3D.
    // Load up the value function.
    const ValueFunction::ConstPtr value =
      ValueFunction::Create(value_directories_[ii], dynamics_,
                            state_dim_, control_dim_);

    // Create the planner.
    const Planner::ConstPtr planner =
      OmplPlanner<og::RRTConnect>::Create(value, space_);

    planners_.push_back(planner);
  }

  // Generate an initial trajectory.
  RunMetaPlanner();

  // Publish environment.
  space_->Visualize(environment_pub_, fixed_frame_id_);

  // Wait a little for the simulator to begin.
  ros::Duration(0.5).sleep();

  initialized_ = true;
  return true;
}

// Load all parameters from config files.
bool Tracker::LoadParameters(const ros::NodeHandle& n) {
  std::string key;

  // Control parameters.
  if (!ros::param::search("meta/control/time_step", key)) return false;
  if (!ros::param::get(key, time_step_)) return false;

  int dimension = 1;
  if (!ros::param::search("meta/control/dim", key)) return false;
  if (!ros::param::get(key, dimension)) return false;
  control_dim_ = static_cast<size_t>(dimension);

  if (!ros::param::search("meta/control/upper", key)) return false;
  if (!ros::param::get(key, control_upper_)) return false;

  if (!ros::param::search("meta/control/lower", key)) return false;
  if (!ros::param::get(key, control_lower_)) return false;
  if (control_upper_.size() != control_dim_ ||
      control_lower_.size() != control_dim_) {
    ROS_ERROR("%s: Upper and/or lower bounds are in the wrong dimension.",
              name_.c_str());
    return false;
  }

  // Planner parameters.
  if (!ros::param::search("meta/planners/values", key)) return false;
  if (!ros::param::get(key, value_directories_)) return false;
  if (value_directories_.size() == 0) {
    ROS_ERROR("%s: Must specify at least one value function directory.",
              name_.c_str());
    return false;
  }

  // State space parameters.
  if (!ros::param::search("meta/state/dim", key)) return false;
  if (!ros::param::get(key, dimension)) return false;
  state_dim_ = static_cast<size_t>(dimension);

  if (!ros::param::search("meta/state/upper", key)) return false;
  if (!ros::param::get(key, state_upper_)) return false;

  if (!ros::param::search("meta/state/lower", key)) return false;
  if (!ros::param::get(key, state_lower_)) return false;

  // Topics and frame ids.
  if (!ros::param::search("meta/topics/control", key)) return false;
  if (!ros::param::get(key, control_topic_)) return false;

  if (!ros::param::search("meta/topics/sensor", key)) return false;
  if (!ros::param::get(key, sensor_topic_)) return false;

  if (!ros::param::search("meta/topics/known_environment", key)) return false;
  if (!ros::param::get(key, environment_topic_)) return false;

  if (!ros::param::search("meta/topics/traj", key)) return false;
  if (!ros::param::get(key, traj_topic_)) return false;

  if (!ros::param::search("meta/topics/tracking_bound", key)) return false;
  if (!ros::param::get(key, tracking_bound_topic_)) return false;

  if (!ros::param::search("meta/frames/fixed", key)) return false;
  if (!ros::param::get(key, fixed_frame_id_)) return false;

  if (!ros::param::search("meta/frames/tracker", key)) return false;
  if (!ros::param::get(key, tracker_frame_id_)) return false;

  if (!ros::param::search("meta/frames/planner", key)) return false;
  if (!ros::param::get(key, planner_frame_id_)) return false;

  return true;
}

// Register all callbacks and publishers.
bool Tracker::RegisterCallbacks(const ros::NodeHandle& n) {
  ros::NodeHandle nl(n);

  // Sensor subscriber.
  sensor_sub_ = nl.subscribe(
    sensor_topic_.c_str(), 10, &Tracker::SensorCallback, this);

  // Visualization publisher(s).
  environment_pub_ = nl.advertise<visualization_msgs::Marker>(
    environment_topic_.c_str(), 10, false);

  traj_pub_ = nl.advertise<visualization_msgs::Marker>(
    traj_topic_.c_str(), 10, false);

  tracking_bound_pub_ = nl.advertise<visualization_msgs::Marker>(
    tracking_bound_topic_.c_str(), 10, false);

  control_pub_ = nl.advertise<geometry_msgs::Vector3>(
    control_topic_.c_str(), 10, false);

  // Timer.
  timer_ =
    nl.createTimer(ros::Duration(time_step_), &Tracker::TimerCallback, this);

  return true;
}


// Callback for processing sensor measurements. Replan trajectory.
void Tracker::SensorCallback(const geometry_msgs::Quaternion::ConstPtr& msg){
  const Vector3d point(msg->x, msg->y, msg->z);
  const double radius = msg->w;

  // Check if our version of the map has already seen this point.
  if (!(space_->IsObstacle(point, radius))) {
    space_->AddObstacle(point, radius);

    // Run meta_planner.
    RunMetaPlanner();

    // Publish environment.
    space_->Visualize(environment_pub_, fixed_frame_id_);
  }
}


// Callback for applying tracking controller.
void Tracker::TimerCallback(const ros::TimerEvent& e) {
  ros::Time current_time = ros::Time::now();

  // Rerun the meta planner if the current time is past the end of the
  // trajectory timeline.
  if (current_time.toSec() > traj_->LastTime()) {
    ROS_WARN("%s: Current time is past the end of the planned trajectory.",
             name_.c_str());

    // Run the meta planner.
    RunMetaPlanner();

    current_time = ros::Time::now();
  }

  // TODO! In a real (non-point mass) system, we will need to query some sort of
  // state filter to get our current state. For now, we just query tf and get position.

  // 0) Get current TF.
  geometry_msgs::TransformStamped tf;

  try {
    tf = tf_buffer_.lookupTransform(
      fixed_frame_id_.c_str(), tracker_frame_id_.c_str(), ros::Time(0));
  } catch(tf2::TransformException &ex) {
    ROS_WARN("%s: %s", name_.c_str(), ex.what());
    ROS_WARN("%s: Could not determine current state.", name_.c_str());
    ros::Duration(time_step_).sleep();
    return;
  }

  // 1) Compute relative state.
  // NOTE: right now velocity calculation is totally not portable...
  if (!first_time_) {
    state_(1) = (tf.transform.translation.x - state_(0)) / time_step_;
    state_(3) = (tf.transform.translation.y - state_(2)) / time_step_;
    state_(5) = (tf.transform.translation.z - state_(4)) / time_step_;
  } else {
    state_(1) = 0.0;
    state_(3) = 0.0;
    state_(5) = 0.0;
    first_time_ = false;
  }

  std::cout << "state: " << state_.transpose() << std::endl;

  state_(dynamics_->SpatialDimension(0)) = tf.transform.translation.x;
  state_(dynamics_->SpatialDimension(1)) = tf.transform.translation.y;
  state_(dynamics_->SpatialDimension(2)) = tf.transform.translation.z;

  const VectorXd planner_state = traj_->GetState(current_time.toSec());
  const VectorXd relative_state = state_ - planner_state;

  std::cout << "relative state: " << relative_state.transpose() << std::endl;

  const Vector3d planner_position = dynamics_->Puncture(planner_state);

  std::cout << "planner pos: " << planner_position.transpose() << std::endl;

  // Publish planner state on tf.
  geometry_msgs::TransformStamped transform_stamped;
  transform_stamped.header.frame_id = fixed_frame_id_;
  transform_stamped.header.stamp = current_time;

  transform_stamped.child_frame_id = planner_frame_id_;

  transform_stamped.transform.translation.x = planner_position(0);
  transform_stamped.transform.translation.y = planner_position(1);
  transform_stamped.transform.translation.z = planner_position(2);

  transform_stamped.transform.rotation.x = 0;
  transform_stamped.transform.rotation.y = 0;
  transform_stamped.transform.rotation.z = 0;
  transform_stamped.transform.rotation.w = 1;

  br_.sendTransform(transform_stamped);

  // 2) Get corresponding value function.
  const ValueFunction::ConstPtr value = traj_->GetValueFunction(current_time.toSec());

  // Visualize the tracking bound.
  visualization_msgs::Marker tracking_bound_marker;
  tracking_bound_marker.ns = "bound";
  tracking_bound_marker.header.frame_id = tracker_frame_id_;
  tracking_bound_marker.header.stamp = current_time;
  tracking_bound_marker.id = 0;
  tracking_bound_marker.type = visualization_msgs::Marker::CUBE;
  tracking_bound_marker.action = visualization_msgs::Marker::ADD;

  tracking_bound_marker.scale.x = 2.0 * value->TrackingBound(0);
  tracking_bound_marker.scale.y = 2.0 * value->TrackingBound(1);
  tracking_bound_marker.scale.z = 2.0 * value->TrackingBound(2);

  tracking_bound_marker.color.a = 0.3;
  tracking_bound_marker.color.r = 0.9;
  tracking_bound_marker.color.g = 0.2;
  tracking_bound_marker.color.b = 0.9;

  tracking_bound_pub_.publish(tracking_bound_marker);

  // 3) Interpolate gradient to get optimal control.
  const VectorXd optimal_control = value->OptimalControl(relative_state);

  std::cout << "optimal control: " << optimal_control.transpose() << std::endl;

  // 4) Apply optimal control.
  geometry_msgs::Vector3 control_msg;
  control_msg.x = optimal_control(0);
  control_msg.y = optimal_control(1);
  control_msg.z = optimal_control(2);

  control_pub_.publish(control_msg);

  // Publish environment.
  space_->Visualize(environment_pub_, fixed_frame_id_);

  // Visualize trajectory.
  traj_->Visualize(traj_pub_, fixed_frame_id_, dynamics_);
}

// Run meta planner.
void Tracker::RunMetaPlanner() {
  const MetaPlanner meta(space_);

  traj_ = meta.Plan(dynamics_->Puncture(state_),
                    dynamics_->Puncture(goal_), planners_);

  // Visualize the new trajectory.
  traj_->Visualize(traj_pub_, fixed_frame_id_, dynamics_);
}

} //\namespace meta
