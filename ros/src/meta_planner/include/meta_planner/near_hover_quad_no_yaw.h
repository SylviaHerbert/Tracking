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
// Defines the NearHoverQuadNoYaw class. Assumes that state 'x' entried are:
// * x(0) -- x
// * x(1) -- x_dot
// * x(2) -- y
// * x(3) -- y_dot
// * x(4) -- z
// * x(5) -- z_dot
//
// Also assumes that entried in control 'u' are:
// * u(0) -- pitch
// * u(1) -- roll
// * u(2) -- thrust
//
///////////////////////////////////////////////////////////////////////////////

#ifndef META_PLANNER_NEAR_HOVER_QUAD_NO_YAW_H
#define META_PLANNER_NEAR_HOVER_QUAD_NO_YAW_H

#include <meta_planner/dynamics.h>

#include <math.h>

namespace meta {

class NearHoverQuadNoYaw : public Dynamics {
public:
  typedef std::shared_ptr<const NearHoverQuadNoYaw> ConstPtr;

  // Destructor.
  ~NearHoverQuadNoYaw() {}

  // Factory method. Use this instead of the constructor.
  static ConstPtr Create(const VectorXd& lower_u,
                         const VectorXd& upper_u);

  // Derived classes must be able to give the time derivative of state
  // as a function of current state and control.
  inline VectorXd Evaluate(const VectorXd& x, const VectorXd& u) const {
    VectorXd x_dot(X_DIM);
    x_dot(0) = x(1);
    x_dot(1) = constants::G * std::tan(u(0));
    x_dot(2) = x(3);
    x_dot(3) = constants::G * std::tan(u(1));
    x_dot(4) = x(5);
    x_dot(5) = u(2) - constants::G;

    return x_dot;
  }

  // Derived classes must be able to compute an optimal control given
  // the gradient of the value function at the specified state.
  // In this case (linear dynamics), the state is irrelevant given the
  // gradient of the value function at that state.
  VectorXd OptimalControl(const VectorXd& x,
                          const VectorXd& value_gradient) const;

  // Puncture a full state vector and return a position.
  Vector3d Puncture(const VectorXd& x) const;

  // Get the corresponding full state dimension to the given spatial dimension.
  size_t SpatialDimension(size_t dimension) const;

  // Derived classes must be able to translate a geometric trajectory
  // (i.e. through Euclidean space) into a full state space trajectory.
  std::vector<VectorXd> LiftGeometricTrajectory(
    const std::vector<Vector3d>& positions,
    const std::vector<double>& times) const;

private:
  // Private constructor. Use the factory method instead.
  explicit NearHoverQuadNoYaw(const VectorXd& lower_u, const VectorXd& upper_u);

  // Static constants for dimensions.
  static const size_t X_DIM;
  static const size_t U_DIM;
};

} //\namespace meta

#endif
