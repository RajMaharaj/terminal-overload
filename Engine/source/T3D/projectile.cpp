// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "T3D/projectile.h"

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
#include "NOTC/fx/multiNodeLaserBeam.h"

const char* ProjectileData::sEmitterNode[ProjectileData::MaxEmitterNodes] =
{
   "LaserTrail0Node",
   "LaserTrail1Node",
   "LaserTrail2Node",
};

//----------------------------------------------------------------------------


IMPLEMENT_CO_DATABLOCK_V1(ProjectileData);

ConsoleDocClass( ProjectileData,
   "@brief Stores properties for an individual projectile type.\n"

   "@tsexample\n"
		"datablock ProjectileData(GrenadeLauncherProjectile)\n"
		"{\n"
		  " projectileShapeName = \"art/shapes/weapons/SwarmGun/rocket.dts\";\n"
		   "directDamage = 30;\n"
		   "radiusDamage = 30;\n"
		   "damageRadius = 5;\n"
		   "areaImpulse = 2000;\n"

		   "explosion = GrenadeLauncherExplosion;\n"
		   "waterExplosion = GrenadeLauncherWaterExplosion;\n"

		   "decal = ScorchRXDecal;\n"
		   "splash = GrenadeSplash;\n"

		   "particleEmitter = GrenadeProjSmokeTrailEmitter;\n"
		   "particleWaterEmitter = GrenadeTrailWaterEmitter;\n"

		   "muzzleVelocity = 30;\n"
		   "velInheritFactor = 0.3;\n"

		   "armingDelay = 2000;\n"
		   "lifetime = 10000;\n"
		   "fadeDelay = 4500;\n"

		   "bounceElasticity = 0.4;\n"
		   "bounceFriction = 0.3;\n"
		   "isBallistic = true;\n"
		   "gravityMod = 0.9;\n"

		   "lightDesc = GrenadeLauncherLightDesc;\n"

		   "damageType = \"GrenadeDamage\";\n"
		"};\n"
   "@endtsexample\n"

   "@ingroup gameObjects\n"
);

IMPLEMENT_CO_NETOBJECT_V1(Projectile);

ConsoleDocClass( Projectile,
   "@brief Base projectile class. Uses the ProjectileData class for properties of individual projectiles.\n"
   "@ingroup gameObjects\n"
);

IMPLEMENT_CALLBACK( ProjectileData, onExplode, void, ( Projectile* proj, Point3F pos, F32 fade ), 
                   ( proj, pos, fade ),
				   "@brief Called when a projectile explodes.\n\n"
                   "This function is only called on server objects.\n"
                   "@param proj The exploding projectile.\n"
				   "@param pos The position of the explosion.\n"
				   "@param fade The current fadeValue of the projectile, affects its visibility.\n\n"
				   "@see Projectile\n"
				  );

IMPLEMENT_CALLBACK( ProjectileData, onCollision, void, ( Projectile* proj, SceneObject* col, F32 fade, Point3F pos, Point3F normal ),
                   ( proj, col, fade, pos, normal ),
				   "@brief Called when a projectile collides with another object.\n\n"
                   "This function is only called on server objects."
				   "@param proj The projectile colliding with SceneObject col.\n"
				   "@param col The SceneObject hit by the projectile.\n"
				   "@param fade The current fadeValue of the projectile, affects its visibility.\n"
				   "@param pos The position of the collision.\n"
                   "@param normal The normal of the collision.\n"
				   "@see Projectile\n"
				  );

IMPLEMENT_CALLBACK( ProjectileData, onTargetLost, void, ( Projectile* proj ),
                   ( proj ),
				   "@brief Called when a projectile misses its target.\n\n"
                   "This function is only called on server objects."
				   "@param proj The projectile.\n"
				   "@see Projectile\n"
				  );

const U32 Projectile::csmStaticCollisionMask =  TerrainObjectType | StaticShapeObjectType;

const U32 Projectile::csmDynamicCollisionMask = PlayerObjectType | VehicleObjectType;

const U32 Projectile::csmDamageableMask = Projectile::csmDynamicCollisionMask;

U32 Projectile::smProjectileWarpTicks = 5;


//--------------------------------------------------------------------------
//
ProjectileData::ProjectileData()
{
   projectileShapeName = NULL;
   hideShapeWhileOverlappingMuzzlePoint = false;

   for(S32 i=0; i < MaxEmitterNodes; i++)
      emitterNode[i] = -1;

   sound = NULL;

   explodesNearEnemies = false;
   explodesNearEnemiesRadius = 10;
   explodesNearEnemiesMask = ShapeBaseObjectType;
   nearEnemyExplosion = NULL;	
   nearEnemyExplosionId = 0;

   missEnemyEffect = NULL;	
   missEnemyEffectId = 0;
   missEnemyEffectRadius = 0;

   for(U32 i = 0; i < NumImpactExplosions; i++)
   {
      impactExplosion[i] = NULL;
      impactExplosionId[i] = 0;
      impactExplosionTypeMask[i] = 0xFFFFFFFF;
   }

   for(U32 i = 0; i < NumBounceEffects; i++)
   {
      bounceEffect[i] = NULL;
      bounceEffectId[i] = 0;
      bounceEffectTypeMask[i] = 0xFFFFFFFF;
   }

   dMemset( laserTrail, 0, sizeof( laserTrail ) );
   dMemset( laserTrailId, 0, sizeof( laserTrailId ) );
   dMemset( laserTrailFlags, 0, sizeof( laserTrailFlags ) );

   explosion = NULL;
   explosionId = 0;

   waterExplosion = NULL;
   waterExplosionId = 0;

   //hasLight = false;
   //lightRadius = 1;
   //lightColor.set(1, 1, 1);
   lightDesc = NULL;

   faceViewer = false;
   scale.set( 1.0f, 1.0f, 1.0f );

   collisionMask = 0xFF;

	maxTrackingAbility = 0;
	trackingAgility = 1;

   isBallistic = false;

	velInheritFactor = 1.0f;
	muzzleVelocity = 50;
   impactForce = 0.0f;

	armingDelay = 0;
   fadeDelay = 20000 / 32;
   lifetime = 20000 / 32;

   activateSeq = -1;
   maintainSeq = -1;

   gravityMod = 1.0;
   bounceElasticity = 0.999f;
   bounceFriction = 0.3f;

   particleEmitter = NULL;
   particleEmitterId = 0;

   particleWaterEmitter = NULL;
   particleWaterEmitterId = 0;

   splash = NULL;
   splashId = 0;

   decal = NULL;
   decalId = 0;

   bounceDecal = NULL;
   bounceDecalId = 0;

   lightDesc = NULL;
   lightDescId = 0;
}

//--------------------------------------------------------------------------

void ProjectileData::initPersistFields()
{
	addNamedField(maxTrackingAbility, TypeS32, ProjectileData);
	addNamedField(trackingAgility, TypeS32, ProjectileData);

   addField("particleEmitter", TYPEID< ParticleEmitterData >(), Offset(particleEmitter, ProjectileData),
      "@brief Particle emitter datablock used to generate particles while the projectile is outside of water.\n\n"
      "@note If datablocks are defined for both particleEmitter and particleWaterEmitter, both effects will play "
      "as the projectile enters or leaves water.\n\n"
      "@see particleWaterEmitter\n");
   addField("particleWaterEmitter", TYPEID< ParticleEmitterData >(), Offset(particleWaterEmitter, ProjectileData),
      "@brief Particle emitter datablock used to generate particles while the projectile is submerged in water.\n\n"
      "@note If datablocks are defined for both particleWaterEmitter and particleEmitter , both effects will play "
      "as the projectile enters or leaves water.\n\n"
      "@see particleEmitter\n");

   addField("projectileShapeName", TypeShapeFilename, Offset(projectileShapeName, ProjectileData),
      "@brief File path to the model of the projectile.\n\n");
   addField("hideShapeWhileOverlappingMuzzlePoint", TypeBool, Offset(hideShapeWhileOverlappingMuzzlePoint, ProjectileData),
      "@brief Don't render projectile shape while its muzzle point is still within its bounding box.\n\n");

   addField("scale", TypePoint3F, Offset(scale, ProjectileData),
      "@brief Scale to apply to the projectile's size.\n\n"
      "@note This is applied after SceneObject::scale\n");

   addField("collisionMask", TypeS8, Offset(collisionMask, ProjectileData),
      "@brief What collision mask should trigger collisions?\n\n");

   addField("sound", TypeSFXTrackName, Offset(sound, ProjectileData),
      "@brief SFXTrack datablock used to play sounds while in flight.\n\n");

   addField("explodesNearEnemies", TypeBool, Offset(explodesNearEnemies, ProjectileData),
      "@brief Whether the projectile explodes near enemies.\n\n");
   addField("explodesNearEnemiesRadius", TypeS32, Offset(explodesNearEnemiesRadius, ProjectileData),
      "@brief How close the projectile has to be to an enemy to explode.\n\n");
   addField("explodesNearEnemiesMask", TypeS32, Offset(explodesNearEnemiesMask, ProjectileData),
      "@brief Projectile will only explode near enemies with this typemask.\n\n");
   addField("nearEnemyExplosion", TYPEID< ExplosionData >(), Offset(nearEnemyExplosion, ProjectileData),
      "@brief Explosion datablock used when the projectile explodes near an enemy.\n\n");

   addField("missEnemyEffect", TYPEID< ExplosionData >(), Offset(missEnemyEffect, ProjectileData),
      "@brief Explosion datablock used when the projectile misses an enemy.\n\n");
   addField("missEnemyEffectRadius", TypeS32, Offset(missEnemyEffectRadius, ProjectileData),
      "@brief How close the projectile has to be to an enemy to trigger the missedEnemyEffect.\n\n");

   addField("impactExplosion", TYPEID< ExplosionData >(), Offset(impactExplosion, ProjectileData), NumImpactExplosions,
      "@brief Explosion datablock used when the projectile impacts.\n\n");
   addField("impactExplosionTypeMask", TypeS32, Offset(impactExplosionTypeMask, ProjectileData), NumImpactExplosions,
      "@brief Use corresponding impact explosion when colliding with objects of this type.\n\n");

   addField("bounceEffect", TYPEID< ExplosionData >(), Offset(bounceEffect, ProjectileData), NumBounceEffects,
      "@brief Explosion datablock used when the projectile bounces.\n\n");
   addField("bounceEffectTypeMask", TypeS32, Offset(bounceEffectTypeMask, ProjectileData), NumBounceEffects,
      "@brief Use corresponding bounce effect when colliding with objects of this type.\n\n");

   addField("laserTrail", TYPEID< MultiNodeLaserBeamData >(), Offset(laserTrail, ProjectileData), NumLaserTrails,
      "@brief MultiNodeLaserBeam datablocks used for projectile trails.\n\n");
   addField("laserTrailFlags", TypeS32, Offset(laserTrailFlags, ProjectileData), NumLaserTrails,
      "@brief Flags for the corresponding laser trail.\n\n");

   addField("explosion", TYPEID< ExplosionData >(), Offset(explosion, ProjectileData),
      "@brief Explosion datablock used when the projectile explodes outside of water.\n\n");
   addField("waterExplosion", TYPEID< ExplosionData >(), Offset(waterExplosion, ProjectileData),
      "@brief Explosion datablock used when the projectile explodes underwater.\n\n");

   addField("splash", TYPEID< SplashData >(), Offset(splash, ProjectileData),
      "@brief Splash datablock used to create splash effects as the projectile enters or leaves water\n\n");

   addField("decal", TYPEID< DecalData >(), Offset(decal, ProjectileData),
      "@brief Decal datablock used for decals placed at projectile explosion points.\n\n");

   addField("bounceDecal", TYPEID< DecalData >(), Offset(bounceDecal, ProjectileData),
      "@brief Decal datablock used for decals placed at bounce points.\n\n");

   addField("lightDesc", TYPEID< LightDescription >(), Offset(lightDesc, ProjectileData),
      "@brief LightDescription datablock used for lights attached to the projectile.\n\n");

   addField("isBallistic", TypeBool, Offset(isBallistic, ProjectileData),
      "@brief Detetmines if the projectile should be affected by gravity and whether or not "
      "it bounces before exploding.\n\n");

   addField("velInheritFactor", TypeF32, Offset(velInheritFactor, ProjectileData),
      "@brief Amount of velocity the projectile recieves from the source that created it.\n\n"
      "Use an amount between 0 and 1 for the best effect. "
      "This value is never modified by the engine.\n"
      "@note This value by default is not transmitted between the server and the client.");
   addField("muzzleVelocity", TypeF32, Offset(muzzleVelocity, ProjectileData),
      "@brief Amount of velocity the projectile recieves from the \"muzzle\" of the gun.\n\n"
      "Used with velInheritFactor to determine the initial velocity of the projectile. "
      "This value is never modified by the engine.\n\n"
      "@note This value by default is not transmitted between the server and the client.\n\n"
      "@see velInheritFactor");
   
   addField("impactForce", TypeF32, Offset(impactForce, ProjectileData));

   addProtectedField("lifetime", TypeS32, Offset(lifetime, ProjectileData), &setLifetime, &getScaledValue, 
      "@brief Amount of time, in milliseconds, before the projectile is removed from the simulation.\n\n"
      "Used with fadeDelay to determine the transparency of the projectile at a given time. "
      "A projectile may exist up to a maximum of 131040ms (or 4095 ticks) as defined by Projectile::MaxLivingTicks in the source code."
      "@see fadeDelay");

   addProtectedField("armingDelay", TypeS32, Offset(armingDelay, ProjectileData), &setArmingDelay, &getScaledValue, 
      "@brief Amount of time, in milliseconds, before the projectile will cause damage or explode on impact.\n\n"
      "This value must be equal to or less than the projectile's lifetime.\n\n"
      "@see lifetime");
   addProtectedField("fadeDelay", TypeS32, Offset(fadeDelay, ProjectileData), &setFadeDelay, &getScaledValue,
      "@brief Amount of time, in milliseconds, before the projectile begins to fade out.\n\n"
      "This value must be smaller than the projectile's lifetime to have an affect.");

   addField("bounceElasticity", TypeF32, Offset(bounceElasticity, ProjectileData), 
      "@brief Influences post-bounce velocity of a projectile that does not explode on contact.\n\n"
      "Scales the velocity from a bounce after friction is taken into account. "
      "A value of 1.0 will leave it's velocity unchanged while values greater than 1.0 will increase it.\n");
   addField("bounceFriction", TypeF32, Offset(bounceFriction, ProjectileData),
      "@brief Factor to reduce post-bounce velocity of a projectile that does not explode on contact.\n\n"
      "Reduces bounce velocity by this factor and a multiple of the tangent to impact. "
      "Used to simulate surface friction.\n");
   addField("gravityMod", TypeF32, Offset(gravityMod, ProjectileData ),
      "@brief Scales the influence of gravity on the projectile.\n\n"
      "The larger this value is, the more that gravity will affect the projectile. "
      "A value of 1.0 will assume \"normal\" influence upon it.\n"
      "The magnitude of gravity is assumed to be 9.81 m/s/s\n\n"
      "@note ProjectileData::isBallistic must be true for this to have any affect.");

   Parent::initPersistFields();
}


//--------------------------------------------------------------------------
bool ProjectileData::onAdd()
{
   if(!Parent::onAdd())
      return false;

   return true;
}


bool ProjectileData::preload(bool server, String &errorStr)
{
   if (Parent::preload(server, errorStr) == false)
      return false;
      
   if( !server )
   {
      if (!particleEmitter && particleEmitterId != 0)
         if (Sim::findObject(particleEmitterId, particleEmitter) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(particleEmitter): %d", particleEmitterId);

      if (!particleWaterEmitter && particleWaterEmitterId != 0)
         if (Sim::findObject(particleWaterEmitterId, particleWaterEmitter) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(particleWaterEmitter): %d", particleWaterEmitterId);

      if (!nearEnemyExplosion && nearEnemyExplosionId != 0)
         if (Sim::findObject(nearEnemyExplosionId, nearEnemyExplosion) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(nearEnemyExplosion): %d", nearEnemyExplosionId);

      if (!missEnemyEffect && missEnemyEffectId != 0)
         if (Sim::findObject(missEnemyEffectId, missEnemyEffect) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(missEnemyEffect): %d", missEnemyEffectId);

      for(U32 i = 0; i < NumImpactExplosions; i++)
      {
         if (!impactExplosion[i] && impactExplosionId[i] != 0)
            if (Sim::findObject(impactExplosionId[i], impactExplosion[i]) == false)
               Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(impactExplosion[%i]): %d", i, impactExplosionId[i]);
      }

      for(U32 i = 0; i < NumBounceEffects; i++)
      {
         if (!bounceEffect[i] && bounceEffectId[i] != 0)
            if (Sim::findObject(bounceEffectId[i], bounceEffect[i]) == false)
               Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(bounceEffect[%i]): %d", i, bounceEffectId[i]);
      }

      for(U32 i = 0; i < NumLaserTrails; i++)
      {
         if (!laserTrail[i] && laserTrailId[i] != 0)
            if (Sim::findObject(laserTrailId[i], laserTrail[i]) == false)
               Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(laserTrail[%i]): %d", i, laserTrailId[i]);
      }

      if (!explosion && explosionId != 0)
         if (Sim::findObject(explosionId, explosion) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(explosion): %d", explosionId);

      if (!waterExplosion && waterExplosionId != 0)
         if (Sim::findObject(waterExplosionId, waterExplosion) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(waterExplosion): %d", waterExplosionId);

      if (!splash && splashId != 0)
         if (Sim::findObject(splashId, splash) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(splash): %d", splashId);

      if (!decal && decalId != 0)
         if (Sim::findObject(decalId, decal) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(decal): %d", decalId);

      if (!bounceDecal && bounceDecalId != 0)
         if (Sim::findObject(bounceDecalId, bounceDecal) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockId(bounceDecal): %d", bounceDecalId);

      String sfxErrorStr;
      if( !sfxResolve( &sound, sfxErrorStr ) )
         Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet: %s", sfxErrorStr.c_str());

      if (!lightDesc && lightDescId != 0)
         if (Sim::findObject(lightDescId, lightDesc) == false)
            Con::errorf(ConsoleLogEntry::General, "ProjectileData::preload: Invalid packet, bad datablockid(lightDesc): %d", lightDescId);   
   }

   if (projectileShapeName && projectileShapeName[0] != '\0')
   {
      projectileShape = ResourceManager::get().load(projectileShapeName);
      if (bool(projectileShape) == false)
      {
         errorStr = String::ToString("ProjectileData::load: Couldn't load shape \"%s\"", projectileShapeName);
         return false;
      }
      activateSeq = projectileShape->findSequence("activate");
      maintainSeq = projectileShape->findSequence("maintain");

      // Resolve emitter nodes
      for(S32 i=0; i<MaxEmitterNodes; i++)
         emitterNode[i] = projectileShape->findNode(sEmitterNode[i]);
   }

   if (bool(projectileShape)) // create an instance to preload shape data
   {
      TSShapeInstance* pDummy = new TSShapeInstance(projectileShape, !server);
      delete pDummy;
   }

   return true;
}

//--------------------------------------------------------------------------
void ProjectileData::packData(BitStream* stream)
{
   Parent::packData(stream);

	stream->writeInt(maxTrackingAbility,32);
	stream->writeInt(trackingAgility,32);

   stream->writeString(projectileShapeName);
   stream->writeFlag(hideShapeWhileOverlappingMuzzlePoint);
   stream->writeFlag(faceViewer);
   if(stream->writeFlag(scale.x != 1 || scale.y != 1 || scale.z != 1))
   {
      stream->write(scale.x);
      stream->write(scale.y);
      stream->write(scale.z);
   }

   stream->write(collisionMask);

   if(stream->writeFlag(explodesNearEnemies))
   {
      stream->writeInt(explodesNearEnemiesRadius, 32);
      stream->writeInt(explodesNearEnemiesMask, 32);
   }

   if (stream->writeFlag(particleEmitter != NULL))
      stream->writeRangedU32(particleEmitter->getId(), DataBlockObjectIdFirst,
                                                   DataBlockObjectIdLast);

   if (stream->writeFlag(particleWaterEmitter != NULL))
      stream->writeRangedU32(particleWaterEmitter->getId(), DataBlockObjectIdFirst,
                                                   DataBlockObjectIdLast);

   if (stream->writeFlag(nearEnemyExplosion != NULL))
      stream->writeRangedU32(nearEnemyExplosion->getId(), DataBlockObjectIdFirst,
                                                   DataBlockObjectIdLast);

   if (stream->writeFlag(missEnemyEffect != NULL))
   {
      stream->writeInt(missEnemyEffectRadius, 10);
      stream->writeRangedU32(missEnemyEffect->getId(), DataBlockObjectIdFirst,
                                                 DataBlockObjectIdLast);
   }

   for(U32 i = 0; i < NumImpactExplosions; i++)
   {
      if(stream->writeFlag(impactExplosion[i] != NULL))
      {
         stream->writeRangedU32(impactExplosion[i]->getId(), DataBlockObjectIdFirst,
                                                    DataBlockObjectIdLast);
         stream->write(impactExplosionTypeMask[i]);
      }
   }

   for(U32 i = 0; i < NumBounceEffects; i++)
   {
      if(stream->writeFlag(bounceEffect[i] != NULL))
      {
         stream->writeRangedU32(bounceEffect[i]->getId(), DataBlockObjectIdFirst,
                                                    DataBlockObjectIdLast);
         stream->write(bounceEffectTypeMask[i]);
      }
   }

   for(U32 i = 0; i < NumLaserTrails; i++)
   {
      if(stream->writeFlag(laserTrail[i] != NULL))
      {
         stream->writeRangedU32(laserTrail[i]->getId(), DataBlockObjectIdFirst,
                                                    DataBlockObjectIdLast);
         stream->write(laserTrailFlags[i]);
      }
   }

   if (stream->writeFlag(explosion != NULL))
      stream->writeRangedU32(explosion->getId(), DataBlockObjectIdFirst,
                                                 DataBlockObjectIdLast);

   if (stream->writeFlag(waterExplosion != NULL))
      stream->writeRangedU32(waterExplosion->getId(), DataBlockObjectIdFirst,
                                                      DataBlockObjectIdLast);

   if (stream->writeFlag(splash != NULL))
      stream->writeRangedU32(splash->getId(), DataBlockObjectIdFirst,
                                              DataBlockObjectIdLast);

   if (stream->writeFlag(decal != NULL))
      stream->writeRangedU32(decal->getId(), DataBlockObjectIdFirst,
                                              DataBlockObjectIdLast);

   if (stream->writeFlag(bounceDecal != NULL))
      stream->writeRangedU32(bounceDecal->getId(), DataBlockObjectIdFirst,
                                              DataBlockObjectIdLast);

   sfxWrite( stream, sound );

   if ( stream->writeFlag(lightDesc != NULL))
      stream->writeRangedU32(lightDesc->getId(), DataBlockObjectIdFirst,
                                                 DataBlockObjectIdLast);

	stream->write(velInheritFactor);
	stream->write(muzzleVelocity);
   stream->write(impactForce);
   
//    stream->writeRangedU32(lifetime, 0, Projectile::MaxLivingTicks);
//    stream->writeRangedU32(armingDelay, 0, Projectile::MaxLivingTicks);
//    stream->writeRangedU32(fadeDelay, 0, Projectile::MaxLivingTicks);

   // [tom, 3/21/2007] Changing these to write all 32 bits as the previous
   // code limited these to a max value of 4095.
   stream->write(lifetime);
   stream->write(armingDelay);
   stream->write(fadeDelay);

   if(stream->writeFlag(isBallistic))
   {
      stream->write(gravityMod);
      stream->write(bounceElasticity);
      stream->write(bounceFriction);
   }

}

void ProjectileData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

	maxTrackingAbility = stream->readInt(32);
	trackingAgility = stream->readInt(32);

   projectileShapeName = stream->readSTString();
   hideShapeWhileOverlappingMuzzlePoint = stream->readFlag();


   faceViewer = stream->readFlag();
   if(stream->readFlag())
   {
      stream->read(&scale.x);
      stream->read(&scale.y);
      stream->read(&scale.z);
   }
   else
      scale.set(1,1,1);

   stream->read(&collisionMask);

   explodesNearEnemies = stream->readFlag();
   if(explodesNearEnemies)
   {
      explodesNearEnemiesRadius = stream->readInt(32);
      explodesNearEnemiesMask = stream->readInt(32);
   }

   if (stream->readFlag())
      particleEmitterId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      particleWaterEmitterId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      nearEnemyExplosionId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if(stream->readFlag())
   {
      missEnemyEffectRadius = stream->readInt(10); 
      missEnemyEffectId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   for(U32 i = 0; i < NumImpactExplosions; i++)
   {
      if(stream->readFlag())
      {
         impactExplosionId[i] = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
         stream->read(&impactExplosionTypeMask[i]);
      }
   }

   for(U32 i = 0; i < NumBounceEffects; i++)
   {
      if(stream->readFlag())
      {
         bounceEffectId[i] = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
         stream->read(&bounceEffectTypeMask[i]);
      }
   }

   for(U32 i = 0; i < NumLaserTrails; i++)
   {
      if(stream->readFlag())
      {
         laserTrailId[i] = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
         stream->read(&laserTrailFlags[i]);
      }
   }

   if (stream->readFlag())
      explosionId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      waterExplosionId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   if (stream->readFlag())
      splashId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      decalId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

   if (stream->readFlag())
      bounceDecalId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   sfxRead( stream, &sound );

   if (stream->readFlag())
      lightDescId = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   
   // [tom, 3/21/2007] See comment in packData()
//    lifetime = stream->readRangedU32(0, Projectile::MaxLivingTicks);
//    armingDelay = stream->readRangedU32(0, Projectile::MaxLivingTicks);
//    fadeDelay = stream->readRangedU32(0, Projectile::MaxLivingTicks);

	stream->read(&velInheritFactor);
	stream->read(&muzzleVelocity);
   stream->read(&impactForce);

   stream->read(&lifetime);
   stream->read(&armingDelay);
   stream->read(&fadeDelay);

   isBallistic = stream->readFlag();
   if(isBallistic)
   {
      stream->read(&gravityMod);
      stream->read(&bounceElasticity);
      stream->read(&bounceFriction);
   }
}

bool ProjectileData::setLifetime( void *obj, const char *index, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);
   
   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->lifetime = value;

   return false;
}

bool ProjectileData::setArmingDelay( void *obj, const char *index, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);

   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->armingDelay = value;

   return false;
}

bool ProjectileData::setFadeDelay( void *obj, const char *index, const char *data )
{
	S32 value = dAtoi(data);
   value = scaleValue(value);

   ProjectileData *object = static_cast<ProjectileData*>(obj);
   object->fadeDelay = value;

   return false;
}

const char *ProjectileData::getScaledValue( void *obj, const char *data)
{

	S32 value = dAtoi(data);
   value = scaleValue(value, false);

   String stringData = String::ToString(value);
   char *strBuffer = Con::getReturnBuffer(stringData.size());
   dMemcpy( strBuffer, stringData, stringData.size() );

   return strBuffer;
}

S32 ProjectileData::scaleValue( S32 value, bool down )
{
   S32 minV = 0;
   S32 maxV = Projectile::MaxLivingTicks;
   
   // scale down to ticks before we validate
   if( down )
      value /= TickMs;
   
   if(value < minV || value > maxV)
	{
      Con::errorf("ProjectileData::scaleValue(S32 value = %d, bool down = %b) - Scaled value must be between %d and %d", value, down, minV, maxV);
		if(value < minV)
			value = minV;
		else if(value > maxV)
			value = maxV;
	}

   // scale up from ticks after we validate
   if( !down )
      value *= TickMs;

   return value;
}

//--------------------------------------------------------------------------
//--------------------------------------
//
Projectile::Projectile()
 : mPhysicsWorld( NULL ),
   mDataBlock( NULL ),
   mCurrPosition( 0, 0, 0 ),
   mCurrVelocity( 0, 0, 1 ),
   mExplode(false),
   mCurrTrackingAbility(0),
   mSourceObjectId( -1 ),
   mSourceObjectSlot( -1 ),
   mCurrTick( 0 ),
   mParticleEmitter( NULL ),
   mParticleWaterEmitter( NULL ),
   mSound( NULL ),
   mProjectileShape( NULL ),
   mActivateThread( NULL ),
   mMaintainThread( NULL ),
   mEmissionCount( 0 ),
   mHasExploded( false ),
   mFadeValue( 1.0f )
{
   // Todo: ScopeAlways?
   mNetFlags.set(Ghostable);
   mTypeMask |= ProjectileObjectType | LightObjectType | DynamicShapeObjectType;

   mLight = LightManager::createLightInfo();
   mLight->setType( LightInfo::Point );   

   mLightState.clear();
   mLightState.setLightInfo( mLight );

   dMemset( mLaserTrailList, 0, sizeof(mLaserTrailList) );

	mTargetMode = None;
	mTarget = NULL;
	mTargetPointOffset.set(0,0,0);
	mTargetPosition.set(0,0,0);
}

Projectile::~Projectile()
{
   SAFE_DELETE(mLight);

   delete mProjectileShape;
   mProjectileShape = NULL;
}

//--------------------------------------------------------------------------
void Projectile::initPersistFields()
{
   addGroup("Physics");

   addProtectedField("initialPosition",  TypePoint3F, Offset(mInitialPosition, Projectile), &_setInitialPosition, &defaultProtectedGetFn,
      "@brief Starting position for the projectile.\n\n");
   //addField("initialPosition",  TypePoint3F, Offset(mCurrPosition, Projectile),
   //   "@brief Starting position for the projectile.\n\n");
   addProtectedField("initialVelocity", TypePoint3F, Offset(mInitialVelocity, Projectile), &_setInitialVelocity, &defaultProtectedGetFn,
      "@brief Starting velocity for the projectile.\n\n");
   //addField("initialVelocity", TypePoint3F, Offset(mCurrVelocity, Projectile),
   //   "@brief Starting velocity for the projectile.\n\n");

   endGroup("Physics");

   addGroup("Source");

   addField("sourceObject",     TypeS32,     Offset(mSourceObjectId, Projectile),
      "@brief ID number of the object that fired the projectile.\n\n"
      "@note If the projectile was fired by a WeaponImage, sourceObject will be "
      "the object that owns the WeaponImage. This is usually the player.");
   addField("sourceSlot",       TypeS32,     Offset(mSourceObjectSlot, Projectile),
      "@brief The sourceObject's weapon slot that the projectile originates from.\n\n");

   endGroup("Source");


   Parent::initPersistFields();
}

bool Projectile::_setInitialPosition( void *object, const char *index, const char *data )
{
   Projectile* p = static_cast<Projectile*>( object );
   if ( p )
   {
	   Point3F pos;

	   S32 count = dSscanf( data, "%f %f %f", 
		   &pos.x, &pos.y, &pos.z);
   	
	   if ( (count != 3) )
      {
         Con::printf("Projectile: Failed to parse initial position \"px py pz\" from '%s'", data);
         return false;
      }

      p->setInitialPosition( pos );
   }
   return false;
}

void Projectile::setInitialPosition( const Point3F& pos )
{
   mInitialPosition = pos;
   mCurrPosition = pos;
}

bool Projectile::_setInitialVelocity( void *object, const char *index, const char *data )
{
   Projectile* p = static_cast<Projectile*>( object );
   if ( p )
   {
	   Point3F vel;

	   S32 count = dSscanf( data, "%f %f %f", 
		   &vel.x, &vel.y, &vel.z);
   	
	   if ( (count != 3) )
      {
         Con::printf("Projectile: Failed to parse initial velocity \"vx vy vz\" from '%s'", data);
         return false;
      }

      p->setInitialVelocity( vel );
   }
   return false;
}

void Projectile::setInitialVelocity( const Point3F& vel )
{
   mInitialVelocity = vel;
   mCurrVelocity = vel;
}

//--------------------------------------------------------------------------

bool Projectile::calculateImpact(float,
                                 Point3F& pointOfImpact,
                                 float&   impactTime)
{
   Con::warnf(ConsoleLogEntry::General, "Projectile::calculateImpact: Should never be called");

   impactTime = 0;
   pointOfImpact.set(0, 0, 0);
   return false;
}


//--------------------------------------------------------------------------
F32 Projectile::getUpdatePriority(CameraScopeQuery *camInfo, U32 updateMask, S32 updateSkips)
{
   F32 ret = Parent::getUpdatePriority(camInfo, updateMask, updateSkips);
   // if the camera "owns" this object, it should have a slightly higher priority
   if(mSourceObject == camInfo->camera)
      return ret + 0.2;
   return ret;
}

bool Projectile::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if( !mDataBlock )
   {
      Con::errorf("Projectile::onAdd - Fail - Not datablock");
      return false;
   }

   if (isServerObject())
   {
		mInitialPosition = mCurrPosition;
		mInitialVelocity = mCurrVelocity;

		// no source object?
		if(mSourceObject == NULL)
		{
			ShapeBase* ptr;
			if (Sim::findObject(mSourceObjectId, ptr))
			{
				mSourceObject = ptr;

				// Since we later do processAfter( mSourceObject ) we must clearProcessAfter
				// if it is deleted. SceneObject already handles this in onDeleteNotify so
				// all we need to do is register for the notification.
				deleteNotify( ptr );
			}
			else
			{
				if (mSourceObjectId != -1)
					Con::errorf(ConsoleLogEntry::General, "Projectile::onAdd: mSourceObjectId is invalid");
				mSourceObject = NULL;
			}
		}
		else
		{			
			mSourceObjectId = mSourceObject->getId();
		}

      // If we're on the server, we need to inherit some of our parent's velocity
      //
      mCurrTick = 0;
   }
   else
   {
      if (bool(mDataBlock->projectileShape))
      {
         mProjectileShape = new TSShapeInstance(mDataBlock->projectileShape, isClientObject());

         if (mDataBlock->activateSeq != -1)
         {
            mActivateThread = mProjectileShape->addThread();
            mProjectileShape->setTimeScale(mActivateThread, 1);
            mProjectileShape->setSequence(mActivateThread, mDataBlock->activateSeq, 0);
         }
      }
      if (mDataBlock->particleEmitter != NULL)
      {
         ParticleEmitter* pEmitter = new ParticleEmitter;
         pEmitter->setPalette(this->getPalette());
         pEmitter->onNewDataBlock(mDataBlock->particleEmitter,false);
         if (pEmitter->registerObject() == false)
         {
            Con::warnf(ConsoleLogEntry::General, "Could not register particle emitter for particle of class: %s", mDataBlock->getName());
            delete pEmitter;
            pEmitter = NULL;
         }
         mParticleEmitter = pEmitter;
      }

      if (mDataBlock->particleWaterEmitter != NULL)
      {
         ParticleEmitter* pEmitter = new ParticleEmitter;
         pEmitter->setPalette(this->getPalette());
         pEmitter->onNewDataBlock(mDataBlock->particleWaterEmitter,false);
         if (pEmitter->registerObject() == false)
         {
            Con::warnf(ConsoleLogEntry::General, "Could not register particle emitter for particle of class: %s", mDataBlock->getName());
            delete pEmitter;
            pEmitter = NULL;
         }
         mParticleWaterEmitter = pEmitter;
      }
   }

	if(mSourceObject.isValid())
	{
		processAfter(mSourceObject);
		deleteNotify(mSourceObject);
	}

   // Setup our bounding box
   if (bool(mDataBlock->projectileShape) == true)
      mObjBox = mDataBlock->projectileShape->bounds;
   else
      mObjBox = Box3F(Point3F(0, 0, 0), Point3F(0, 0, 0));

   MatrixF initialTransform( true );
   initialTransform.setPosition( mCurrPosition );
   setTransform( initialTransform );   // calls resetWorldBox

   addToScene();

   if ( PHYSICSMGR )
      mPhysicsWorld = PHYSICSMGR->getWorld( isServerObject() ? "server" : "client" );

	this->scriptOnAdd();

   return true;
}


void Projectile::onRemove()
{
	this->scriptOnRemove();

   for( S32 i = 0; i < ProjectileData::NumLaserTrails; i++ )
   {
      if(mLaserTrailList[i])
      {
         mLaserTrailList[i]->deleteOnFadeout();
         mLaserTrailList[i] = NULL;
      }
   }

   if( !mParticleEmitter.isNull() )
   {
      mParticleEmitter->deleteWhenEmpty();
      mParticleEmitter = NULL;
   }

   if( !mParticleWaterEmitter.isNull() )
   {
      mParticleWaterEmitter->deleteWhenEmpty();
      mParticleWaterEmitter = NULL;
   }

   SFX_DELETE( mSound );

   removeFromScene();
   Parent::onRemove();
}

void Projectile::onDeleteNotify(SimObject* obj)
{
	Parent::onDeleteNotify(obj);

   if(obj == mTarget)
		mTarget = NULL;
}

bool Projectile::onNewDataBlock( GameBaseData *dptr, bool reload )
{
   mDataBlock = dynamic_cast<ProjectileData*>( dptr );
   if ( !mDataBlock || !Parent::onNewDataBlock( dptr, reload ) )
      return false;

   if ( isGhost() )
   {
      // Create the sound ahead of time.  This reduces runtime
      // costs and makes the system easier to understand.

      SFX_DELETE( mSound );

      if ( mDataBlock->sound )
         mSound = SFX->createSource( mDataBlock->sound );
   }

   return true;
}

void Projectile::submitLights( LightManager *lm, bool staticLighting )
{
   if ( staticLighting || mHasExploded || !mDataBlock->lightDesc )
      return;
   
   mDataBlock->lightDesc->submitLight( &mLightState, getRenderTransform(), lm, this );   
}

bool Projectile::pointInWater(const Point3F &point)
{   
   // This is pretty much a hack so we can use the existing ContainerQueryInfo
   // and findObject router.
   
   // We only care if we intersect with water at all 
   // so build a box at the point that has only 1 z extent.
   // And test if water coverage is anything other than zero.

   Box3F boundsBox( point, point );
   boundsBox.maxExtents.z += 1.0f;

   ContainerQueryInfo info;
   info.box = boundsBox;
   info.mass = 0.0f;
   
   // Find and retreive physics info from intersecting WaterObject(s)
   if(mContainer != NULL)
   {
      mContainer->findObjects( boundsBox, WaterObjectType, findRouter, &info );
   }
   else
   {
      // Handle special case where the projectile has exploded prior to having
      // called onAdd() on the client.  This occurs when the projectile on the
      // server is created and then explodes in the same network update tick.
      // On the client end in NetConnection::ghostReadPacket() the ghost is
      // created and then Projectile::unpackUpdate() is called prior to the
      // projectile being registered.  Within unpackUpdate() the explosion
      // is triggered, but without being registered onAdd() isn't called and
      // the container is not set.  As all we're doing is checking if the
      // given explosion point is within water, we should be able to use the
      // global container here.  We could likely always get away with this,
      // but using the actual defined container when possible is the right
      // thing to do.  DAW
      AssertFatal(isClientObject(), "Server projectile has not been properly added");
      gClientContainer.findObjects( boundsBox, WaterObjectType, findRouter, &info );
   }

   return ( info.waterCoverage > 0.0f );
}

//----------------------------------------------------------------------------

void Projectile::emitParticles(const Point3F& from, const Point3F& to, const Point3F& vel, const U32 ms)
{
   if ( mHasExploded )
      return;

   Point3F axis = -vel;

   if( axis.isZero() )
      axis.set( 0.0, 0.0, 1.0 );
   else
      axis.normalize();

   bool fromWater = pointInWater(from);
   bool toWater   = pointInWater(to);

   if (!fromWater && !toWater && bool(mParticleEmitter))                                        // not in water
      mParticleEmitter->emitParticles(from, to, axis, vel, ms);
   else if (fromWater && toWater && bool(mParticleWaterEmitter))                                // in water
      mParticleWaterEmitter->emitParticles(from, to, axis, vel, ms);
   else if (!fromWater && toWater && mDataBlock->splash)     // entering water
   {
      // cast the ray to get the surface point of the water
      RayInfo rInfo;
      if (gClientContainer.castRay(from, to, WaterObjectType, &rInfo))
      {
         MatrixF trans = getTransform();
         trans.setPosition(rInfo.point);

         Splash *splash = new Splash();
         splash->onNewDataBlock(mDataBlock->splash, false);
         splash->setTransform(trans);
         splash->setInitialState(trans.getPosition(), Point3F(0.0, 0.0, 1.0));
         if (!splash->registerObject())
         {
            delete splash;
            splash = NULL;
         }

         // create an emitter for the particles out of water and the particles in water
         if (mParticleEmitter)
            mParticleEmitter->emitParticles(from, rInfo.point, axis, vel, ms);

         if (mParticleWaterEmitter)
            mParticleWaterEmitter->emitParticles(rInfo.point, to, axis, vel, ms);
      }
   }
   else if (fromWater && !toWater && mDataBlock->splash)     // leaving water
   {
      // cast the ray in the opposite direction since that point is out of the water, otherwise
      //  we hit water immediately and wont get the appropriate surface point
      RayInfo rInfo;
      if (gClientContainer.castRay(to, from, WaterObjectType, &rInfo))
      {
         MatrixF trans = getTransform();
         trans.setPosition(rInfo.point);

         Splash *splash = new Splash();
         splash->onNewDataBlock(mDataBlock->splash,false);
         splash->setTransform(trans);
         splash->setInitialState(trans.getPosition(), Point3F(0.0, 0.0, 1.0));
         if (!splash->registerObject())
         {
            delete splash;
            splash = NULL;
         }

         // create an emitter for the particles out of water and the particles in water
         if (mParticleEmitter)
            mParticleEmitter->emitParticles(rInfo.point, to, axis, vel, ms);

         if (mParticleWaterEmitter)
            mParticleWaterEmitter->emitParticles(from, rInfo.point, axis, vel, ms);
      }
   }
}

void Projectile::explode()
{
   MatrixF xform(true);
   xform.setColumn(3, mCurrPosition);
   setTransform(xform);
   mCurrVelocity    = Point3F(0, 0, 0);
   Point3F normal = mCurrPosition;
   normal.normalize();
   this->explode(mCurrPosition, normal, 0);
}

void Projectile::explode( const Point3F &p, const Point3F &n, const U32 collideType )
{
   // Make sure we don't explode twice...
   if ( mHasExploded )
      return;

   mHasExploded = true;

   // Move the explosion point slightly off the surface to avoid problems with radius damage
   Point3F explodePos = p + n * 0.001f;

   if ( isServerObject() )
   {
      // Do what the server needs to do, damage the surrounding objects, etc.
      mExplosionPosition = explodePos;
      mExplosionNormal = n;
      mCollideHitType  = collideType;

	   mDataBlock->onExplode_callback( this, mExplosionPosition, mFadeValue );

      setMaskBits(ExplosionMask);

      // Just wait till the timeout to self delete. This 
      // gives server object time to get ghosted to the client.
   } 
   else 
   {
      // Client just plays the explosion at the right place...
      //       
      ExplosionData* pData = mDataBlock->explosion;
      Explosion* pExplosion = NULL;

      if(mDataBlock->waterExplosion && pointInWater(p))
      {
         pData = mDataBlock->waterExplosion;
      }
      else if(collideType == 0 && mDataBlock->nearEnemyExplosion)
      {
         pData = mDataBlock->nearEnemyExplosion;
      }
      else
      {
         for(U32 i = 0; i < ProjectileData::NumImpactExplosions; i++)
         {
            if(mDataBlock->impactExplosion[i] != NULL
            && (mDataBlock->impactExplosionTypeMask[i] & collideType))
            {
               pData = mDataBlock->impactExplosion[i];
               break;
            }
         }
      }
    
      if(pData)
      {
         pExplosion = new Explosion;
         pExplosion->setPalette(this->getPalette());
         pExplosion->onNewDataBlock(pData, false);
      }

      if(pExplosion)
      {
         MatrixF xform(true);
         xform.setPosition(explodePos);
         pExplosion->setTransform(xform);
         pExplosion->setInitialState(explodePos, n);
         pExplosion->setCollideType( collideType );
         if (pExplosion->registerObject() == false)
         {
            Con::errorf(ConsoleLogEntry::General, "Projectile(%s)::explode: couldn't register explosion",
                        mDataBlock->getName() );
            delete pExplosion;
            pExplosion = NULL;
         }
      }

      // Client (impact) decal.
      if ( mDataBlock->decal )     
      {
         DecalInstance* dinst = gDecalManager->addDecal( p, n, mRandF(0.0f, M_2PI_F), mDataBlock->decal );
         if(dinst)
            dinst->mPalette = this->getPalette();
      }

      if(mEmissionCount == 0)
         this->addLaserTrailNode(mInitialPosition, false);
      this->addLaserTrailNode(p, false);

      // Client object
      updateSound();
   }

   /*
   // Client and Server both should apply forces to PhysicsWorld objects
   // within the explosion. 
   if ( false && mPhysicsWorld )
   {
      F32 force = 200.0f;
      mPhysicsWorld->explosion( p, 15.0f, force );
   }
   */
}

void Projectile::setTarget(GameBase* target)
{
	// look for target...
	mTarget = target;
	if(mTarget)
	{
		mTargetMode = Object;
		deleteNotify(mTarget);
		mTargetPointOffset = mTarget->getPosition();
		if( mTarget->getTypeMask() & PlayerObjectType )
		{
			F32 rand = Platform::getRandom()*mTarget->getObjBox().len_z();
			if( rand < 1.5 ) rand = 1.5;
			mTargetPointOffset.z += rand;
		}
		mTargetPointOffset -= mTarget->getPosition();

		//mTargetClientControlled = mTarget->getControllingClient() ? true : false;
	}
	else
	{
		mTargetMode = None;
		//mTargetClientControlled = false;
	}
	setMaskBits(TargetMask|MovementMask);
}

void Projectile::setTargetPosition(const Point3F& pos)
{
	mTargetMode = Position;
	mTarget = NULL;
	mTargetPosition = pos;
	setMaskBits(TargetMask|MovementMask);
}

void Projectile::updateSound()
{
   if (!mDataBlock->sound)
      return;

   if ( mSound )
   {
      if ( mHasExploded )
         mSound->stop();
      else
      {
         if ( !mSound->isPlaying() )
            mSound->play();

         mSound->setVelocity( getVelocity() );
         mSound->setTransform( getRenderTransform() );
      }
   }
}

void Projectile::processTick( const Move *move )
{
   Parent::processTick( move );
   mCurrTick++;

   simulate( TickSec );
}

void Projectile::simulate( F32 dt )
{         
   if ( isServerObject() && mCurrTick >= mDataBlock->lifetime )
   {
      deleteObject();
      return;
   }
   
   if ( mHasExploded )
      return;

   // ... otherwise, we have to do some simulation work.
   RayInfo rInfo;
   Point3F oldPosition;
   Point3F newPosition;

   oldPosition = mCurrPosition;
   if ( mDataBlock->isBallistic )
      mCurrVelocity.z -= 9.81 * mDataBlock->gravityMod * dt;

   // target-tracking...
	if( mDataBlock->maxTrackingAbility > 0 && mTargetMode != None )
		updateTargetTracking();

   newPosition = oldPosition + mCurrVelocity * dt;

   // disable the source objects collision reponse for a short time while we
   // determine if the projectile is capable of moving from the old position
   // to the new position, otherwise we'll hit ourself
   bool disableSourceObjCollision = (mSourceObject.isValid() && mCurrTick <= SourceIdTimeoutTicks);
   if ( disableSourceObjCollision )
      mSourceObject->disableCollision();
   disableCollision();

   // Determine if the projectile is going to hit any object between the previous
   // position and the new position. This code is executed both on the server
   // and on the client (for prediction purposes). It is possible that the server
   // will have registered a collision while the client prediction has not. If this
   // happens the client will be corrected in the next packet update.

   // Raycast the abstract PhysicsWorld if a PhysicsPlugin exists.
   bool hit = false;

   if ( mPhysicsWorld )
      hit = mPhysicsWorld->castRay( oldPosition, newPosition, &rInfo, Point3F( newPosition - oldPosition) * mDataBlock->impactForce );            
   else 
      hit = getContainer()->castRay(oldPosition, newPosition, csmDynamicCollisionMask | csmStaticCollisionMask, mDataBlock->collisionMask, &rInfo);

   if ( hit )
   {
      // make sure the client knows to bounce
      if ( isServerObject() && ( rInfo.object->getTypeMask() & csmStaticCollisionMask ) == 0 )
         setMaskBits( BounceMask );

      MatrixF xform( true );
      xform.setColumn( 3, rInfo.point );
      setTransform( xform );
      mCurrPosition    = rInfo.point;

      // Get the object type before the onCollision call, in case
      // the object is destroyed.
      U32 objectType = rInfo.object->getTypeMask();

      // re-enable the collision response on the source object since
      // we need to process the onCollision and explode calls
      if ( disableSourceObjCollision )
         mSourceObject->enableCollision();

      // Ok, here is how this works:
      // onCollision is called to notify the server scripts that a collision has occurred, then
      // a call to explode is made to start the explosion process. The call to explode is made
      // twice, once on the server and once on the client.
      // The server process is responsible for two things:
      //    1) setting the ExplosionMask network bit to guarantee that the client calls explode
      //    2) initiate the explosion process on the server scripts
      // The client process is responsible for only one thing:
      //    1) drawing the appropriate explosion

      // It is possible that during the processTick the server may have decided that a hit
      // has occurred while the client prediction has decided that a hit has not occurred.
      // In this particular scenario the client will have failed to call onCollision and
      // explode during the processTick. However, the explode function will be called
      // during the next packet update, due to the ExplosionMask network bit being set.
      // onCollision will remain uncalled on the client however, therefore no client
      // specific code should be placed inside the function!
      onCollision( rInfo.point, rInfo.normal, rInfo.object );
      // Next order of business: do we explode on this hit?
      if ( mCurrTick > mDataBlock->armingDelay || mDataBlock->armingDelay == 0 )
      {
         mCurrVelocity    = Point3F::Zero;
         explode( rInfo.point, rInfo.normal, objectType );
      }

      if ( mDataBlock->isBallistic )
      {
         this->createBounceExplosion(rInfo);
         
         // Otherwise, this represents a bounce.  First, reflect our velocity
         //  around the normal...
         Point3F bounceVel = mCurrVelocity - rInfo.normal * (mDot( mCurrVelocity, rInfo.normal ) * 2.0);
         mCurrVelocity = bounceVel;

         // Add in surface friction...
         Point3F tangent = bounceVel - rInfo.normal * mDot(bounceVel, rInfo.normal);
         mCurrVelocity  -= tangent * mDataBlock->bounceFriction;

         // Now, take elasticity into account for modulating the speed of the grenade
         mCurrVelocity *= mDataBlock->bounceElasticity;

         // Set the new position to the impact and the bounce
         // will apply on the next frame.
         //F32 timeLeft = 1.0f - rInfo.t;
         newPosition = oldPosition = rInfo.point + rInfo.normal * 0.05f;
      }
      else
      {
         mCurrVelocity    = Point3F::Zero;
      }
   }
   else
   {
      // No collision
      this->missedEnemiesCheck(oldPosition, newPosition);
   }

   // re-enable the collision response on the source object now
   // that we are done processing the ballistic movement
   if ( disableSourceObjCollision )
      mSourceObject->enableCollision();
   enableCollision();

   if ( isClientObject() )
   {
      if(mEmissionCount == 0)
      {
         emitParticles(mInitialPosition, newPosition, mCurrVelocity, U32( dt * 1000.0f ) );
         this->addLaserTrailNode(mInitialPosition, false);
      }
      else
         emitParticles(mCurrPosition, newPosition, mCurrVelocity, U32( dt * 1000.0f ) );
      this->addLaserTrailNode(newPosition, true);
      mEmissionCount++;
      updateSound();
   }

   mCurrDeltaBase = newPosition;
   mCurrBackDelta = mCurrPosition - newPosition;
   mCurrPosition = newPosition;

   MatrixF xform( true );
   xform.setColumn( 3, mCurrPosition );
   setTransform( xform );
}

void Projectile::updateTargetTracking()
{
	Point3F targetPos, targetDir;
	F32 speed = mCurrVelocity.len();

	if(mTargetMode == Object)
		targetPos = mTarget->getPosition()+mTargetPointOffset;
	else if(mTargetMode == Position)
		targetPos = mTargetPosition;
	else
		return;
	F32 dist = (targetPos-mCurrPosition).len();

	// target moving? - intercept it!...
	if(mTargetMode == Object)
	{
		F32 tickDist = speed * (F32(TickMs) / 1000.0f);
		if( dist > tickDist && mTarget->getVelocity().len() > 0 )
		{
			Point3F vel;
			
			vel = mTarget->getVelocity();
			vel.normalize();
			vel *= 1-(1/dist);

			targetPos += vel; 
		}
	}

	targetDir = targetPos - mCurrPosition;
	targetDir.normalize();
	if( mCurrTrackingAbility < mDataBlock->maxTrackingAbility )
	{
		mCurrTrackingAbility += mDataBlock->trackingAgility;
		targetDir *= mCurrTrackingAbility;
	}
	else
	{
		targetDir *= mDataBlock->maxTrackingAbility;
	}
	targetDir += mCurrVelocity;
	targetDir.normalize();

	mCurrVelocity = targetDir;
	mCurrVelocity *= speed; 
}

void Projectile::addLaserTrailNode(const Point3F& pos, bool useEmitterNode)
{
   if(this->isServerObject())
      return;

   for(S32 i = 0; i < ProjectileData::NumLaserTrails; i++)
   {
      Point3F p = pos;
      if(useEmitterNode && mProjectileShape && mDataBlock->emitterNode[i] != -1)
      {
         MatrixF xform = this->getRenderTransform();
         xform.setPosition(pos);

         MatrixF mat;
         mat.mul(xform, mProjectileShape->mNodeTransforms[mDataBlock->emitterNode[i]]);
         mat.getColumn(3,&p);
      }

      if( mLaserTrailList[i] == NULL )
      {
         if( mDataBlock->laserTrail[i] )
         {
            mLaserTrailList[i] = new MultiNodeLaserBeam();
            mLaserTrailList[i]->setPalette(this->getPalette());
            mLaserTrailList[i]->onNewDataBlock(mDataBlock->laserTrail[i], false);
            if( !mLaserTrailList[i]->registerObject() )
            {
               Con::warnf( ConsoleLogEntry::General, "Could not register laserTrail %d for class: %s",i, mDataBlock->getName() );
               delete mLaserTrailList[i];
               mLaserTrailList[i] = NULL;
            }
            else
            {
               mLaserTrailList[i]->addNodes(p);
               mLaserTrailList[i]->setRender(true);
               mLaserTrailList[i]->fade();
            }
         }
      }
      else
      {
         mLaserTrailList[i]->addNodes(p);
      }
   }
}

void Projectile::interpolateLaserTrails(const Point3F& interpPos)
{
   for(S32 i = 0; i < ProjectileData::NumLaserTrails; i++)
   {
      if(!mLaserTrailList[i])
         continue;

      Point3F p = interpPos;
      if(mProjectileShape && mDataBlock->emitterNode[i] != -1)
      {
         MatrixF xform = this->getRenderTransform();
         xform.setPosition(interpPos);

         MatrixF mat;
         mat.mul(xform, mProjectileShape->mNodeTransforms[mDataBlock->emitterNode[i]]);
         mat.getColumn(3,&p);
      }

      mLaserTrailList[i]->setLastNodePos(p);
   }
}

void Projectile::advanceTime(F32 dt)
{
   Parent::advanceTime(dt);

   if ( mHasExploded || dt == 0.0)
      return;

   if (mActivateThread &&
         mProjectileShape->getDuration(mActivateThread) > mProjectileShape->getTime(mActivateThread) + dt)
   {
      mProjectileShape->advanceTime(dt, mActivateThread);
   }
   else
   {

      if (mMaintainThread)
      {
         mProjectileShape->advanceTime(dt, mMaintainThread);
      }
      else if (mActivateThread && mDataBlock->maintainSeq != -1)
      {
         mMaintainThread = mProjectileShape->addThread();
         mProjectileShape->setTimeScale(mMaintainThread, 1);
         mProjectileShape->setSequence(mMaintainThread, mDataBlock->maintainSeq, 0);
         mProjectileShape->advanceTime(dt, mMaintainThread);
      }
   }
}

void Projectile::interpolateTick(F32 delta)
{
   Parent::interpolateTick(delta);

   if( mHasExploded )
      return;

   Point3F interpPos = mCurrDeltaBase + mCurrBackDelta * delta;
   Point3F dir = mCurrVelocity;
   if(dir.isZero())
      dir.set(0,0,1);
   else
      dir.normalize();

   MatrixF xform(true);
	xform = MathUtils::createOrientFromDir(dir);
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
void Projectile::onCollision(const Point3F& hitPosition, const Point3F& hitNormal, SceneObject* hitObject)
{
   // No client specific code should be placed or branched from this function
   if(isClientObject())
      return;

   if (hitObject != NULL && isServerObject())
   {
	   mDataBlock->onCollision_callback( this, hitObject, mFadeValue, hitPosition, hitNormal );
   }
}

void Projectile::onTargetLost()
{
	// No client specific code should be placed or branched from this function
	if(isServerObject())
	   mDataBlock->onTargetLost_callback(this);
}

//--------------------------------------------------------------------------
U32 Projectile::packUpdate( NetConnection *con, U32 mask, BitStream *stream )
{
   U32 retMask = Parent::packUpdate( con, mask, stream );

   const bool isInitalUpdate = mask & GameBase::InitialUpdateMask;

   // InitialUpdateMask
   if ( stream->writeFlag( isInitalUpdate ) )
   {
      stream->writeRangedU32( mCurrTick, 0, MaxLivingTicks );

		mathWrite(*stream, mInitialPosition);
		mathWrite(*stream, mInitialVelocity);

      if ( mSourceObject.isValid() )
      {
         // Potentially have to write this to the client, let's make sure it has a
         //  ghost on the other side...
         S32 ghostIndex = con->getGhostIndex( mSourceObject );
         if ( stream->writeFlag( ghostIndex != -1 ) )
         {
            stream->writeRangedU32( U32(ghostIndex), 
                                    0, 
                                    NetConnection::MaxGhostCount );

            stream->writeRangedU32( U32(mSourceObjectSlot),
                                    0, 
                                    ShapeBase::MaxMountedImages - 1 );
         }
         else 
            // have not recieved the ghost for the source object yet, try again later
            retMask |= GameBase::InitialUpdateMask;
      }
      else
         stream->writeFlag( false );
   }

   // ExplosionMask
   //
   // ExplosionMask will be set during the initial update but hidden is
   // only true if we have really exploded.
   if ( stream->writeFlag( ( mask & ExplosionMask ) && mHasExploded ) )
   {
      mathWrite(*stream, mExplosionPosition);
      mathWrite(*stream, mExplosionNormal);
      stream->write(mCollideHitType);
   }

   // BounceMask
   if ( stream->writeFlag( mask & BounceMask ) )
   {
      // Bounce against dynamic object
      mathWrite(*stream, mCurrPosition);
      mathWrite(*stream, mCurrVelocity);
   }


   // movement update...
   if(stream->writeFlag(mask & MovementMask))
   {
      mathWrite(*stream, mCurrPosition);
      mathWrite(*stream, mCurrVelocity);
		stream->writeInt(mCurrTrackingAbility,32);
   }

	// TargetMask
	if(stream->writeFlag(mask & TargetMask))
	{
      U32 targetMode = None;
      if(mTargetMode == Object)
      {
         if(mTarget)
         {
            S32 ghostIndex = con->getGhostIndex(mTarget);
            if(ghostIndex == -1)
               retMask |= TargetMask;
            else
               targetMode = Object; // No ghost, try again later.
         }
      }
      else
         targetMode = mTargetMode;

      stream->writeRangedU32(targetMode, None, Position);

      if(targetMode == Object)
      {
         S32 ghostIndex = con->getGhostIndex(mTarget);
         stream->writeRangedU32(U32(ghostIndex), 0, NetConnection::MaxGhostCount);
         mathWrite(*stream, mTargetPointOffset);
         //stream->writeFlag(mTargetClientControlled);
      }
      else if(targetMode == Position)
      {
         mathWrite(*stream, mTargetPosition);
      }
	}

   return retMask;
}

void Projectile::unpackUpdate(NetConnection* con, BitStream* stream)
{
   Parent::unpackUpdate(con, stream);
   
   if ( stream->readFlag() ) // InitialUpdateMask
   {
      mCurrTick = stream->readRangedU32( 0, MaxLivingTicks );

		mathRead(*stream, &mInitialPosition);
		mathRead(*stream, &mInitialVelocity);

      if ( stream->readFlag() )
      {
         mSourceObjectId   = stream->readRangedU32( 0, NetConnection::MaxGhostCount );
         mSourceObjectSlot = stream->readRangedU32( 0, ShapeBase::MaxMountedImages - 1 );

         NetObject* pObject = con->resolveGhost( mSourceObjectId );
         if ( pObject != NULL )
            mSourceObject = dynamic_cast<ShapeBase*>( pObject );
      }
      else
      {
         mSourceObjectId   = -1;
         mSourceObjectSlot = -1;
         mSourceObject     = NULL;
      }
   }
   
   if ( stream->readFlag() ) // ExplosionMask
   {
      Point3F explodePoint;
      Point3F explodeNormal;
      mathRead( *stream, &explodePoint );
      mathRead( *stream, &explodeNormal );
      stream->read( &mCollideHitType );

      // start the explosion visuals
      explode( explodePoint, explodeNormal, mCollideHitType );
   }

   if ( stream->readFlag() ) // BounceMask
   {
      Point3F pos;
      mathRead( *stream, &pos );
      mathRead( *stream, &mCurrVelocity );

      mCurrDeltaBase = pos;
      mCurrBackDelta = mCurrPosition - pos;
      mCurrPosition = pos;
      setPosition( mCurrPosition );
   }


   // movement update
   if (stream->readFlag())
   {
      mathRead(*stream, &mCurrPosition);
      mathRead(*stream, &mCurrVelocity);
		mCurrTrackingAbility = stream->readInt(32);
   }

	// TargetMask
	if(stream->readFlag())
	{
      U32 targetMode = stream->readRangedU32(None, Position);
      if(targetMode == None)
      {
         mTargetMode = None;
         mTarget = NULL;
      }
      else if(targetMode == Object)
      {
         mTargetMode = Object;
         U32 ghostIndex = stream->readRangedU32(0, NetConnection::MaxGhostCount);
         NetObject* pObject = con->resolveGhost(ghostIndex);
         if( pObject != NULL )
         {
            mTarget = dynamic_cast<GameBase*>(pObject);
            deleteNotify(mTarget);
         }
         mathRead(*stream, &mTargetPointOffset);
         //mTargetClientControlled = stream->readFlag();
      }
      else if(targetMode == Position)
      {
         mTarget = NULL;
         mTargetMode = Position;
         mathRead(*stream, &mTargetPosition);
      }
	}
}

//--------------------------------------------------------------------------
void Projectile::prepRenderImage( SceneRenderState* state )
{
   if (mHasExploded || mFadeValue <= (1.0/255.0))
      return;

   if ( mDataBlock->lightDesc )
   {
      mDataBlock->lightDesc->prepRender( state, &mLightState, getRenderTransform() );
   }

   /*
   if ( mFlareData )
   {
      mFlareState.fullBrightness = mDataBlock->lightDesc->mBrightness;
      mFlareState.scale = mFlareScale;
      mFlareState.lightInfo = mLight;
      mFlareState.lightMat = getTransform();

      mFlareData->prepRender( state, &mFlareState );
   }
   */

   prepBatchRender( state );
}

void Projectile::prepBatchRender( SceneRenderState *state )
{
   if ( !mProjectileShape )
      return;

	if(mDataBlock->hideShapeWhileOverlappingMuzzlePoint
   && this->getRenderWorldBox().isContained(mInitialPosition))
		return;

   GFXTransformSaver saver;

   // Set up our TS render state.
   TSRenderState rdata;
   rdata.setSceneState( state );

   // We might have some forward lit materials
   // so pass down a query to gather lights.
   LightQuery query;
   query.init( getWorldSphere() );
   rdata.setLightQuery( &query );

   MatrixF mat = getRenderTransform();
   mat.scale( mObjScale );
   mat.scale( mDataBlock->scale );
   GFX->setWorldMatrix( mat );

   mProjectileShape->setDetailFromPosAndScale( state, mat.getPosition(), mObjScale );
   mProjectileShape->animate();

   mProjectileShape->render( rdata );
}

void Projectile::proximityCallback(SceneObject* obj, void* key)
{
   ShapeBase* shape = (ShapeBase*)obj;
   ProximityInfo* prox = (ProximityInfo*)key;

   Projectile* prj = prox->prj;

   if(shape 
   && shape->getDamageState() == ShapeBase::Enabled
   && shape->getTeamId() != prj->getTeamId() 
   && shape->getTeamId() != 0)
   {
      const Point3F& oldPosition = prox->oldPos;
      const Point3F& newPosition = prox->newPos;

      Point3F vec = newPosition-oldPosition;

      const F32& x1 = oldPosition.x;
      const F32& y1 = oldPosition.y;
      const F32& z1 = oldPosition.z;

      const F32& x2 = newPosition.x;
      const F32& y2 = newPosition.y;
      const F32& z2 = newPosition.z;

      Point3F shapepos; shape->getWorldBox().getCenter(&shapepos);
      const F32& x3 = shapepos.x;
      const F32& y3 = shapepos.y;
      const F32& z3 = shapepos.z;

      F32 explodeDist = prj->mDataBlock->explodesNearEnemiesRadius;
      F32 missedDist = prj->mDataBlock->missEnemyEffectRadius;
      if(missedDist == 0)
         missedDist = explodeDist;

      // find point on vec that is closest to sphere...
      F32 u = ((x3-x1)*(x2-x1)+(y3-y1)*(y2-y1)+(z3-z1)*(z2-z1))
            / ((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));

      if(u >= 0 && u <= 1)
      {
         // closest point is indeed between old and new position...
         Point3F p = oldPosition+vec*u;
         F32 dist = (p-shapepos).len();
         if(prj->mDataBlock->explodesNearEnemies && 
            dist <= explodeDist && 
            prj->isServerObject() &&
            prj->mCurrTick > prj->mDataBlock->armingDelay)
         {
            // explode near enemy!
            MatrixF xform(true);
            xform.setColumn(3, p);
            prj->setTransform(xform);
            prj->mExplode = true;
         }
         else if(prj->mDataBlock->missEnemyEffect &&
                    dist <= missedDist &&
                    prj->isClientObject())
         {
            // missed enemy effect!
            // find actual intersection points...
            F32 r = missedDist;

#define SQR(x) ((x)*(x))
            F32 A = SQR(x2-x1)+SQR(y2-y1)+SQR(z2-z1);

            F32 B = 2*((x2-x1)*(x1-x3)+(y2-y1)*(y1-y3)+(z2-z1)*(z1-z3));

            F32 C = SQR(x3) + SQR(y3) + SQR(z3) + SQR(x1) + SQR(y1) + SQR(z1)
                  -2*(x3*x1 + y3*y1 + z3*z1) - SQR(r);

            // D < 0: no intersection
            // D = 1: one intersection
            // D > 1: two intersections
            F32 D = B*B - 4*A*C;

            if( D >= 0 )
            {
               u = (-B - mSqrt(D))/(2*A);
               // other u (if D > 1): (-B + mSqrt(D))/(2*A);

               Point3F exppos = oldPosition+vec*u;
               Point3F normal = prj->mCurrPosition;
               normal.normalize();

               Explosion* pExplosion = new Explosion;
               pExplosion->setPalette(prj->getPalette());
               pExplosion->onNewDataBlock(prj->mDataBlock->missEnemyEffect, false);

               MatrixF xform(true);
               xform.setPosition(exppos);
               pExplosion->setTransform(xform);
               pExplosion->setInitialState(exppos, normal);
               pExplosion->setCollideType(0);
               if (pExplosion->registerObject() == false)
               {
                  Con::errorf(ConsoleLogEntry::General, "Projectile(%s): missed enemy: couldn't register explosion",
                           prj->mDataBlock->getName() );
                  delete pExplosion;
               }
            }
         }
      }
   }
}      

void Projectile::missedEnemiesCheck(const Point3F& start, const Point3F& end)
{
   if( mDataBlock->explodesNearEnemies || mDataBlock->missEnemyEffect )
   {
      Point3F deltaVec = end - start;
      F32 distDelta = deltaVec.len();

      Point3F center = start + deltaVec*0.5;
      F32 radius = mDataBlock->explodesNearEnemiesRadius;
      if(mDataBlock->missEnemyEffectRadius > radius)
         radius = mDataBlock->missEnemyEffectRadius;
      F32 dist = distDelta/2 + radius;

      Box3F box;
      box.minExtents.x = center.x - dist;
      box.minExtents.y = center.y - dist;
      box.minExtents.z = center.z - dist;
      box.maxExtents.x = center.x + dist;
      box.maxExtents.y = center.y + dist;
      box.maxExtents.z = center.z + dist;

      U32 searchMask = mDataBlock->explodesNearEnemiesMask;

      ProximityInfo prox(this, start, end);

      this->getContainer()->findObjects(box,
         searchMask, &Projectile::proximityCallback, &prox);

      if(mExplode)
      {
         Point3F p = this->getPosition();
         Point3F n = mCurrVelocity; n.normalize();
         this->explode(p, n, 0);
      }
   }
}

bool Projectile::missedObject(const SceneObject* obj, const Point3F& oldPos, const Point3F& newPos)
{
   const Point3F& oldPosition = oldPos;
   const Point3F& newPosition = newPos;

   Point3F vec = newPosition-oldPosition;

   const F32& x1 = oldPosition.x;
   const F32& y1 = oldPosition.y;
   const F32& z1 = oldPosition.z;

   const F32& x2 = newPosition.x;
   const F32& y2 = newPosition.y;
   const F32& z2 = newPosition.z;

   Point3F shapepos; obj->getWorldBox().getCenter(&shapepos);
   const F32& x3 = shapepos.x;
   const F32& y3 = shapepos.y;
   const F32& z3 = shapepos.z;

   const F32& r = this->mDataBlock->explodesNearEnemiesRadius;

   // find point on vec that is closest to sphere...
   F32 u = ((x3-x1)*(x2-x1)+(y3-y1)*(y2-y1)+(z3-z1)*(z2-z1))
         / ((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));

   if(u >= 0 && u <= 1)
   {
      // closest point is indeed between old and new position...
      Point3F p = oldPosition+vec*u;
      if((p-shapepos).len() <= r)
         return true;
   }   

   return false;
}

void Projectile::createBounceExplosion(const RayInfo& rInfo, bool decal)
{
   if(isServerObject())
      return;

   if(!rInfo.object)
      return;

   Point3F p = rInfo.point;
   Point3F n = rInfo.normal;
   U32 t = rInfo.object->getTypeMask();

   ExplosionData* pData = NULL;
   for(U32 i = 0; i < ProjectileData::NumBounceEffects; i++)
   {
      if(mDataBlock->bounceEffect[i] != NULL
      && (mDataBlock->bounceEffectTypeMask[i] & t))
      {
         pData = mDataBlock->bounceEffect[i];
         break;
      }
   }

   if(!pData)
      return;

   // Explosion
   Explosion* pExplosion = new Explosion;
   pExplosion->setPalette(this->getPalette());
   pExplosion->onNewDataBlock(pData, false);

   MatrixF xform(true);
   xform.setPosition(p);
   pExplosion->setTransform(xform);
   pExplosion->setInitialState(p, n);
   if (pExplosion->registerObject() == false)
   {
      Con::errorf(ConsoleLogEntry::General, "Projectile(%s)::createBounceExplosion: couldn't register explosion",
                  mDataBlock->getName() );
      delete pExplosion;
      pExplosion = NULL;
   }

   // Decal
   if(decal && mDataBlock->bounceDecal)     
   {
      DecalInstance* dinst = gDecalManager->addDecal(p, n, 0.0f, mDataBlock->bounceDecal);
      if(dinst)
         dinst->mPalette = this->getPalette();
   }
}

DefineEngineMethod(Projectile, presimulate, void, (F32 seconds), (1.0f), 
                                       "@brief Updates the projectile's positional and collision information.\n\n"
                                       "This function will first delete the projectile if it is a server object and is outside it's ProjectileData::lifetime. "
                                       "Also responsible for applying gravity, determining collisions, triggering explosions, "
                                       "emitting trail particles, and calculating bounces if necessary."
									            "@param seconds Amount of time, in seconds since the simulation's start, to advance.\n"
									            "@tsexample\n"
									               "// Tell the projectile to process a simulation event, and provide the amount of time\n"
										            "// that has passed since the simulation began.\n"
										            "%seconds = 2.0;\n"
										            "%projectile.presimulate(%seconds);\n"
									            "@endtsexample\n"
                                       "@note This function is not called if the SimObject::hidden is true.")
{
	object->simulate( seconds );
}

DefineEngineMethod(Projectile, setTrackingAbility, void, (U32 val),,
   "@brief Set projectile tracking ability.\n\n"
   "@param New value\n"
)
{
	object->mCurrTrackingAbility = val;
	object->setMaskBits(Projectile::MovementMask);
}

DefineEngineMethod(Projectile, getTrackingAbility, S32, (),,
   "@brief Return projectile's current tracking ability value.\n\n"
)
{
	return object->mCurrTrackingAbility;
}

DefineEngineMethod(Projectile, setTarget, bool, (GameBase* target),,
   "@brief Set projectile's target.\n\n"
   "@param Target object\n"
)
{
   if(target)
	{
		object->setTarget(target);
		return true;
   }
	else
	{
		object->setTarget(NULL);
		return false;
	}
}

DefineEngineMethod(Projectile, getTarget, S32, (),,
   "@brief Return projectile's target object.\n\n"
)
{
   GameBase* target = object->getTarget();
   return target ? target->getId() : 0;
}

DefineEngineMethod(Projectile, setTargetPosition, void, (Point3F pos),,
   "@brief Set projectile's target position.\n\n"
   "@param Target point\n"
)
{
   object->setTargetPosition(pos);
}

DefineEngineMethod(Projectile, setVelocity, void, (Point3F vel),,
   "@brief Set projectile's velocity.\n\n"
   "@param vel The new velocity vector\n"
)
{
   object->mCurrVelocity = vel;
   object->setMaskBits(Projectile::MovementMask);
}

DefineEngineMethod(Projectile, getVelocity, Point3F, (),,
   "@brief Returns the projectile's velocity.\n\n"
)
{
   return object->mCurrVelocity;
}

DefineEngineMethod(Projectile, explode, void, (),,
   "@brief Causes the projectile to explode.\n\n"
)
{
   object->explode();
}

DefineEngineMethod(Projectile, isAlive, bool, (),,
   "@brief Returns wether the projectile is alive.\n\n"
)
{
   return object->isAlive();
}
