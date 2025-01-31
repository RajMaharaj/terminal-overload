// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "T3D/fx/windEmitter.h"

#include "math/mBox.h"
#include "core/tAlgorithm.h"
#include "platform/profiler.h"


Vector<WindEmitter*> WindEmitter::smAllEmitters;


WindEmitter::WindEmitter()
{
   smAllEmitters.push_back( this );

   mEnabled = true;
   mScore = 0.0f;
   mSphere.center.zero();
   mSphere.radius = 0.0f;
   mStrength = 0.0f;
   mTurbulenceFrequency = 0.0f;
   mTurbulenceStrength = 0.0f;
   mVelocity.zero();
}

WindEmitter::~WindEmitter()
{
   WindEmitterList::iterator iter = find( smAllEmitters.begin(), smAllEmitters.end(), this );
   smAllEmitters.erase( iter );
}

void WindEmitter::setPosition( const Point3F& pos )
{
   mSphere.center = pos;
}

void WindEmitter::update( const Point3F& pos, const VectorF& velocity )
{
   mSphere.center = pos;
   mVelocity = velocity;
}

void WindEmitter::setRadius( F32 radius )
{
   mSphere.radius = radius;
}

void WindEmitter::setStrength( F32 strength )
{
   mStrength = strength;
}

void WindEmitter::setTurbulency( F32 frequency, F32 strength )
{
   mTurbulenceFrequency = frequency;
   mTurbulenceStrength = strength;
}

S32 QSORT_CALLBACK WindEmitter::_sortByScore(const void* a, const void* b)
{
	return mSign((*(WindEmitter**)b)->mScore - (*(WindEmitter**)a)->mScore);
}

bool WindEmitter::findBest(   const Point3F& cameraPos, 
                              const VectorF& cameraDir,
                              F32 viewDistance,
                              U32 maxResults,
                              WindEmitterList* results )
{
   PROFILE_START(WindEmitter_findBest);

   // Build a sphere from the camera point.
	SphereF cameraSphere;
   cameraSphere.center = cameraPos;
   cameraSphere.radius = viewDistance;

   // Collect the active spheres within the camera space and score them.
   WindEmitterList best;
   WindEmitterList::iterator iter = smAllEmitters.begin();
   for ( ; iter != smAllEmitters.end(); iter++ )
   {        
      const SphereF& sphere = *(*iter);

      // Skip any spheres outside of our camera range or that are disabled.
      if ( !(*iter)->mEnabled || !cameraSphere.isIntersecting( sphere ) )
         continue;

      // Simple score calculation...
      //
      // score = ( radius / distance to camera ) * dot( cameraDir, vector from camera to sphere )
      //
      Point3F vect = sphere.center - cameraSphere.center;
      F32 dist = vect.len();
      (*iter)->mScore = dist * sphere.radius;
      vect /= getMax( dist, 0.001f );
      (*iter)->mScore *= mDot( vect, cameraDir );

      best.push_back( *iter );
   }

   // Sort the results by score!
   dQsort( best.address(), best.size(), sizeof(WindEmitter*), &WindEmitter::_sortByScore );

   // Clip the results to the max requested.
   if ( best.size() > maxResults )
      best.setSize( maxResults );

   // Merge the results and return.
   results->merge( best );

   PROFILE_END(); // WindEmitter_findBest

   return best.size() > 0;
}
