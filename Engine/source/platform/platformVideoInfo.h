// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _PLATFORM_VIDEOINFO_H_
#define _PLATFORM_VIDEOINFO_H_

#include "platform/platform.h"
#include "core/util/str.h"
#include "core/util/tVector.h"

// The purpose of this class is to abstract the gathering of video adapter information.
// This information is not specific to the API being used to do the rendering, or
// the capabilities of that renderer. That information is queried in a different
// class. 

class PlatformVideoInfo
{
   // # of devices
   // description
   // manufacturer
   // chip set
   // driver version
   // VRAM
public:
   enum PVIQueryType
   {
      PVI_QueryStart = 0,     ///< Start of the enum for looping

      // The NumAdapters query is the only non-adapter specific query, the following
      // queries are all specific to an adapter.
      PVI_NumDevices = 0,     ///< Number of sub adapters
      PVI_Description,        ///< String description of the adapter
      PVI_Name,               ///< Card name string
      PVI_ChipSet,            ///< Chipset string
      PVI_DriverVersion,      ///< Driver version string
      PVI_VRAM,               ///< Dedicated video memory in megabytes

      // Please add query types above this value
      PVI_QueryCount,          ///< Counter so that this enum can be looped over
      PVI_NumAdapters,        ///< Number of adapters on the system
   };

   struct PVIAdapter
   {
      U32 numDevices;
      String description;
      String name;
      String chipSet;
      String driverVersion;
      U32 vram;
   };

private:
   Vector<PVIAdapter> mAdapters; ///< Vector of adapters

   /// Signal handling method for GFX signals
   // bool processGFXSignal( GFXDevice::GFXDeviceEventType gfxEvent );

protected:
   /// This method will be called before any queries are made. All initialization,
   /// for example Win32 COM startup, should be done in this method. If the return
   /// value is false, no querys will be made.
   virtual bool _initialize() = 0; 

   /// This is the query method which subclasses must implement. The querys made
   /// are all found in the PVIQueryType enum. If NULL is specified for outValue,
   /// the sub class should simply return true if the query type is supported, and
   /// false otherwise. 
   virtual bool _queryProperty( const PVIQueryType queryType, const U32 adapterId, String *outValue ) = 0;

public:
   PlatformVideoInfo();
   virtual ~PlatformVideoInfo();

   bool profileAdapters();

   const PVIAdapter &getAdapterInformation( const U32 adapterIndex ) const;
};

#endif