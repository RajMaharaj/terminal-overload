// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

datablock ShotgunProjectileData(WpnSG1Projectile)
{
   //projectileShapeName = "content/xa/notc/core/shapes/smg1/projectile/p1/shape.dae";

   // ShotgunProjectileData fields
   bulletDistMode = 1;
	numBullets = 10;
	range = 500;
	muzzleSpreadRadius = 0.5;
	referenceSpreadRadius = 1.0;
	referenceSpreadDistance = 25;

   //lightDesc = BulletProjectileLightDesc;

	// script damage properties...
	impactDamage       = 10;
	impactImpulse      = 500;
	splashDamage       = 0;
	splashDamageRadius = 0;
	splashImpulse      = 0;
 
   explodesNearEnemies = false;
   explodesNearEnemiesRadius = 5;
   missEnemyEffect = GenericMissEnemyEffect1;
   
	energyDrain = 3; // how much energy does firing this projectile drain?

   explosion           = "WpnSG1ProjectileExplosion";
   decal               = "WpnSG1ProjectileDecal";
   //particleEmitter     = "WpnSG1ProjectileEmitter";
   laserTrail[0]       = "WpnSG1ProjectileLaserTrail";

   muzzleVelocity      = 9999;
   velInheritFactor    = 0;

   armingDelay         = 0;
   lifetime            = 992;
   fadeDelay           = 1472;
   bounceElasticity    = 0;
   bounceFriction      = 0;
   isBallistic         = false;
   gravityMod          = 1;

   //lightDesc = "WpnSG1ProjectileLightDesc";
};

function WpnSG1Projectile::onAdd(%this, %obj)
{
   //Parent::onAdd(%this, %obj);

   %data = WpnSG1Projectile;
   %ammo = WpnSG1Ammo;
	%player = %obj.sourceObject;
	%slot = %obj.sourceSlot;
 
   if(%player.getInventory(%ammo) == 0)
   {
      %obj.delete();
      return;
   }

   %player.decInventory(%ammo, 1);
}

function WpnSG1Projectile::onCollision(%this,%obj,%col,%fade,%pos,%normal)
{
   Parent::onCollision(%this,%obj,%col,%fade,%pos,%normal);
   
   if( !(%col.getType() & $TypeMasks::ShapeBaseObjectType) )
      return;

   %src = %obj.sourceObject;
   if(!isObject(%src))
      return;

   %currTime = getSimTime();

   // NOTE: This was a problem with ROTC, may not be
   //       a problem with Terminal Overload:
   // FIXME: strange linux version bug:
   //        after the game has been running a long time
   //        (%currTime == %obj.hitTime)
   //        often evaluates to false even if the
   //        values appear to be equal.
   //        (%currTime - %obj.hitTime) evaluates to 1
   //        in those cases.
   if(%currTime - %obj.hitTime <= 1)
   {
      %col.numSG1BulletHits += 1;
      if(%col.numSG1BulletHits == 4)
         %src.getDataBlock().addDiscTarget(%src, %col);
   }
   else
   {
      %obj.hitTime = %currTime;
      %col.numSG1BulletHits = 1;
   }
}

