// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

datablock ParticleData(WpnMG1ProjectileExplosion_Smoke)
{
	dragCoeffiecient	  = 1;
	gravityCoefficient	= "0";
	inheritedVelFactor	= 0.0;

	lifetimeMS			  = 500;
	lifetimeVarianceMS	= 200;

	useInvAlpha =  false;

	textureName = "content/o/rotc/p.5.4/textures/rotc/corona.png";

	colors[0]	  = "1.0 1.0 1.0 1.0";
	colors[1]	  = "1.0 1.0 1.0 0.0";
	sizes[0]		= "0.997986";
	sizes[1]		= 0.0;
	times[0]		= 0.0;
	times[1]		= 1.0;

	allowLighting = 0;
   renderDot = 0;
   animTexName = "content/o/rotc/p.5.4/textures/rotc/corona.png";
   times[2] = "1";
};

datablock ParticleEmitterData(WpnMG1ProjectileExplosion_SmokeEmitter)
{
	ejectionPeriodMS = 5;
	periodVarianceMS = 0;
	ejectionVelocity = "10";
	velocityVariance = "3";
	ejectionOffset	= 0.0;
	thetaMin			= "0";
	thetaMax			= 80;
	phiReferenceVel  = 0;
	phiVariance		= 360;
	overrideAdvances = 0;
	orientParticles  = false;
	lifetimeMS		 = 50;
	particles = "WpnMG1ProjectileExplosion_Smoke";
   paletteSlot = 0;
   blendStyle = "ADDITIVE";
   targetLockTimeMS = "480";
};

datablock ParticleData(WpnMG1ProjectileExplosion_DebrisParticles)
{
	spinSpeed = 200;
	spinRandomMin = -200.0;
	spinRandomMax =  200.0;
	dragCoefficient		= 1;
	gravityCoefficient	= 2.5;
	windCoefficient		= 0.0;
	inheritedVelFactor	= 0.0;
	constantAcceleration = 0.0;
	lifetimeMS			  = 1500;
	lifetimeVarianceMS	= 0;
	textureName = "content/o/rotc/p.5.4/shapes/rotc/misc/debris1.white";
	colors[0]	  = "1.0 1.0 1.0 1.0";
	colors[1]	  = "1.0 1.0 1.0 1.0";
	colors[2]	  = "1.0 1.0 1.0 0.0";
	sizes[0]		= 0.25;
	sizes[1]		= 0.25;
	sizes[2]		= 0.25;
	times[0]		= 0.0;
	times[1]		= 0.5;
	times[2]		= 1.0;
	useInvAlpha =  false;
	allowLighting = false;
};

datablock ParticleEmitterData(WpnMG1ProjectileExplosion_DebrisEmitter)
{
	ejectionPeriodMS = 50;
	periodVarianceMS = 0;
	ejectionVelocity = "20";
	velocityVariance = 5.0;
	ejectionOffset	= 0.0;
	thetaMin			= 0;
	thetaMax			= 60;
	phiReferenceVel  = 0;
	phiVariance		= 360;
	lifetimeMS		 = 5;
	lifetimeVarianceMS = 0;
	overrideAdvances = 0;
	orientParticles  = true;
	particles = "WpnMG1ProjectileExplosion_DebrisParticles";
   blendStyle = "ADDITIVE";
   targetLockTimeMS = "480";
};

datablock ExplosionData(WpnMG1ProjectileExplosion)
{
	soundProfile = WpnMG1ProjectileExplosionSound;

	lifetimeMS = 300;

	// shape...
	//explosionShape = "content/o/rotc/p.5.4/shapes/rotc/effects/explosion5.green.dts";
	faceViewer = false;
	playSpeed = 4.0;
	sizes[0] = "0.01 0.01 0.01";
	sizes[1] = "0.20 0.20 0.20";
	times[0] = 0.0;
	times[1] = 1.0;

	emitter[0] = WpnMG1ProjectileExplosion_DebrisEmitter;
	emitter[1] = WpnMG1ProjectileExplosion_SmokeEmitter;

	//debris = WpnMG1ProjectileExplosion_Debris;
	//debrisThetaMin = 0;
	//debrisThetaMax = 60;
	//debrisNum = 1;
	//debrisNumVariance = 1;
	//debrisVelocity = 10.0;
	//debrisVelocityVariance = 5.0;

	// Dynamic light
	lightStartRadius = 4;
	lightEndRadius = 0;
	lightStartColor = "1 1 1 1.0";
	lightEndColor = "1 1 1 1.0";

	shakeCamera = false;
};
