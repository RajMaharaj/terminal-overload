// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/input/oculusVR/oculusVRSensorData.h"
#include "platform/input/oculusVR/oculusVRUtil.h"
#include "console/console.h"

OculusVRSensorData::OculusVRSensorData()
{
   reset();
}

void OculusVRSensorData::reset()
{
   mDataSet = false;
}

void OculusVRSensorData::setData(OVR::SensorFusion& data, const F32& maxAxisRadius)
{
   // Sensor rotation
   OVR::Quatf orientation;
   if(data.GetPredictionDelta() > 0)
   {
      orientation = data.GetPredictedOrientation();
   }
   else
   {
      orientation = data.GetOrientation();
   }
   OVR::Matrix4f orientMat(orientation);
   OculusVRUtil::convertRotation(orientMat.M, mRot);
   mRotQuat.set(mRot);

   // Sensor rotation in Euler format
   OculusVRUtil::convertRotation(orientation, mRotEuler);

   // Sensor rotation as axis
   OculusVRUtil::calculateAxisRotation(mRot, maxAxisRadius, mRotAxis);

   // Sensor raw values
   OVR::Vector3f accel = data.GetAcceleration();
   OculusVRUtil::convertAcceleration(accel, mAcceleration);

   OVR::Vector3f angVel = data.GetAngularVelocity();
   OculusVRUtil::convertAngularVelocity(angVel, mAngVelocity);

   OVR::Vector3f mag;
   if(data.HasMagCalibration() && data.IsYawCorrectionEnabled())
   {
      mag = data.GetCalibratedMagnetometer();
   }
   else
   {
      mag = data.GetMagnetometer();
   }
   OculusVRUtil::convertMagnetometer(mag, mMagnetometer);

   mDataSet = true;
}

void OculusVRSensorData::simulateData(const F32& maxAxisRadius)
{
   // Sensor rotation
   mRot.identity();
   mRotQuat.identity();
   mRotEuler.zero();

   // Sensor rotation as axis
   OculusVRUtil::calculateAxisRotation(mRot, maxAxisRadius, mRotAxis);

   // Sensor raw values
   mAcceleration.zero();
   mAngVelocity.zero();
   mMagnetometer.zero();

   mDataSet = true;
}

U32 OculusVRSensorData::compare(OculusVRSensorData* other, bool doRawCompare)
{
   S32 result = DIFF_NONE;

   // Check rotation
   if(mRotEuler.x != other->mRotEuler.x || mRotEuler.y != other->mRotEuler.y || mRotEuler.z != other->mRotEuler.z || !mDataSet)
   {
      result |= DIFF_ROT;
   }

   // Check rotation as axis
   if(mRotAxis.x != other->mRotAxis.x || !mDataSet)
   {
      result |= DIFF_ROTAXISX;
   }
   if(mRotAxis.y != other->mRotAxis.y || !mDataSet)
   {
      result |= DIFF_ROTAXISY;
   }

   // Check raw values
   if(doRawCompare)
   {
      if(mAcceleration.x != other->mAcceleration.x || mAcceleration.y != other->mAcceleration.y || mAcceleration.z != other->mAcceleration.z || !mDataSet)
      {
         result |= DIFF_ACCEL;
      }
      if(mAngVelocity.x != other->mAngVelocity.x || mAngVelocity.y != other->mAngVelocity.y || mAngVelocity.z != other->mAngVelocity.z || !mDataSet)
      {
         result |= DIFF_ANGVEL;
      }
      if(mMagnetometer.x != other->mMagnetometer.x || mMagnetometer.y != other->mMagnetometer.y || mMagnetometer.z != other->mMagnetometer.z || !mDataSet)
      {
         result |= DIFF_MAG;
      }
   }

   return result;
}
