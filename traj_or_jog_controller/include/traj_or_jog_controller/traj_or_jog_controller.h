#ifndef TRAJ_OR_JOG_CONTROLLER_H
#define TRAJ_OR_JOG_CONTROLLER_H

// Pluginlib
#include <pluginlib/class_list_macros.hpp>

// Project
#include <actionlib/server/action_server.h>
#include <joint_trajectory_controller/joint_trajectory_controller.h>
#include <realtime_tools/realtime_buffer.h>
#include <trajectory_interface/quintic_spline_segment.h>
#include <std_msgs/Float64MultiArray.h>

namespace traj_or_jog_controller
{
template <class SegmentImpl, class HardwareInterface>
class TrajOrJogController : 
public joint_trajectory_controller::JointTrajectoryController <SegmentImpl,HardwareInterface>
{
public:
  /** \name Non Real-Time Safe Functions
   *\{*/

  /** \brief Override the init function of the base class. */
  virtual bool init(HardwareInterface* hw, ros::NodeHandle& root_nh, ros::NodeHandle& controller_nh);
  /*\}*/

  /** \name Real-Time Safe Functions
  *\{*/

  /** \brief Start the controller. */
  void starting(const ros::Time& time)
  {
    // Start the base class, JointTrajectoryController
    JointTrajectoryController::starting(time);

    // Start the real-time velocity controller with 0.0 velocities
    commands_buffer_.readFromRT()->assign(n_joints_, 0.0); 
  }

  void stopTrajectoryExecution()
  {
    allow_trajectory_execution_ = false;
    JointTrajectoryController::setHoldPosition(ros::Time::now());
    JointTrajectoryController::stopping(ros::Time::now());
  }

  /** \brief Override updates of the base class. */
  virtual void update(const ros::Time& time, const ros::Duration& period);
  /*\}*/

  realtime_tools::RealtimeBuffer<std::vector<double> > commands_buffer_;
  unsigned int n_joints_;

protected:
  /** \brief Provide an action server for the execution of trajectories. */
  typedef joint_trajectory_controller::JointTrajectoryController<SegmentImpl, HardwareInterface>  JointTrajectoryController;
  typedef actionlib::ActionServer<control_msgs::FollowJointTrajectoryAction>::GoalHandle  GoalHandle;

  ros::Subscriber  velocity_command_sub_;
  bool allow_trajectory_execution_; ///< Current mode.
  bool trajectory_is_active_; ///< Tracks the joint trajectory pointer

  // This stores the last time a jogging command was recieved and will help tell when it is stale
  ros::Time last_jog_cmd_stamp_;

  typename JointTrajectoryController::Segment::State traj_jog_current_state_; ///< Stores the current position and velocity
  typename JointTrajectoryController::Segment::State traj_jog_desired_state_; ///< Stores the desired position and velocity

  /**
   * \brief Callback for real-time JointGroupVelocityController commands.
   * Incoming commands interrupt trajectory execution.
   */
  void velocityCommandCB(const std_msgs::Float64MultiArrayConstPtr& msg)
  {
    // Disable trajectory execution since the new real-time velocity command takes priority
    if (allow_trajectory_execution_)
    {
      stopTrajectoryExecution();
    }

    if(msg->data.size()!=n_joints_)
    { 
      ROS_ERROR_STREAM("Dimension of command (" << msg->data.size() << ") does not match number of joints (" << n_joints_ << ")! Not executing!");
      return; 
    }
    commands_buffer_.writeFromNonRT(msg->data);

    last_jog_cmd_stamp_ = ros::Time::now();
  }

  /**
   * \brief Override the callback for the JointTrajectoryController action server.
   */
  void goalCB(GoalHandle gh);

  /**
   * \brief Update and store the current joint state
   */
  void getCurrentState()
  {
    for(unsigned int i=0; i<n_joints_; ++i)
    {
      traj_jog_current_state_.position[i] = JointTrajectoryController::joints_[i].getPosition();
      traj_jog_current_state_.velocity[i] = JointTrajectoryController::joints_[i].getVelocity();
    }
  }

    /**
   * \brief This looks at the current and desired state and sends a jog command based on them
   */
  void sendJogCommand(const ros::Duration& period);
};
}

#endif  // header guard
