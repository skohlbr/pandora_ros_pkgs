/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, P.A.N.D.O.R.A. Team.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the P.A.N.D.O.R.A. Team nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Geromichalos Dimitrios Panagiotis <geromidg@gmail.com>
 *********************************************************************/

#include "pandora_gazebo_plugins/pandora_differential_plugin.h"

namespace gazebo
{

  // Register this plugin with the simulator
  GZ_REGISTER_MODEL_PLUGIN(GazeboRosDifferential)

  // Constructor
  GazeboRosDifferential::GazeboRosDifferential()
  {
    this->seed = 0;
  }

  // Destructor
  GazeboRosDifferential::~GazeboRosDifferential()
  {
    // Differential Dynamic Reconfigure
    if (this->reconfigure_thread_)
    {
      this->reconfigure_thread_->join();
      ROS_DEBUG_STREAM_NAMED("pandora_differential_plugin", "differential reconfigure joined");
    }

    event::Events::DisconnectWorldUpdateBegin(this->update_connection_);

    // Finalize the controller
    this->rosnode_->shutdown();
    delete this->rosnode_;
  }

  void GazeboRosDifferential::Load(
      physics::ModelPtr _parent,
      sdf::ElementPtr _sdf)
  {
    // Save pointers
    this->world_ = _parent->GetWorld();
    this->model_ = _parent;
    this->sdf = _sdf;

    // ROS callback queue for processing subscription
    this->deferred_load_thread_ = boost::thread(boost::bind(&GazeboRosDifferential::LoadThread, this));
  }

  void GazeboRosDifferential::LoadThread()
  {
    if (!GazeboRosDifferential::LoadParameters())
    {
      ROS_FATAL("Unable to load important parameters, exiting...");
      return;
    }

    // Initialize the variables used in the PID algorithm
    this->previous_error_ = 0.0;
    this->integral_ = 0.0;

    // Make sure the ROS node for Gazebo has already been initialized
    if (!ros::isInitialized())
    {
      ROS_FATAL_STREAM(
            "A ROS node for Gazebo has not been initialized, "
         << "unable to load plugin. "
         << "Load the Gazebo system plugin "
         << "libgazebo_ros_api_plugin.so' in "
         << "the gazebo_ros package.");
      return;
    }

    this->rosnode_ = new ros::NodeHandle(this->robot_namespace_);

    // Publish multi queue
    this->pmq.startServiceThread();

    // /\brief Start a thread for the differential dynamic reconfigure node
    // FIXME: Wait for the rest of the plugin to load

    /* Dynamic reconfigure DISABLED!
    this->reconfigure_thread_.reset(
        new boost::thread(boost::bind(&GazeboRosDifferential::LoadReconfigureThread,this)));
    */

    this->joint_state_pub_Queue = this->pmq.addPub<sensor_msgs::JointState>();

    this->joint_state_pub_ = this->rosnode_->advertise<sensor_msgs::JointState>("differential_side_joint_states", 1);

    // Advertise services on the custom queue
    ros::AdvertiseServiceOptions aso = ros::AdvertiseServiceOptions::create<std_srvs::Empty>(
        "/differantial_side",
        boost::bind(&GazeboRosDifferential::ServiceCallback, this, _1, _2),
        ros::VoidPtr(),
        &this->callback_queue_);

    this->srv_ = this->rosnode_->advertiseService(aso);

    // New Mechanism for Updating every World Cycle
    // Listen to the update event. This event is broadcast every
    // simulation iteration.
    this->update_connection_ = event::Events::ConnectWorldUpdateBegin(
        boost::bind(&GazeboRosDifferential::UpdateChild, this));

    ROS_INFO("Starting PandoraDifferential Plugin (ns = %s)!", this->robot_namespace_.c_str());
  }

  bool GazeboRosDifferential::LoadParameters()
  {
    this->robot_namespace_ = "";

    if (this->sdf->HasElement("robotNamespace"))
    {
      this->robot_namespace_ = this->sdf->Get<std::string>("robotNamespace") + "/";
    }

    if (!this->sdf->HasElement("baseLink"))
    {
      ROS_FATAL("Differential plugin missing <baseLink>.");
      return false;
    }
    else
    {
      this->base_link_ = this->model_->GetLink(this->sdf->Get<std::string>("baseLink"));
    }

    if (!this->sdf->HasElement("leftFrontWheelLink"))
    {
      ROS_FATAL("Differential plugin missing <leftFrontWheelLink>.");
      return false;
    }
    else
    {
      this->left_front_wheel_link_ = this->model_->GetLink(this->sdf->Get<std::string>("leftFrontWheelLink"));
    }

    if (!this->sdf->HasElement("leftRearWheelLink"))
    {
      ROS_FATAL("Differential plugin missing <leftRearWheelLink>.");
      return false;
    }
    else
    {
      this->left_rear_wheel_link_ = this->model_->GetLink(this->sdf->Get<std::string>("leftRearWheelLink"));
    }

    if (!this->sdf->HasElement("rightFrontWheelLink"))
    {
      ROS_FATAL("Differential plugin missing <rightFrontWheelLink>.");
      return false;
    }
    else
    {
      this->right_front_wheel_link_ = this->model_->GetLink(this->sdf->Get<std::string>("rightFrontWheelLink"));
    }

    if (!this->sdf->HasElement("rightRearWheelLink"))
    {
      ROS_FATAL("Differential plugin missing <rightRearWheelLink>.");
      return false;
    }
    else
    {
      this->right_rear_wheel_link_ = this->model_->GetLink(this->sdf->Get<std::string>("rightRearWheelLink"));
    }

    if (!this->sdf->HasElement("leftSideJoint"))
    {
      ROS_FATAL("Differential plugin missing <leftSideJoint>.");
      return false;
    }
    else
    {
      this->left_side_joint_ = this->model_->GetJoint(this->sdf->Get<std::string>("leftSideJoint"));
      joint_state_msg_.name.push_back(this->sdf->Get<std::string>("leftSideJoint"));
    }

    if (!this->sdf->HasElement("rightSideJoint"))
    {
      ROS_FATAL("Differential plugin missing <rightSideJoint>.");
      return false;
    }
    else
    {
      this->right_side_joint_ = this->model_->GetJoint(this->sdf->Get<std::string>("rightSideJoint"));
      joint_state_msg_.name.push_back(this->sdf->Get<std::string>("rightSideJoint"));
    }

    if (!this->sdf->HasElement("maxAngle"))
    {
      ROS_INFO("Differential plugin missing <maxAngle>, defaults to 0.");
      this->max_angle_ = 0;
    }
    else
    {
      this->max_angle_ = this->sdf->Get<double>("maxAngle");
    }

    if (!this->sdf->HasElement("maxDownforce"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing <maxDownforce>, "
        << "defaults to 0.");
      this->max_downforce_ = 0;
    }
    else
    {
      this->max_downforce_ = this->sdf->Get<double>("maxDownforce");
    }

    if (!this->sdf->HasElement("maxDifferentialForceZ"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing "
        << "<maxDifferentialForceZ>, defaults to 0.");
      this->max_differential_force_z_ = 0;
    }
    else
    {
      this->max_differential_force_z_ = this->sdf->Get<double>("maxDifferentialForceZ");
    }

    if (!this->sdf->HasElement("maxDifferentialForceY"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing "
        << "<maxDifferentialForceY>, defaults to 0.");
      this->max_differential_force_y_ = 0;
    }
    else
    {
      this->max_differential_force_y_ = this->sdf->Get<double>("maxDifferentialForceY");
    }

    // TODO(gerom): Get the value directly from the joints.
    if (!this->sdf->HasElement("sideJointDamping"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing <sideJointDamping>, "
        << "defaults to 0.");
      this->side_joint_damping_ = 0;
    }
    else
    {
      this->side_joint_damping_ = this->sdf->Get<double>("sideJointDamping");
    }

    if (!this->sdf->HasElement("correctionForceModifier"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing "
        << "<correctionForceModifier>, defaults to 0.");
      this->correction_force_modifier_ = 0;
    }
    else
    {
      this->correction_force_modifier_ = this->sdf->Get<double>("correctionForceModifier");
    }

    if (!this->sdf->HasElement("P"))
    {
      ROS_INFO("Differential plugin missing <P>, defaults to 0.");
      this->k_p_ = 0;
    }
    else
    {
      this->k_p_ = this->sdf->Get<double>("P");
    }

    if (!this->sdf->HasElement("I"))
    {
      ROS_INFO("Differential plugin missing <I>, defaults to 0.");
      this->k_i_ = 0;
    }
    else
    {
      this->k_i_ = this->sdf->Get<double>("I");
    }

    if (!this->sdf->HasElement("D"))
    {
      ROS_INFO("Differential plugin missing <D>, defaults to 0.");
      this->k_d_ = 0;
    }
    else
    {
      this->k_d_ = this->sdf->Get<double>("D");
    }

    if (!this->sdf->HasElement("publishJointStates"))
    {
      ROS_INFO_STREAM(
           "Differential plugin missing <publishJointStates>, "
        << "defaults to false.");
      this->publish_joint_states_ = false;
    }
    else
    {
      this->publish_joint_states_ = (this->sdf->Get<std::string>("publishJointStates") == "true");
    }

    return true;
  }

  void GazeboRosDifferential::LoadReconfigureThread()
  {
    this->reconfigure_srv_.reset(new dynamic_reconfigure::Server<pandora_gazebo_plugins::DifferentialConfig>());
    this->reconfigure_callback_ = boost::bind(&GazeboRosDifferential::ConfigCallback, this, _1, _2);
    this->reconfigure_srv_->setCallback(this->reconfigure_callback_);

    // TODO(gerom): Test when callback is executed.
    ROS_INFO("Differential reconfigure ready.");
  }

  // Returns true always
  bool GazeboRosDifferential::ServiceCallback(
      std_srvs::Empty::Request& req,
      std_srvs::Empty::Response& res)
  {
    return true;
  }

  void GazeboRosDifferential::ConfigCallback(
      pandora_gazebo_plugins::DifferentialConfig& config,
      uint32_t level)
  {
    this->max_angle_                 = config.maxAngle;
    this->side_joint_damping_        = config.sideJointDamping;
    this->max_downforce_             = config.maxDownforce;
    this->max_differential_force_z_  = config.maxDifferentialForceZ;
    this->max_differential_force_y_  = config.maxDifferentialForceY;
    this->k_p_                       = config.P;
    this->k_i_                       = config.I;
    this->k_d_                       = config.D;
    this->correction_force_modifier_ = config.correctionForceModifier;

    ROS_INFO("Differential dynamic reconfigure complete.");
  }

  double GazeboRosDifferential::GetUpdateRate()
  {
    return this->world_->GetPhysicsEngine()->GetRealTimeUpdateRate();
  }

  void GazeboRosDifferential::UpdateAngles()
  {
    this->left_angle_ = this->left_side_joint_->GetAngle(0).Radian();
    this->right_angle_ = this->right_side_joint_->GetAngle(0).Radian();
  }

  void GazeboRosDifferential::PublishJointStates()
  {
    this->joint_state_msg_.header.stamp = ros::Time::now();
    this->joint_state_msg_.position.clear();
    this->joint_state_msg_.position.push_back(this->left_angle_);
    this->joint_state_msg_.position.push_back(this->right_angle_);

    {
      boost::mutex::scoped_lock lock(this->lock_);
      this->joint_state_pub_Queue->push(this->joint_state_msg_, this->joint_state_pub_);
    }
  }

  void GazeboRosDifferential::AddDifferentialForces()
  {
    // Initialize the forces to be set
    math::Vector3 left_rear_force(0, 0, 0);
    math::Vector3 right_rear_force(0, 0, 0);

    // Calculate and normalize the positive angle difference
    double angle_difference = fabs(fabs(this->left_angle_) - fabs(this->right_angle_));
    angle_difference /= this->max_angle_;

    // Calculate the forces for each link
    // TODO(gerom): Test if the force are applied with the correct sign.
    if (this->left_angle_ > this->right_angle_)
    {
      left_rear_force.z = this->max_differential_force_z_ * angle_difference * (- 1);
      left_rear_force.y = this->max_differential_force_y_ * angle_difference;
    }

    if (this->left_angle_ < this->right_angle_)
    {
      right_rear_force.z = this->max_differential_force_z_ * angle_difference * (- 1);
      right_rear_force.y = this->max_differential_force_y_ * angle_difference * (- 1);
    }

    // Set the forces to the wheel links
    this->left_rear_wheel_link_->AddRelativeForce(left_rear_force);
    this->right_rear_wheel_link_->AddRelativeForce(right_rear_force);
  }

  // TODO(gerom): Implement maximum side joint damping
  //       and normalize side joint damping.
  void GazeboRosDifferential::AddDownforces()
  {
    // Get the linear velocity of each link in the z axis in ( mm / sec )
    double left_front_z_vel = 1000.0 * left_front_wheel_link_->GetRelativeLinearVel().z;
    double left_rear_z_vel = 1000.0 * left_rear_wheel_link_->GetRelativeLinearVel().z;
    double right_front_z_vel = 1000.0 * right_front_wheel_link_->GetRelativeLinearVel().z;
    double right_rear_z_vel = 1000.0 * right_rear_wheel_link_->GetRelativeLinearVel().z;

    // Initialize the downforces to be set
    math::Vector3 left_front_downforce(0, 0, 0);
    math::Vector3 left_rear_downforce(0, 0, 0);
    math::Vector3 right_front_downforce(0, 0, 0);
    math::Vector3 right_rear_downforce(0, 0, 0);

    // Calculate the downforce for each link
    if (left_front_z_vel < 0)
    {
      left_front_downforce.z = this->max_downforce_ * this->side_joint_damping_ * left_front_z_vel;
    }
    if (left_rear_z_vel < 0)
    {
      left_rear_downforce.z = this->max_downforce_ * this->side_joint_damping_ * left_rear_z_vel;
    }
    if (right_front_z_vel < 0)
    {
      right_front_downforce.z = this->max_downforce_ * this->side_joint_damping_ * right_front_z_vel;
    }
    if (right_rear_z_vel < 0)
    {
      right_rear_downforce.z = this->max_downforce_ * this->side_joint_damping_ * right_rear_z_vel;
    }

    // Set the downforces to the wheel links
    this->left_front_wheel_link_->AddRelativeForce(left_front_downforce);
    this->left_rear_wheel_link_->AddRelativeForce(left_rear_downforce);
    this->right_front_wheel_link_->AddRelativeForce(right_front_downforce);
    this->right_rear_wheel_link_->AddRelativeForce(right_rear_downforce);
  }

  double GazeboRosDifferential::PIDAlgorithm(
      double error,
      double& previous_error,
      double& integral,
      double k_p,
      double k_i = 0,
      double k_d = 0,
      double i_clamp_min = 0,
      double i_clamp_max = 0)
  {
    // TODO(gerom): Implement integral clamping.

    // Calculate the time between two engine iterations
    double dt = (1.0 / GazeboRosDifferential::GetUpdateRate());

    // Calculate the proportional contribution to output
    double p_term = (k_p * error);

    // Calculate the integral contribution to output
    integral += (error * dt);
    double i_term = (k_i * integral);

    // Calculate the derivative error & update the previous error
    double derivative = ((error - previous_error) / dt);
    previous_error = error;

    // Calculate the derivative contribution to output
    double d_term = (k_d * derivative);

    // Calculate the output
    double output = (p_term + i_term + d_term);

    return output;
  }

  double GazeboRosDifferential::PIDAlgorithm()
  {
    // Calculate the error of the loop ( target - state )
    double error = ((this->left_angle_ + this->right_angle_) / 2);
    // double error = ( left_angle_abs - right_angle_abs );

    // Normalize the error
    error /= this->max_angle_;

    return GazeboRosDifferential::PIDAlgorithm(
        error,
        this->previous_error_,
        this->integral_,
        this->k_p_,
        this->k_i_,
        this->k_d_);
  }

  void GazeboRosDifferential::AddBaseCorrectionForce()
  {
    // Initialize the force to be applied
    math::Vector3 correction_force(0, 0, 0);

    // Calculate the force
    correction_force.x = (this->correction_force_modifier_ / 100.0) * GazeboRosDifferential::PIDAlgorithm();

    // Apply the correction force at the base link
    this->base_link_->AddRelativeForce(correction_force);
  }

  void GazeboRosDifferential::AddSideCorrectionForce()
  {
    // Separate the sign and the value of the angles.
    double left_angle_abs = fabs(this->left_angle_);
    double right_angle_abs = fabs(this->right_angle_);
    int left_angle_sign = copysign(1, this->left_angle_);
    int right_angle_sign = copysign(1, this->right_angle_);

    // Calculate the error
    double angle_diff = left_angle_abs - right_angle_abs;

    // Maximum hardcoded force to be applied
    double max_force = 15.0;

    // Apply the correction forces at the side joints accordingly
    if ((this->left_angle_ * this->right_angle_) > 0)
    {
      this->left_side_joint_->SetForce(0, (-1) * max_force * left_angle_sign);
      this->right_side_joint_->SetForce(0, (-1) * max_force * right_angle_sign);
    }

    if ((this->left_angle_ * this->right_angle_) < 0)
    {
      if (angle_diff > 0)
      {
        this->left_side_joint_->SetForce(0, (-1) * (1.0 / 4.0) * max_force * right_angle_sign);
        this->right_side_joint_->SetForce(0, (-1) * (1.0 / 4.0) * max_force * right_angle_sign);
      }
      else
      {
        this->left_side_joint_->SetForce(0, (-1) * (1.0 / 4.0) * max_force * left_angle_sign);
        this->right_side_joint_->SetForce(0, (-1) * (1.0 / 4.0) * max_force * left_angle_sign);
      }
    }
  }

  void GazeboRosDifferential::AddPhysicsForces()
  {
    /*
    double m = left_front_wheel_link_->GetInertial()->GetMass();
    math::Vector3 g = left_front_wheel_link_->GetWorld()->GetPhysicsEngine()->GetGravity();
    math::Vector3 z = left_front_wheel_link_->GetWorldCoGPose().pos;

    double m1 = right_front_wheel_link_->GetInertial()->GetMass();
    math::Vector3 g1 = right_front_wheel_link_->GetWorld()->GetPhysicsEngine()->GetGravity();
    math::Vector3 z1 = right_front_wheel_link_->GetWorldCoGPose().pos;
    */

    /*
    double left_front_z = left_front_wheel_link_->GetWorldCoGPose ( ).pos.z;
    double left_rear_z = left_rear_wheel_link_->GetWorldCoGPose ( ).pos.z;
    double right_front_z = right_front_wheel_link_->GetWorldCoGPose ( ).pos.z;
    double right_rear_z = right_rear_wheel_link_->GetWorldCoGPose ( ).pos.z;

    double min_z = 1000.0;
    double force_multiplier = 100.0;

    if (left_front_z < min_z)
    {
      min_z = left_front_z;
    }
    if (left_rear_z < min_z)
    {
      min_z = left_rear_z;
    }
    if (right_front_z < min_z)
    {
      min_z = right_front_z;
    }
    if (right_rear_z < min_z)
    {
      min_z = right_rear_z;
    }
    */

    // left_front_wheel_link_->AddRelativeForce(math::Vector3(0, 0, force_multiplier * (left_front_z - min_z)));
    // left_rear_wheel_link_->AddRelativeForce(math::Vector3(0, 0, force_multiplier * (left_rear_z - min_z)));
    // right_front_wheel_link_->AddRelativeForce(math::Vector3(0, 0, force_multiplier * (right_front_z - min_z)));
    // right_rear_wheel_link_->AddRelativeForce(math::Vector3(0, 0, force_multiplier * (right_rear_z - min_z)));
    left_front_wheel_link_->AddRelativeForce(math::Vector3(0, 0, 20));
    left_rear_wheel_link_->AddRelativeForce(math::Vector3(0, 0, 20));
    right_front_wheel_link_->AddRelativeForce(math::Vector3(0, 0, 20));
    right_rear_wheel_link_->AddRelativeForce(math::Vector3(0, 0, 20));

    // ROS_INFO("LEFT:%f", left_front_wheel_link_->GetRelativeForce().z);
    // ROS_ERROR("RIGHT:%f", right_front_wheel_link_->GetRelativeForce().z);
  }

  void GazeboRosDifferential::UpdateChild()
  {
    // Get the angles in the current iteration of the engine
    GazeboRosDifferential::UpdateAngles();

    // Add PID controlled force at base link (marginally stable)
    // GazeboRosDifferential::AddBaseCorrectionForce ( );

    // Add hardcoded forces (semi-control, working - error not well defined)
    GazeboRosDifferential::AddSideCorrectionForce();

    // Add forces at z axis to overcome high side joint damping (virtual forces)
    // GazeboRosDifferential::AddDownforces ( );

    // Add forces at y / z axes (real forces, applied due to the differential)
    // GazeboRosDifferential::AddDifferentialForces ( );

    // Add forces to improve robot's behaviour (due to poorly simulated physics)
    // GazeboRosDifferential::AddPhysicsForces ( );

    // Publish joint states to be used in RViz
    if (this->publish_joint_states_)
    {
      GazeboRosDifferential::PublishJointStates();
    }
  }

}  // namespace gazebo
