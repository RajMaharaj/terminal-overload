// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _T3D_PHYSICS_PXBODY_H_
#define _T3D_PHYSICS_PXBODY_H_

#ifndef _T3D_PHYSICS_PHYSICSBODY_H_
#include "T3D/physics/physicsBody.h"
#endif
#ifndef _PHYSICS_PHYSICSUSERDATA_H_
#include "T3D/physics/physicsUserData.h"
#endif
#ifndef _REFBASE_H_
#include "core/util/refBase.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif

class PxWorld;
class NxActor;
class PxCollision;
class NxMaterial;


class PxBody : public PhysicsBody
{
protected:

   /// The physics world we are in.
   PxWorld *mWorld;

   /// The physics actor.
   NxActor *mActor;

   /// The unshared local material used on all the 
   /// shapes on this actor.
   NxMaterial *mMaterial;

   /// We hold the collision reference as it contains
   /// allocated objects that we own and must free.
   StrongRefPtr<PxCollision> mColShape;

   /// 
   MatrixF mInternalTransform;

   /// The body flags set at creation time.
   U32 mBodyFlags;

   /// Is true if this body is enabled and active
   /// in the simulation of the scene.
   bool mIsEnabled;

   ///
   void _releaseActor();

public:

   PxBody();
   virtual ~PxBody();

   // PhysicsObject
   virtual PhysicsWorld* getWorld();
   virtual void setTransform( const MatrixF &xfm );
   virtual MatrixF& getTransform( MatrixF *outMatrix );
   virtual Box3F getWorldBounds();
   virtual void setSimulationEnabled( bool enabled );
   virtual bool isSimulationEnabled() { return mIsEnabled; }

   // PhysicsBody
   virtual bool init(   PhysicsCollision *shape, 
                        F32 mass,
                        U32 bodyFlags,
                        SceneObject *obj, 
                        PhysicsWorld *world );
   virtual bool isDynamic() const;
   virtual PhysicsCollision* getColShape();
   virtual void setSleepThreshold( F32 linear, F32 angular );
   virtual void setDamping( F32 linear, F32 angular );
   virtual void getState( PhysicsState *outState );
   virtual F32 getMass() const;
   virtual Point3F getCMassPosition() const;
   virtual void setLinVelocity( const Point3F &vel );
   virtual void setAngVelocity( const Point3F &vel );
   virtual Point3F getLinVelocity() const;
   virtual Point3F getAngVelocity() const;
   virtual void setSleeping( bool sleeping );
   virtual void setMaterial(  F32 restitution,
                              F32 friction, 
                              F32 staticFriction );
   virtual void applyCorrection( const MatrixF &xfm );
   virtual void applyImpulse( const Point3F &origin, const Point3F &force );
};

#endif // _T3D_PHYSICS_PXBODY_H_
