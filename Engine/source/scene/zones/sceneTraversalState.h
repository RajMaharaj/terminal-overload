// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _SCENETRAVERSALSTATE_H_
#define _SCENETRAVERSALSTATE_H_

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif

#ifndef _MBOX_H_
#include "math/mBox.h"
#endif

#ifndef _MPLANESET_H_
#include "math/mPlaneSet.h"
#endif


class SceneCullingState;
class SceneCullingVolume;
class SceneZoneSpace;


/// Temporary state for zone traversals.  Keeps track of the zones
/// that were visited in the current traversal chain as well as of
/// the total area that so far has been visited in the traversal.
class SceneTraversalState
{
   protected:

      /// Scene culling state.
      SceneCullingState* mCullingState;

      /// Stack of zones visited in current traversal chain.
      Vector< U32 > mZoneStack;

      /// Stack of culling volumes.  Topmost is current volume.
      Vector< SceneCullingVolume > mCullingVolumeStack;
      
      /// Area of scene visited in traversal.
      Box3F mTraversedArea;

   public:

      /// Construct an empty scene traversal state.
      /// @param cullingState Scene culling state.
      SceneTraversalState( SceneCullingState* cullingState );

      /// Return the scene culling state for this traversal.
      SceneCullingState* getCullingState() const { return mCullingState; }

      /// Get the scene area that has been visited so far in the traversal.
      const Box3F& getTraversedArea() const { return mTraversedArea; }

      /// Add an area to what has been visited so far in the traversal.
      void addToTraversedArea( const Box3F& area ) { mTraversedArea.intersect( area ); }

      /// @name Zone Stack
      /// @{

      /// Return the number of zones that are currently on the traversal stack.
      U32 getTraversalDepth() const { return mZoneStack.size(); }

      /// Push a zone onto the traversal stack.
      void pushZone( U32 zoneId ) { mZoneStack.push_back( zoneId ); }

      /// Pop the topmost zone from the traversal stack.
      void popZone() { mZoneStack.pop_back(); }

      /// Return true if the given zone has already been visited in the
      /// current traversal chain, i.e. if it is currently on the traversal
      /// stack.
      bool haveVisitedZone( U32 zoneId ) const { return mZoneStack.contains( zoneId ); }

      /// Return the zone ID of the topmost zone on the traversal stack.
      U32 getZoneIdFromStack( U32 depth = 0 ) { return mZoneStack[ mZoneStack.size() - 1 - depth ]; }

      /// Return the zone space that owns the topmost zone on the traversal stack.
      SceneZoneSpace* getZoneFromStack( U32 depth = 0 );

      /// @}

      /// @name Culling Volume Stack
      /// @{

      /// Push a culling volume onto the stack so that it becomes the current culling
      /// volume.
      void pushCullingVolume( const SceneCullingVolume& volume );

      /// Pop the current culling volume from the stack.
      void popCullingVolume();

      ///
      const SceneCullingVolume& getCurrentCullingVolume() const { return mCullingVolumeStack.last(); }

      /// @}
};

#endif // !_SCENETRAVERSALSTATE_H_
