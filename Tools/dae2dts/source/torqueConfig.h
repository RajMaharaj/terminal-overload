// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _TORQUECONFIG_H_
#define _TORQUECONFIG_H_

//-----------------------------------------------------------------------------
//Hi, and welcome to the Torque Config file.
//
//This file is a central reference for the various configuration flags that
//you'll be using when controlling what sort of a Torque build you have. In
//general, the information here is global for your entire codebase, applying
//not only to your game proper, but also to all of your tools.

/// Since we can build different engine "products" out of the same
/// base engine source we need a way to differentiate which product
/// this particular game is using.
///
/// TGE       0001
/// TGEA      0002
/// TGB       0003
/// TGEA 360  0004
/// TGE WII   0005
/// Torque 3D 0006
///
#define TORQUE_ENGINE_PRODUCT      0006

/// What's the name of your application? Used in a variety of places.
#define TORQUE_APP_NAME            "dae2dts"

/// What version of the application specific source code is this?
///
/// Version number is major * 1000 + minor * 100 + revision * 10.
#define TORQUE_APP_VERSION         1000

/// Human readable application version string.
#define TORQUE_APP_VERSION_STRING  "1.0.0"

/// Define me if you want to enable multithreading support.
#ifndef TORQUE_MULTITHREAD
#define TORQUE_MULTITHREAD
#endif

/// Define me if you want to disable Torque memory manager.
#ifndef TORQUE_DISABLE_MEMORY_MANAGER
#define TORQUE_DISABLE_MEMORY_MANAGER
#endif

/// Define me if you don't want Torque to compile dso's
#define TORQUE_NO_DSO_GENERATION

// Define me if this build is a tools build

#ifndef TORQUE_PLAYER
#  define TORQUE_TOOLS
#else
#  undef TORQUE_TOOLS
#endif

/// Define me if you want to enable the profiler.
///    See also the TORQUE_SHIPPING block below
//#define TORQUE_ENABLE_PROFILER

/// Define me to enable debug mode; enables a great number of additional
/// sanity checks, as well as making AssertFatal and AssertWarn do something.
/// This is usually defined by the build target.
//#define TORQUE_DEBUG

/// Define me if this is a shipping build; if defined I will instruct Torque
/// to batten down some hatches and generally be more "final game" oriented.
/// Notably this disables a liberal resource manager file searching, and
/// console help strings.
//#define TORQUE_SHIPPING

/// Define me to enable a variety of network debugging aids.
///
///  - NetConnection packet logging.
///  - DebugChecksum guards to detect mismatched pack/unpacks.
///  - Detection of invalid destination ghosts.
///
//#define TORQUE_DEBUG_NET

/// Define me to enable detailed console logging of net moves.
//#define TORQUE_DEBUG_NET_MOVES

/// Enable this define to change the default Net::MaxPacketDataSize
/// Do this at your own risk since it has the potential to cause packets
/// to be split up by old routers and Torque does not have a mechanism to
/// stitch split packets back together. Using this define can be very useful
/// in controlled network hardware environments (like a LAN) or for singleplayer
/// games (like BArricade and its large paths)
//#define MAXPACKETSIZE 1500

/// Modify me to enable metric gathering code in the renderers.
///
/// 0 does nothing; higher numbers enable higher levels of metric gathering.
//#define TORQUE_GATHER_METRICS 0

/// Define me if you want to enable debug guards in the memory manager.
///
/// Debug guards are known values placed before and after every block of
/// allocated memory. They are checked periodically by Memory::validate(),
/// and if they are modified (indicating an access to memory the app doesn't
/// "own"), an error is flagged (ie, you'll see a crash in the memory
/// manager's validate code). Using this and a debugger, you can track down
/// memory corruption issues quickly.
//#define TORQUE_DEBUG_GUARD

/// Define me if you want to enable instanced-static behavior
//#define TORQUE_ENABLE_THREAD_STATICS

/// Define me if you want to gather static-usage metrics
//#define TORQUE_ENABLE_THREAD_STATIC_METRICS

/// Define me if you want to enable debug guards on the FrameAllocator.
/// 
/// This is similar to the above memory manager guards, but applies only to the
/// fast FrameAllocator temporary pool memory allocations. The guards are only
/// checked when the FrameAllocator frees memory (when it's water mark changes).
/// This is most useful for detecting buffer overruns when using FrameTemp<> .
/// A buffer overrun in the FrameAllocator is unlikely to cause a crash, but may
/// still result in unexpected behavior, if other FrameTemp's are stomped.
//#define FRAMEALLOCATOR_DEBUG_GUARD

/// This #define is used by the FrameAllocator to set the size of the frame.
///
/// It was previously set to 3MB but I've increased it to 16MB due to the
/// FrameAllocator being used as temporary storage for bitmaps in the D3D9
/// texture manager.
#define TORQUE_FRAME_SIZE     16 << 20

// Finally, we define some dependent #defines. This enables some subsidiary
// functionality to get automatically turned on in certain configurations.

#ifdef TORQUE_DEBUG

   #define TORQUE_GATHER_METRICS 0
   #define TORQUE_ENABLE_PROFILE_PATH
   #ifndef TORQUE_DEBUG_GUARD
      #define TORQUE_DEBUG_GUARD
   #endif
   #ifndef TORQUE_NET_STATS
      #define TORQUE_NET_STATS
   #endif

   // Enables the C++ assert macros AssertFatal, AssertWarn, etc.
   #define TORQUE_ENABLE_ASSERTS

#endif

#ifdef TORQUE_RELEASE
  // If it's not DEBUG, it's a RELEASE build, put appropriate things here.
#endif

#ifdef TORQUE_SHIPPING

    // TORQUE_SHIPPING flags here.

#else

   // Enable the profiler by default, if we're not doing a shipping build.
   #define TORQUE_ENABLE_PROFILER

   // Enable the TorqueScript assert() instruction if not shipping.
   #define TORQUE_ENABLE_SCRIPTASSERTS

   // We also enable GFX debug events for use in Pix and other graphics
   // debugging tools.
   #define TORQUE_ENABLE_GFXDEBUGEVENTS

#endif

#ifdef TORQUE_TOOLS
#  define TORQUE_INSTANCE_EXCLUSION   "TorqueToolsTest"
#else
#  define TORQUE_INSTANCE_EXCLUSION   "TorqueTest"
#endif

// Someday, it might make sense to do some pragma magic here so we error
// on inconsistent flags.

// The Xbox360 has it's own profiling tools, the Torque Profiler should not be used
#ifdef TORQUE_OS_XENON
#  ifdef TORQUE_ENABLE_PROFILER
#     undef TORQUE_ENABLE_PROFILER
#  endif
#
#  ifdef TORQUE_ENABLE_PROFILE_PATH
#     undef TORQUE_ENABLE_PROFILE_PATH
#endif
#endif


#endif // _TORQUECONFIG_H_

