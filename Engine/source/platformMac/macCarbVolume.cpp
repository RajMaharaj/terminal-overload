// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include <CoreServices/CoreServices.h>

#include "platform/platform.h"
#include "platformMac/macCarbVolume.h"
#include "platform/platformVolume.h"
#include "console/console.h"


//#define DEBUG_SPEW


struct MacFileSystemChangeNotifier::Event
{
   FSEventStreamRef mStream;
   Torque::Path mDir;
   bool mHasChanged;
};


static void fsNotifyCallback(
   ConstFSEventStreamRef stream,
   void* callbackInfo,
   size_t numEvents,
   void* eventPaths,
   const FSEventStreamEventFlags eventFlags[],
   const FSEventStreamEventId eventIds[] )
{
   MacFileSystemChangeNotifier::Event* event =
      reinterpret_cast< MacFileSystemChangeNotifier::Event* >( callbackInfo );
      
   // Defer handling this to internalProcessOnce() so we stay in
   // line with how the volume system expects notifications to
   // be reported.
      
   event->mHasChanged = true;
}

//-----------------------------------------------------------------------------
//    Change notifications.
//-----------------------------------------------------------------------------


MacFileSystemChangeNotifier::MacFileSystemChangeNotifier( MacFileSystem* fs )
   : Parent( fs )
{
   VECTOR_SET_ASSOCIATION( mEvents );   
}

MacFileSystemChangeNotifier::~MacFileSystemChangeNotifier()
{
   for( U32 i = 0, num = mEvents.size(); i < num; ++ i )
   {
      FSEventStreamStop( mEvents[ i ]->mStream );
      FSEventStreamInvalidate( mEvents[ i ]->mStream );
      FSEventStreamRelease( mEvents[ i ]->mStream );
      
      SAFE_DELETE( mEvents[ i ] );
   }
}

void MacFileSystemChangeNotifier::internalProcessOnce()
{
   for( U32 i = 0; i < mEvents.size(); ++ i )
      if( mEvents[ i ]->mHasChanged )
      {
         // Signal the change.
         
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[MacFileSystemChangeNotifier] Directory %i changed: '%s'",
            i + 1, mEvents[ i ]->mDir.getFullPath().c_str() );
         #endif
         
         internalNotifyDirChanged( mEvents[ i ]->mDir );
         mEvents[i ]->mHasChanged = false;
      }
}

bool MacFileSystemChangeNotifier::internalAddNotification( const Torque::Path& dir )
{
   // Map the path.
   
   Torque::Path fullFSPath = mFS->mapTo( dir );
   String osPath = PathToOS( fullFSPath );

   // Create event stream.
   
   Event* event = new Event;
   
   CFStringRef path = CFStringCreateWithCharacters( NULL, osPath.utf16(), osPath.numChars() );
   CFArrayRef paths = CFArrayCreate( NULL, ( const void** ) &path, 1, NULL );

   FSEventStreamRef stream;
   CFAbsoluteTime latency = 3.f;

   FSEventStreamContext context;
   dMemset( &context, 0, sizeof( context ) );
   context.info = event;
   
   stream = FSEventStreamCreate(
      NULL,
      &fsNotifyCallback,
      &context,
      paths,
      kFSEventStreamEventIdSinceNow,
      latency,
      kFSEventStreamCreateFlagNone
   );
   
   event->mStream = stream;
   event->mDir = dir;
   event->mHasChanged = false;
   
   mEvents.push_back( event );

   // Put it in the run loop and start the stream.
   
   FSEventStreamScheduleWithRunLoop( stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
   FSEventStreamStart( stream );
   
   CFRelease( path );
   CFRelease( paths );
         
   #ifdef DEBUG_SPEW
   Platform::outputDebugString( "[MacFileSystemChangeNotifier] Added change notification %i to '%s' (full path: %s)",
      mEvents.size(), dir.getFullPath().c_str(), osPath.c_str() );
   #endif
   
   return true;
}

bool MacFileSystemChangeNotifier::internalRemoveNotification( const Torque::Path& dir )
{
   for( U32 i = 0, num = mEvents.size(); i < num; ++ i )
      if( mEvents[ i ]->mDir == dir )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[MacFileSystemChangeNotifier] Removing change notification %i from '%s'",
            i + 1, dir.getFullPath().c_str() );
         #endif
         
         FSEventStreamStop( mEvents[ i ]->mStream );
         FSEventStreamInvalidate( mEvents[ i ]->mStream );
         FSEventStreamRelease( mEvents[ i ]->mStream );
         
         SAFE_DELETE( mEvents[ i ] );
         
         mEvents.erase( i );
         
         return true;
      }
      
   return false;
}

//-----------------------------------------------------------------------------
//    Platform API.
//-----------------------------------------------------------------------------

Torque::FS::FileSystemRef Platform::FS::createNativeFS( const String &volume )
{
   return new MacFileSystem( volume );
}

bool Torque::FS::VerifyWriteAccess(const Torque::Path &path)
{
   return true;
}
