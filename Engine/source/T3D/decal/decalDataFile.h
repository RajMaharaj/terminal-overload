// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _DECALDATAFILE_H_
#define _DECALDATAFILE_H_

#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

#ifndef _DECALSPHERE_H_
#include "T3D/decal/decalSphere.h"
#endif


class Stream;
class DecalData;



/// This is the data file for decals.
/// Not intended to be used directly, do your work with decals
/// via the DecalManager.
class DecalDataFile
{
   protected:

      enum { FILE_VERSION = 5 };

      /// Set to true if the file is dirty and
      /// needs to be saved before being destroyed.
      bool mIsDirty;

      /// @name Memory Management
      /// @{

      /// Allocator for DecalInstances.
      FreeListChunker< DecalInstance > mChunker;

      /// Allocate a new, uninitialized DecalInstance.
      DecalInstance* _allocateInstance() { return mChunker.alloc(); }

      /// Free the memory of the given DecalInstance.
      void _freeInstance( DecalInstance *decal ) { mChunker.free( decal ); }

      /// @}
      
      /// @name Instance Management
      /// @{

      /// The decal sphere that we have last insert an item into.  This sphere
      /// is most likely to be a good candidate for the next insertion so
      /// test this sphere first.
      DecalSphere* mSphereWithLastInsertion;

      /// List of bounding sphere shapes that contain and organize
      /// DecalInstances for optimized culling and lookup.
      Vector< DecalSphere* > mSphereList;

      /// Add the given decal to the sphere list.
      void _addDecalToSpheres( DecalInstance *inst );

      /// Remove the decal from the sphere list.
      bool _removeDecalFromSpheres( DecalInstance *inst );

      /// @}
   
   public:

      DecalDataFile();
      virtual ~DecalDataFile();

      Vector< DecalSphere* >& getSphereList() { return mSphereList; }
      const Vector< DecalSphere* >& getSphereList() const { return mSphereList; }

      /// Return true if the decal data has been modified since the last save or load.
      bool isDirty() const { return mIsDirty; }

      /// Deletes all the data and resets the 
      /// file to an empty state.
      void clear();

      /// @name I/O
      /// @{

      /// Write the decal data to the given stream.
      bool write( Stream& stream );

      /// Read the decal data from the given stream.
      bool read( Stream& stream );

      /// @}

      /// @name Decal Management
      /// @{

      /// Create a new decal in this file using the given data.
      DecalInstance* addDecal( const Point3F& pos, const Point3F& normal, const Point3F& tangent,
                               DecalData* decalData, F32 decalScale, S32 decalTexIndex, U8 flags );

      /// Remove a decal from the file.
      void removeDecal( DecalInstance *inst );

      /// Let the file know that the data of the given decal has changed.
      void notifyDecalModified( DecalInstance *inst );

      /// @}
};

#endif // _DECALDATAFILE_H_
