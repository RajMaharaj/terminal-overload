// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "NOTC/nortdisc.h"

#include "scene/sceneRenderState.h"
#include "scene/sceneManager.h"
#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"
#include "console/consoleTypes.h"
#include "console/typeValidators.h"
#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "T3D/fx/explosion.h"
#include "T3D/shapeBase.h"
#include "ts/tsShapeInstance.h"
#include "sfx/sfxTrack.h"
#include "sfx/sfxSource.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxTypes.h"
#include "math/mathUtils.h"
#include "math/mathIO.h"
#include "sim/netConnection.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/fx/splash.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsWorld.h"
#include "gfx/gfxTransformSaver.h"
#include "T3D/containerQuery.h"
#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalData.h"
#include "T3D/lightDescription.h"
#include "console/engineAPI.h"

IMPLEMENT_CO_DATABLOCK_V1(NortDiscData);
IMPLEMENT_CO_NETOBJECT_V1(NortDisc);

//--------------------------------------------------------------------------

//#define NORTDISC_DEBUG

#ifdef TORQUE_DEBUG
# ifdef NORTDISC_DEBUG
#  define DEBUG(x) (Con::errorf x)
# else
#  define DEBUG(x)
# endif
#else
# define DEBUG(x)
#endif

//--------------------------------------------------------------------------

namespace {
	MRandomLCG sgRandom(0x1);
}

//--------------------------------------------------------------------------

//IMPLEMENT_CO_SERVEREVENT_V1(NortDiscHitEvent);
//
//NortDiscHitEvent::NortDiscHitEvent()
//{
//	mGuaranteeType = Guaranteed;
//
//	mObjId = mDiscId = -1;
//	mDeflected = false;
//}
//
//
//NortDiscHitEvent::NortDiscHitEvent(const RayInfo& rInfo, U32 objId, 
//						U32 discId, const Point3F& vec, bool deflected)
//{
//	mInfo.point  = rInfo.point;
//	mInfo.normal = rInfo.normal;
//	mInfo.t      = rInfo.t;
//	mObjId = objId;
//	mDiscId = discId;
//	mVec = vec;
//	mDeflected = deflected;
//}
//
//void NortDiscHitEvent::pack(NetConnection* conn, BitStream* bstream)
//{
//	mathWrite(*bstream, mInfo.point);
//	mathWrite(*bstream, mInfo.normal);
//	bstream->write(mInfo.t);
//	bstream->writeRangedU32(mObjId, 0, NetConnection::MaxGhostCount);
//	bstream->writeRangedU32(mDiscId, 0, NetConnection::MaxGhostCount);
//	mathWrite(*bstream, mVec);
//	bstream->writeFlag(mDeflected);
//}
//
//void NortDiscHitEvent::unpack(NetConnection* conn, BitStream* bstream)
//{
//	mathRead(*bstream, &mInfo.point);
//	mathRead(*bstream, &mInfo.normal);
//	bstream->read(&mInfo.t);
//	mObjId  = bstream->readRangedU32(0, NetConnection::MaxGhostCount);
//	mDiscId = bstream->readRangedU32(0, NetConnection::MaxGhostCount);
//	mathRead(*bstream, &mVec);
//	mDeflected = bstream->readFlag();
//}
//
//void NortDiscHitEvent::write(NetConnection* conn, BitStream* bstream)
//{
//	this->pack(conn,bstream);
//}
//
//void NortDiscHitEvent::process(NetConnection* conn)
//{
//	// TODO: implement more anti-cheating checks
//
//	GameConnection* gameconn = dynamic_cast<GameConnection*>(conn);
//	if(!gameconn) return;
//
//	NortDisc* disc = NULL;
//	ShapeBase* shape = NULL;
//
//	NetObject* netObj = conn->resolveObjectFromGhostIndex((S32)mObjId);
//	shape = dynamic_cast<ShapeBase*>(netObj);
//	if(!shape) return;
//
//	netObj = conn->resolveObjectFromGhostIndex((S32)mDiscId);
//	disc = dynamic_cast<NortDisc*>(netObj);
//	if(!disc) return;
//
//	mInfo.object = shape;
//
//	// a client is only allowed "to speak"
//	// for his own control object...
//	if( shape != gameconn->getControlObject() )
//		return;		
//
//	if(mDeflected)
//	{
//		// we currently trust our clients 100%...
//		DEBUG(("NortDiscHitEvent: disc has been deflected"));
//	}
//	else
//	{
//		DEBUG(("NortDiscHitEvent: disc has not been deflected"));
//	}
//
//	disc->processNortDiscHitEvent(conn,shape,mInfo,mVec,mDeflected);
//}

//--------------------------------------------------------------------------
//
NortDiscData::NortDiscData()
{
	maxVelocity = muzzleVelocity;
	acceleration = 0;
	startVertical = false;
}

//--------------------------------------------------------------------------

void NortDiscData::initPersistFields()
{
   Parent::initPersistFields();

   addField("maxVelocity",   TypeF32,  Offset(maxVelocity, NortDiscData));
   addField("acceleration",  TypeF32,  Offset(acceleration, NortDiscData));
   addField("startVertical", TypeBool,  Offset(startVertical, NortDiscData));
}

//--------------------------------------------------------------------------

bool NortDiscData::onAdd()
{
   if(!Parent::onAdd())
      return false;

   return true;
}


bool NortDiscData::preload(bool server, String &errorStr)
{
   if (Parent::preload(server, errorStr) == false)
      return false;

   return true;
}

//--------------------------------------------------------------------------

void NortDiscData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->write(maxVelocity);
   stream->write(acceleration);

   stream->writeFlag(startVertical);
}

void NortDiscData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   stream->read(&maxVelocity);
   stream->read(&acceleration);

   startVertical = stream->readFlag();
}


//--------------------------------------------------------------------------

NortDisc::NortDisc()
{
	mState = Attacking;
	mSpinTargetPos.set(0, 0, 0);
}

NortDisc::~NortDisc()
{

}

void NortDisc::onDeleteNotify(SimObject* obj)
{
	Parent::onDeleteNotify(obj);
}

//--------------------------------------------------------------------------

void NortDisc::initPersistFields()
{
   Parent::initPersistFields();
}

void NortDisc::consoleInit()
{
	Parent::consoleInit();

	Con::setIntVariable("$NortDisc::Attacking",Attacking);
	Con::setIntVariable("$NortDisc::Returning",Returning);
	Con::setIntVariable("$NortDisc::Deflected",Deflected);
}


//--------------------------------------------------------------------------

bool NortDisc::onAdd()
{
   if(!Parent::onAdd())
      return false;

	// disc starts parallel to horizon by default...
	MatrixF mat = MathUtils::createOrientFromDir(mCurrVelocity);
	mat.setPosition(mCurrPosition);
	this->setTransform(mat);

	if(mDataBlock->startVertical)
	{
		MatrixF rot(EulerF(0, M_PI_F / 2, 0));
      MatrixF tmp = mat;
		mat.mul(tmp, rot);
		this->setTransform(mat);
	}

	// pretty big (currently hardcoded) bounding box
	// to make targeting a disc possible...
	mObjBox.minExtents = Point3F(-2,-2,-2);
	mObjBox.maxExtents = Point3F(2,2,2);
	resetWorldBox();
      
   return true;
}


void NortDisc::onRemove()
{

   Parent::onRemove();
}


bool NortDisc::onNewDataBlock(GameBaseData* dptr, bool reload)
{
   mDataBlock = dynamic_cast<NortDiscData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr, reload))
      return false;

   return true;
}

//----------------------------------------------------------------------------

bool NortDisc::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   // Collision disabled when hidden or deflected.
   if(mState == Deflected)
      return false;

   // Collide against bounding box. Need at least this for the editor.
   F32 st,et,fst = 0,fet = 1;
   F32 *bmin = &mObjBox.minExtents.x;
   F32 *bmax = &mObjBox.maxExtents.x;
   F32 const *si = &start.x;
   F32 const *ei = &end.x;

   for (int i = 0; i < 3; i++) {
      if (*si < *ei) {
         if (*si > *bmax || *ei < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si < *bmin)? (*bmin - *si) / di: 0;
         et = (*ei > *bmax)? (*bmax - *si) / di: 1;
      }
      else {
         if (*ei > *bmax || *si < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si > *bmax)? (*bmax - *si) / di: 0;
         et = (*ei < *bmin)? (*bmin - *si) / di: 1;
      }
      if (st > fst) fst = st;
      if (et < fet) fet = et;
      if (fet < fst)
         return false;
      bmin++; bmax++;
      si++; ei++;
   }

   info->normal = start - end;
   info->normal.normalizeSafe();
   getTransform().mulV( info->normal );

   info->t = fst;
   info->object = this;
   info->point.interpolate(start,end,fst);
   info->material = 0;
   return true;
}

//----------------------------------------------------------------------------

class ObjectDeleteEvent : public SimEvent
{
public:
   void process(SimObject *obj)
   {
      obj->deleteObject();
   }
};

void NortDisc::processTick(const Move* move)
{
   //Parent::processTick(move);

   mCurrTick++;

   // See if we can get out of here the easy way ...
	if( isServerObject() && mCurrTick >= mDataBlock->lifetime )
	{
      deleteObject();
      return;
	}

	// since deflected discs are harmless eyecandy, they just linger 
	// on the server waiting for the end of their lifetime to come...
	if(isServerObject() && mState == Deflected)
		return;

	//
	// ... otherwise, we have to do some simulation work...
	//


	if(mState == Attacking && mTarget)		
	{
		U32 targetType = mTarget->getTypeMask();
		if(targetType & ProjectileObjectType)
		{
			if(((NortDisc*)mTarget)->mState == Deflected)
				this->onTargetLost();
		}
		else if(targetType & ShapeBaseObjectType)
		{
			if(((ShapeBase*)mTarget)->getDamageState() != ShapeBase::Enabled)
				this->onTargetLost();
		}
	}

	F32 timeLeft;
	RayInfo rInfo;
	Point3F oldPosition;
	Point3F newPosition;

	oldPosition = mCurrPosition;

	// acceleration...
	F32 currSpeed = mCurrVelocity.len();
	F32 newSpeed = currSpeed + mDataBlock->acceleration;
	newSpeed = mClampF(newSpeed, 0, mDataBlock->maxVelocity);
	mCurrVelocity.normalize();
	mCurrVelocity *= newSpeed;

	// ballistic?...
	if(mDataBlock->isBallistic)
	  mCurrVelocity.z -= 9.81 * mDataBlock->gravityMod * (F32(TickMs) / 1000.0f);

	F32 oldYaw, oldPitch;
	F32 newYaw, newPitch;

	MathUtils::getAnglesFromVector(mCurrVelocity,oldYaw,oldPitch);

	// target-tracking?
	if( mState == Deflected )
	{
		// spin around...
		this->updateSpin();
	}
	else if( mTarget && mDataBlock->maxTrackingAbility > 0 )
		updateTargetTracking();

	MathUtils::getAnglesFromVector(mCurrVelocity,newYaw,newPitch);
	//Con::errorf("old: %f %f \t new: %f %f",oldYaw,oldPitch,newYaw,newPitch);

	newPosition = oldPosition + mCurrVelocity * (F32(TickMs) / 1000.0f);

	timeLeft = 1.0;

	//
	// check for bounce, collision, etc...
	//

	// don't collide with ourself...
	this->disableCollision();

	// never collide with source object when attacking...
	bool noCollisionWithSource = mSourceObject && mState == Attacking;
	if(noCollisionWithSource)
		mSourceObject->disableCollision();

   // Determine if the projectile is going to hit any object between the previous
   // position and the new position. This code is executed both on the server
   // and on the client (for prediction purposes). It is possible that the server
   // will have registered a collision while the client prediction has not. If this
   // happens the client will be corrected in the next packet update.
	bool collision = this->findCollision(oldPosition, newPosition, &rInfo);
	if(collision)
	{
      SceneObject* col = rInfo.object;

		//mTraveledDistance += (rInfo.point-oldPosition).len();

      this->setMaskBits(MovementMask);

	   this->onCollision(rInfo.point, rInfo.normal, col);

	   if(col == mTarget)
	   {			
		   DEBUG(("%s: hit target!", isGhost() ? "CLNT" : "SRVR"));
		   if(isServerObject())
			   Con::executef(mDataBlock, "onHitTarget", getIdString());
	   }
	   else
	   {
		   // re-set our target...
		   //this->setTarget(mTarget);
	   }

		// returned to source?
		if(mState == Returning && col == mTarget)
		{
			this->enableCollision();

			if(noCollisionWithSource)
				mSourceObject->enableCollision();

			if(isServerObject())
			{
				DEBUG(("NortDisc: server: disc retourned to source @ %u",Platform::getVirtualMilliseconds()));
				this->deleteObject();
			}
			else
			{
				DEBUG(("NortDisc: client: disc retourned to source @ %u",Platform::getVirtualMilliseconds()));				
            mHasExploded = true;
			}

			return;
		}


	   bool bounce = true;

	   // don't bounce if the target was a ShapeBase and we killed it...
	   if(col && col->getTypeMask() & ShapeBaseObjectType)
	   {
		   ShapeBase* shape = (ShapeBase*)col;
		   ShapeBase::DamageState state = shape->getDamageState();
		   if( state  == ShapeBase::Disabled || state == ShapeBase::Destroyed )
		   {
			   mCurrTrackingAbility = 0; 
			   bounce = false;
		   }
	   }

		if(bounce)
		{
			// let's bounce...
			newPosition = this->bounce(rInfo, mCurrVelocity, true);

			//
			if(mState == Deflected)
			{
				Point3F randVec;
				randVec.x = sgRandom.randF( -1.0, 1.0 );
				randVec.y = sgRandom.randF( -1.0, 1.0 );
				randVec.z = sgRandom.randF(  0.0, 1.0 );
				randVec.normalize();
				randVec *= 10;

				mSpinTargetPos = mCurrPosition + randVec;
			}
		}
	}
	else // no collision...
	{
		//mTraveledDistance += (newPosition-oldPosition).len();

		// check if we've missed our target...
		if(mTarget)
		{
			if(this->isServerObject() && this->missedObject(mTarget, oldPosition, newPosition))
				Con::executef(mDataBlock, "onMissedTarget", getIdString());
		}
	}

	this->enableCollision();

	if(noCollisionWithSource)
		mSourceObject->enableCollision();

   mCurrDeltaBase = newPosition;
	mCurrBackDelta = mCurrPosition - newPosition;

   if(isClientObject())
   {
      if(mEmissionCount == 0)
      {
         emitParticles(mInitialPosition, newPosition, mCurrVelocity, TickMs);
         this->addLaserTrailNode(mInitialPosition, false);
      }
      else
         emitParticles(mCurrPosition, newPosition, mCurrVelocity, TickMs);
      this->addLaserTrailNode(newPosition, true);
      mEmissionCount++;
   }

   mCurrPosition = newPosition;

	//
	// compute new transformation matrix...
	//

	if(isClientObject())
	{

		enum Roll { Left, Right } rollDir;

		MatrixF mat = this->getTransform();

		Point3F xv,yv,zv;

		yv = mCurrVelocity; yv.normalize();
		mat.getColumn(0,&xv); xv.normalize();

		F32 xv_yaw,xv_pitch;
		MathUtils::getAnglesFromVector(xv,xv_yaw,xv_pitch);

		//DEBUG(("NortDisc: xv_pitch: %f",xv_pitch));

		F32 roll = 0;
		F32 diff = mFabs(newYaw-oldYaw);
		if(diff < 0.05)
		{
			//DEBUG(("NortDisc: steered straight ahead"));

			// roll back to horizon...
			if(xv_pitch > 0)
				rollDir = Right;
			else
				rollDir = Left;

			F32 pitchAbs = mFabs(xv_pitch);
			if(pitchAbs < 0.1)
				roll = pitchAbs;
			else
				roll = 0.1;
		}
		else
		{
			if(newYaw - oldYaw > 0)
			{
				//DEBUG(("NortDisc: steered right"));
				rollDir = Right;
			}
			else
			{
				//DEBUG(("NortDisc: steered left"));
				rollDir = Left;
			}

			roll = diff;
		}

		if(rollDir == Left)
		{
			//DEBUG(("NortDisc: rolling left"));
			xv_pitch += roll;
		}
		else
		{
			//DEBUG(("NortDisc: rolling right"));
			xv_pitch -= roll;
		}

		MathUtils::getVectorFromAngles(xv,xv_yaw,xv_pitch);

		mCross(xv,yv,&zv); zv.normalize();
		mCross(yv,zv,&xv); xv.normalize();

		mat.setColumn(0,xv);
		mat.setColumn(1,yv);
		mat.setColumn(2,zv);
		mat.setPosition(mCurrPosition);

		this->setTransform(mat);

	}
	else // on the server
	{
		// server only cares about position...
		MatrixF xform(true);
		xform.setColumn(3, mCurrPosition);
		setTransform(xform);
	}
}

void NortDisc::interpolateTick(F32 delta)
{
   if( mHasExploded )
      return;

   Point3F interpPos = mCurrDeltaBase + mCurrBackDelta * delta;
   Point3F dir = mCurrVelocity;
   if(dir.isZero())
      dir.set(0,0,1);
   else
      dir.normalize();

   MatrixF xform = this->getRenderTransform();
   xform.setPosition(interpPos);
   setRenderTransform(xform);

   // fade out the projectile image
   S32 time = (S32)(mCurrTick - delta);
   if(time > mDataBlock->fadeDelay)
   {
      F32 fade = F32(time - mDataBlock->fadeDelay);
      mFadeValue = 1.0 - (fade / F32(mDataBlock->lifetime));
   }
   else
      mFadeValue = 1.0;

   this->interpolateLaserTrails(interpPos);

   updateSound();
}

//--------------------------------------------------------------------------

bool
NortDisc::findCollision(const Point3F &start, const Point3F &end, RayInfo* info)
{
	U32 colMask = csmStaticCollisionMask | csmDynamicCollisionMask;

	if(mState == Attacking)
		colMask |= ProjectileObjectType;

	bool collision = getContainer()->castRay(start, end, colMask, info);
	if(collision)
	{
		/*
		if(info->object->getType() & DeflectorObjectType)
		{
			Deflector* deflector = (Deflector*)info->object;
			DeflectorData* deflectorData = (DeflectorData*)deflector->getDataBlock();

			if(!deflector->isActive() || mDataBlock->penetrateDeflectorLevel >= deflectorData->level)
			{
				DEBUG(("%s: ignoring deflector and trying again...", isGhost() ? "CLNT" : "SRVR"));

				deflector->disableCollision();
				collision = findCollision(start, end, info);
				deflector->enableCollision();
			}
		}
		else*/ if(info->object->getTypeMask() & ProjectileObjectType)
		{
			Projectile* prj = (Projectile*)info->object;

			if(prj != mTarget)
			{
				DEBUG(("%s: ignoring non-target disc and trying again...", isGhost() ? "CLNT" : "SRVR"));

				prj->disableCollision();
				collision = findCollision(start, end, info);
				prj->enableCollision();
			}
		}
	}

	return collision;
}

void
NortDisc::updateSpin()
{
	mCurrTrackingAbility++;
	if(mCurrTrackingAbility >= 20)
	{
		mCurrTrackingAbility = 10;

		Point3F randVec;
		randVec.x = sgRandom.randF( -1.0, 1.0 );
		randVec.y = sgRandom.randF( -1.0, 1.0 );
		randVec.z = sgRandom.randF( -1.0, 1.0 );
		randVec.normalize();
		randVec *= 10;

		mSpinTargetPos = mCurrPosition + randVec;
	}

	Point3F targetPos, targetDir;
	F32 speed = mCurrVelocity.len();

	targetPos = mSpinTargetPos;
	targetDir = targetPos - mCurrPosition;
	targetDir.normalize();
	targetDir *= mCurrTrackingAbility;
	targetDir += mCurrVelocity;
	targetDir.normalize();

	mCurrVelocity = targetDir;
	mCurrVelocity *= speed; 
}

void
NortDisc::setTarget(GameBase* target)
{
	Parent::setTarget(target);
}

Point3F
NortDisc::bounce(const RayInfo& rInfo, const Point3F& vec, bool bounceExp)
{
	if( isServerObject() )
	{
		setMaskBits(MovementMask | BounceMask);

#if 0
		// no need to add new bouncePoints if the important ones have been sent already...
		if( !mBouncePointsSent && mNumBouncePoints < 10 )
		{
			mNumBouncePoints++;
			mBouncePoint[mNumBouncePoints-1].pos = rInfo.point;
			mBouncePoint[mNumBouncePoints-1].norm = rInfo.normal;
			mBouncePoint[mNumBouncePoints-1].decal = (
				!(rInfo.object->getType() & Projectile::csmDynamicCollisionMask)
				&& (rInfo.object->getType() & Projectile::csmStaticCollisionMask)
			);
		}
#endif
	}
	else
	{
      // Create bounce explosion.
      if(bounceExp)
         this->createBounceExplosion(rInfo);

		this->addLaserTrailNode(rInfo.point, true);

#if 0
		// NortDiscs create oriented decals when bouncing...
		if(mDataBlock->decalCount > 0
		&& !(rInfo.object->getType() & Projectile::csmDynamicCollisionMask)
		&& (rInfo.object->getType() & Projectile::csmStaticCollisionMask))
		{
			// randomly choose a decal between 0 and (decal count - 1)
			U32 idx = (U32)(mCeil(mDataBlock->decalCount * Platform::getRandom()) - 1.0f);

			// this should never choose a NULL idx, but check anyway
			if(mDataBlock->decals[idx] != NULL)
			{
				DecalManager *decalMngr = gClientSceneGraph->getCurrentDecalManager();
				if(decalMngr)
					decalMngr->addDecal(rInfo.point, vec, rInfo.normal, mDataBlock->decals[idx]);
			}
		}
#endif
	}

	// reset mCurrTrackingAbility to zero...
	mCurrTrackingAbility = 0;

	// reflect our velocity around the normal...
	Point3F bounceVel = vec - rInfo.normal * (mDot( vec, rInfo.normal ) * 2.0);
	mCurrVelocity = bounceVel;

	// Add in surface friction...
	Point3F tangent = bounceVel - rInfo.normal * mDot(bounceVel, rInfo.normal);
	mCurrVelocity  -= tangent * mDataBlock->bounceFriction;

	// Now, take elasticity into account for modulating the speed of the disc
	mCurrVelocity *= mDataBlock->bounceElasticity;

	U32 timeLeft = 1.0 * (1.0 - rInfo.t);
	Point3F oldPosition = rInfo.point + rInfo.normal * 0.05;
	Point3F newPosition = oldPosition + (mCurrVelocity * ((timeLeft/1000.0) * TickMs));

#if 0
	mBounceCount++;

	mTraveledDistance += (newPosition-oldPosition).len();
#endif

	return newPosition;
}

void
NortDisc::createExplosion(const Point3F& p, const Point3F& n)
{
	if(isServerObject() || !mDataBlock->explosion)
      return;

	Explosion* pExplosion = new Explosion;
   pExplosion->setPalette(this->getPalette());
	pExplosion->onNewDataBlock(mDataBlock->explosion, false);
   if(pExplosion)
   {
      MatrixF xform(true);
      xform.setPosition(p);
      pExplosion->setTransform(xform);
      pExplosion->setInitialState(p, n);
      if (pExplosion->registerObject() == false)
      {
         Con::errorf(ConsoleLogEntry::General, "NortDisc(%s)::createExplosion: couldn't register explosion",
                     mDataBlock->getName() );
         delete pExplosion;
         pExplosion = NULL;
      }
   }
}


//--------------------------------------------------------------------------
U32 NortDisc::packUpdate(NetConnection* con, U32 mask, BitStream* stream)
{
	U32 retMask = Parent::packUpdate(con, mask, stream);

	if(stream->writeFlag(mask & MovementMask))
	{
		stream->writeInt(mState,2);
	}

	return retMask;
}

void NortDisc::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);

	// movement mask
	if(stream->readFlag())
	{
		mState = (State)stream->readInt(2);

		if(mState == Deflected)
			this->deflected(mCurrVelocity);
	}
}

//--------------------------------------------------------------------------

void
NortDisc::deflected(const Point3F& newVel)
{
	if(isServerObject())
		Con::executef(mDataBlock, "onDeflected", getIdString());

	mCurrVelocity = newVel;
	mState = Deflected;
	this->setTarget(NULL);
	setMaskBits(MovementMask);

	//
	// eyecandy stuff...
	//

#if 0
	if(isClientObject())
	{
		mHidden = false;
		Point3F normal = newVel;
		normal.normalize();
		ExplosionType expType = HitDeflectorExplosion;
		this->createExplosion(mCurrPosition, normal, expType);
	}
#endif
}

void
NortDisc::stopAttacking(U32 targetType)
{
	if(mSourceObject)
	{
		// return to source...
		this->setTarget(mSourceObject);
		mState = Returning;
	}
	else
	{
		// clear target...
		this->setTarget(NULL);
	}

	setMaskBits(MovementMask);
}

//void
//NortDisc::processNortDiscHitEvent(NetConnection* conn, GameBase* obj,
//					const RayInfo& rInfo, const Point3F& vec, bool deflected)
//{
//	mTraveledDistance += (rInfo.point-mCurrPosition).len();
//
//	// don't do the onCollision script call if we've been deflected...
//	if(isServerObject() && !deflected && obj)
//		this->onCollision(rInfo.point,rInfo.normal,obj);
//
//	if(deflected)
//	{
//		Con::executef(mDataBlock, 2, "onDeflected", getIdString());
//
//		// clear target...
//		this->setTarget(NULL);
//
//		mState = Deflected;
//	}
//	else if(obj == mTarget)
//	{			
//		Con::executef(mDataBlock, 2, "onHitTarget", getIdString());
//		if(mSourceObject)
//		{
//			// return to source...
//			this->setTarget(mSourceObject);
//		}
//		else
//		{
//			// clear target...
//			this->setTarget(NULL);
//		}
//
//		mState = Returning;
//	}
//	else
//	{
//		// re-set our target...
//		this->setTarget(mTarget);
//	}
//
//
//	bool bounce = true;
//
//	// don't bounce if the target was a ShapeBase and we killed it...
//	if(obj && obj->getType() & ShapeBaseObjectType)
//	{
//		ShapeBase* shape = (ShapeBase*)obj;
//		ShapeBase::DamageState state = shape->getDamageState();
//		if( state  == ShapeBase::Disabled || state == ShapeBase::Destroyed )
//		{
//			mCurrTrackingAbility = 0; 
//			bounce = false;
//		}
//	}
//
//	if(bounce)
//		mCurrPosition = this->bounce(rInfo,vec);
//
//	setMaskBits(MovementMask);
//
//	//
//	// eyecandy stuff:
//	//
//
//	if(isClientObject() && obj && obj->getType() & ProjectileObjectType)
//	{
//		ExplosionType expType;
//		if(deflected)
//			expType = HitDeflectorExplosion;
//		else
//			if(obj->getTeamId() != mTeamId)
//				expType = HitEnemyExplosion;
//			else
//				expType = HitTeammateExplosion;
//
//		this->createExplosion(rInfo.point,rInfo.normal,expType);
//	}
//	else if(isServerObject())
//	{
//		ExplosionData* expData = NULL;
//		if(deflected)
//			expData = mDataBlock->hitDeflectorExplosion;
//		else
//			if(obj && obj->getTeamId() != mTeamId)
//				expData = mDataBlock->hitEnemyExplosion;
//			else
//				expData = mDataBlock->hitTeammateExplosion;
//
//		if(expData)
//		{
//			SimGroup* clientGroup = Sim::getClientGroup();
//			for(SimGroup::iterator itr = clientGroup->begin(); itr != clientGroup->end(); itr++)
//			{
//				NetConnection* nc = static_cast<NetConnection*>(*itr);
//				if(nc == conn) 
//					continue;
//
//				CreateExplosionEvent* event = new CreateExplosionEvent(expData,rInfo.point,rInfo.normal);
//				nc->postNetEvent(event);
//			}
//		}
//	}
//}

//--------------------------------------------------------------------------

ConsoleMethod(NortDisc, state, S32, 2, 2, "")
{
   return object->state();
}

ConsoleMethod(NortDisc, setDeflected, void, 3, 3, "")
{
	Point3F vec;
	dSscanf(argv[3], "%f %f %f", &vec.x, &vec.y, &vec.z);
	object->deflected(vec);
}

ConsoleMethod(NortDisc, stopAttacking, void, 2, 2, "")
{
	object->stopAttacking(ShapeBaseObjectType);
}
