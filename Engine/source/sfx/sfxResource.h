// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _SFXRESOURCE_H_
#define _SFXRESOURCE_H_

#ifndef _SFXCOMMON_H_
   #include "sfx/sfxCommon.h"
#endif
#ifndef __RESOURCE_H__
   #include "core/resource.h"
#endif


class SFXStream;


/// This is the base class for all sound file resources including
/// streamed sound files.  It acts much like an always in-core
/// header to the actual sound data which is read through an SFXStream.
///
/// The first step occurs at ResourceManager::load() time at which
/// only the header information, the format, size frequency, and 
/// looping flag, are loaded from the sound file.  This provides 
/// just the nessasary information to simulate sound playback for
/// sounds playing just out of the users hearing range.
/// 
/// The second step loads the actual sound data or begins filling
/// the stream buffer.  This is triggered by a call to openStream().
/// SFXProfile, for example, does this when mPreload is enabled.
///
class SFXResource
{
   public:
   
      typedef void Parent;
      
   protected:

      /// The constructor is protected. 
      /// @see SFXResource::load()
      SFXResource();

      /// Path to the sound file.
      String mFileName;

      /// The format of the sample data.
      SFXFormat mFormat;

      /// The length of the sample in milliseconds.
      U32 mDuration;
      
      /// Construct a resource instance for the given file.  Format and duration
      /// are read from the given stream.
      SFXResource( String fileName, SFXStream* stream );
      
   public:

      /// The destructor.
      virtual ~SFXResource() {}

      /// This is a helper function used by SFXProfile for load
      /// a sound resource.  It takes care of trying different 
      /// types for extension-less filenames.
      ///
      /// @param filename The sound file path with or without extension.
      ///
      static Resource< SFXResource > load( String filename );

      /// A helper function which returns true if the 
      /// sound resource exists.
      ///
      /// @param filename The sound file path with or without extension.
      ///
      static bool exists( String filename );

      /// Return the path to the sound file.
      const String& getFileName() { return mFileName; }

      /// Returns the total playback time milliseconds.
      U32 getDuration() const { return mDuration; }

      /// The format of the data in the resource.
      const SFXFormat& getFormat() const { return mFormat; }

      /// Open a stream for reading the resource's sample data.
      SFXStream* openStream();

      // Internal.
      struct _NewHelper;
      friend struct _NewHelper;
};


#endif  // _SFXRESOURCE_H_
