// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _OCULUSVRDEVICE_H_
#define _OCULUSVRDEVICE_H_

#include "platform/input/oculusVR/oculusVRConstants.h"
#include "platform/input/oculusVR/oculusVRHMDDevice.h"
#include "platform/input/oculusVR/oculusVRSensorDevice.h"
#include "platform/input/IInputDevice.h"
#include "platform/input/event.h"
#include "platform/output/IDisplayDevice.h"
#include "core/util/tSingleton.h"
#include "math/mQuat.h"
#include "math/mPoint4.h"
#include "OVR.h"

#define DEFAULT_RIFT_UNIT 0

class OculusVRDevice : public IInputDevice, public IDisplayDevice
{
public:
   static bool smEnableDevice;

   // If no HMD is present simulate it being available
   static bool smSimulateHMD;

   // Use the chromatic aberration correction version of the barrel
   // distortion shader.
   static bool smUseChromaticAberrationCorrection;

   // Type of rotation events to broadcast
   static bool smGenerateAngleAxisRotationEvents;
   static bool smGenerateEulerRotationEvents;

   // Broadcast sensor rotation as axis
   static bool smGenerateRotationAsAxisEvents;

   // The maximum sensor angle when used as an axis event
   // as measured from a vector pointing straight up (in degrees)
   static F32 smMaximumAxisAngle;

   // Broadcast sensor raw data: acceleration, angular velocity, magnetometer reading
   static bool smGenerateSensorRawEvents;

   // Indicates that a whole frame event should be generated and frames
   // should be buffered.
   static bool smGenerateWholeFrameEvents;

protected:
   class DeviceListener : public OVR::MessageHandler
   {
   protected:
      OculusVRDevice* mOwner;

   public:
      DeviceListener(OculusVRDevice* owner) { mOwner = owner; }
      virtual ~DeviceListener() { mOwner = NULL; }

      virtual void OnMessage(const OVR::Message&);
   };

   // Our OVR SDK device listener class
   DeviceListener* mListener;

   // The OVR SDK device manager
   OVR::DeviceManager* mDeviceManager;

   // Discovered HMD devices
   Vector<OculusVRHMDDevice*> mHMDDevices;

   // Discovered sensor devices
   Vector<OculusVRSensorDevice*> mSensorDevices;

   /// Is the device active
   bool mActive;

   // Should the input texture into the HMD (the render target that the scene has been
   // rendered to) be scaled according to the HMD's distortion calculation?
   bool mScaleInputTexture;

protected:
   void cleanUp();

   /// Build out the codes used for controller actions with the
   /// Input Event Manager
   void buildCodeTable();

   void addHMDDevice(OVR::HMDDevice* hmd);

   void createSimulatedHMD();

   void addSensorDevice(OVR::SensorDevice* sensor);

   void createSimulatedSensor();

public:
   OculusVRDevice();
   ~OculusVRDevice();

   static void staticInit();

   bool enable();
   void disable();

   bool getActive() { return mActive; }
   void setActive(bool state) { mActive = state; }

   bool process();

   // IDisplayDevice
   virtual bool providesYFOV() const;
   virtual F32 getYFOV() const;
   virtual bool providesEyeOffset() const;
   virtual const Point3F& getEyeOffset() const;
   virtual bool providesProjectionOffset() const;
   virtual const Point2F& getProjectionOffset() const;

   // HMDs
   U32 getHMDCount() const { return mHMDDevices.size(); }
   const OculusVRHMDDevice* getHMDDevice(U32 index) const;
   F32 getHMDCurrentIPD(U32 index);
   void setHMDCurrentIPD(U32 index, F32 ipd);

   // Sensors
   U32 getSensorCount() const { return mSensorDevices.size(); }
   const OculusVRSensorDevice* getSensorDevice(U32 index) const;
   EulerF getSensorEulerRotation(U32 index);
   VectorF getSensorAcceleration(U32 index);
   EulerF getSensorAngularVelocity(U32 index);
   VectorF getSensorMagnetometer(U32 index);
   F32 getSensorPredictionTime(U32 index);
   void setSensorPredictionTime(U32 index, F32 dt);
   void setAllSensorPredictionTime(F32 dt);
   bool getSensorGravityCorrection(U32 index);
   void setSensorGravityCorrection(U32 index, bool state);
   bool getSensorYawCorrection(U32 index);
   void setSensorYawCorrection(U32 index, bool state);
   bool getSensorMagnetometerCalibrated(U32 index);
   void resetAllSensors();

public:
   // For ManagedSingleton.
   static const char* getSingletonName() { return "OculusVRDevice"; }   
};

/// Returns the OculusVRDevice singleton.
#define OCULUSVRDEV ManagedSingleton<OculusVRDevice>::instance()

#endif   // _OCULUSVRDEVICE_H_
