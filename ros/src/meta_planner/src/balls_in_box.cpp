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
// Defines a Box environment with spherical obstacles. For simplicity, this
// does not bother with a kdtree index to speed up collision queries, since
// it is only for a simulated demo.
//
///////////////////////////////////////////////////////////////////////////////

#include <demo/balls_in_box.h>

namespace meta {

// Factory method. Use this instead of the constructor.
BallsInBox::Ptr BallsInBox::Create() {
  BallsInBox::Ptr ptr(new BallsInBox());
  return ptr;
}

// Constructor. Don't use this. Use the factory method instead.
BallsInBox::BallsInBox()
  : Box() {}

// Inherited collision checker from Box needs to be overwritten.
// Takes in incoming and outgoing value functions. See planner.h for details.
bool BallsInBox::IsValid(const Vector3d& position,
                         const ValueFunction::ConstPtr& incoming_value,
                         const ValueFunction::ConstPtr& outgoing_value) const {
#ifdef ENABLE_DEBUG_MESSAGES
  if (!incoming_value.get() || !outgoing_value.get()) {
    ROS_ERROR("Value function pointer was null.");
    return false;
  }
#endif

  // Check bounds.
  for (size_t ii = 0; ii < 3; ii++) {
    const double bound = outgoing_value->
      SwitchingTrackingBound(ii, incoming_value);

    if (position(ii) < lower_(ii) + bound ||
        position(ii) > upper_(ii) - bound)
      return false;
  }

  // Check against each obstacle.
  // NOTE: assuming rectangular tracking bound.
  for (size_t ii = 0; ii < points_.size(); ii++) {
    const Vector3d& p = points_[ii];

    // Start by checking this position directly against the obstacle center.
    if ((position - p).norm() <= radii_[ii])
      return false;

    // Find the corner of the tracking bound which is closest to this obstacle.
    // NOTE: this check assumes that the tracking bubble is not greater than
    // twice the obstacle diameter.
    Vector3d corner;
    for (size_t jj = 0; jj < 3; jj++) {
      const double bound = outgoing_value->
        SwitchingTrackingBound(jj, incoming_value);

      corner(jj) = (position(jj) - p(jj) > 0.0) ?
        position(jj) - bound : position(jj) + bound;
    }

    if ((corner - p).norm() <= radii_[ii])
      return false;
  }

  return true;
}


// Checks for obstacles within a sensing radius. Returns true if at least
// one obstacle was found.
bool BallsInBox::SenseObstacles(const Vector3d& position, double sensor_radius,
                                std::vector<Vector3d>& obstacle_positions,
                                std::vector<double>& obstacle_radii) const {
  obstacle_positions.clear();
  obstacle_radii.clear();

  for (size_t ii = 0; ii < points_.size(); ii++){
    if ((position - points_[ii]).norm() <= radii_[ii] + sensor_radius) {
      obstacle_positions.push_back(points_[ii]);
      obstacle_radii.push_back(radii_[ii]);
    }
  }

  return obstacle_positions.size() > 0;
}

// Checks if a given obstacle is in the environment.
bool BallsInBox::IsObstacle(const Vector3d& obstacle_position,
                            double obstacle_radius) const {
  for (size_t ii = 0; ii < points_.size(); ii++)
    if ((obstacle_position - points_[ii]).norm() < 1e-8 &&
        std::abs(obstacle_radius - radii_[ii]) < 1e-8)
      return true;

  return false;
}


// Inherited visualizer from Box needs to be overwritten.
void BallsInBox::Visualize(const ros::Publisher& pub,
                           const std::string& frame_id) const {
  if (pub.getNumSubscribers() <= 0)
    return;

  // Set up box marker.
  visualization_msgs::Marker cube;
  cube.ns = "cube";
  cube.header.frame_id = frame_id;
  cube.header.stamp = ros::Time::now();
  cube.id = 0;
  cube.type = visualization_msgs::Marker::CUBE;
  cube.action = visualization_msgs::Marker::ADD;
  cube.color.a = 0.5;
  cube.color.r = 0.3;
  cube.color.g = 0.7;
  cube.color.b = 0.7;

  geometry_msgs::Point center;

  // Fill in center and scale.
  cube.scale.x = upper_(0) - lower_(0);
  center.x = lower_(0) + 0.5 * cube.scale.x;

  cube.scale.y = upper_(1) - lower_(1);
  center.y = lower_(1) + 0.5 * cube.scale.y;

  cube.scale.z = upper_(2) - lower_(2);
  center.z = lower_(2) + 0.5 * cube.scale.z;

  cube.pose.position = center;
  cube.pose.orientation.x = 0.0;
  cube.pose.orientation.y = 0.0;
  cube.pose.orientation.z = 0.0;
  cube.pose.orientation.w = 1.0;

  // Publish cube marker.
  pub.publish(cube);

  // Visualize obstacles as spheres.
  for (size_t ii = 0; ii < points_.size(); ii++){
    visualization_msgs::Marker sphere;
    sphere.ns = "sphere";
    sphere.header.frame_id = frame_id;
    sphere.header.stamp = ros::Time::now();
    sphere.id = static_cast<int>(ii);
    sphere.type = visualization_msgs::Marker::SPHERE;
    sphere.action = visualization_msgs::Marker::ADD;

    sphere.scale.x = 2.0 * radii_[ii];
    sphere.scale.y = 2.0 * radii_[ii];
    sphere.scale.z = 2.0 * radii_[ii];

    sphere.color.a = 0.9;
    sphere.color.r = 0.7;
    sphere.color.g = 0.5;
    sphere.color.b = 0.5;

    geometry_msgs::Point p;
    const Vector3d point = points_[ii];
    p.x = point(0);
    p.y = point(1);
    p.z = point(2);

    sphere.pose.position = p;

    // Publish sphere marker.
    pub.publish(sphere);
  }

}

// Add a spherical obstacle of the given radius to the environment.
void BallsInBox::AddObstacle(const Vector3d& point, double r) {
  const double kSmallNumber = 1e-8;

#ifdef ENABLE_DEBUG_MESSAGES
  if (r < kSmallNumber)
    ROS_ERROR("Radius was too small: %f.", r);
#endif

  points_.push_back(point);
  radii_.push_back(std::max(r, kSmallNumber));
}

} //\namespace meta
