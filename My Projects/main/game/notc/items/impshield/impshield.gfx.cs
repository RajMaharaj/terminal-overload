// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

datablock MultiNodeLaserBeamData(ItemImpShieldBeam)
{
	material[0] = "xa_notc_core_shapes_standardcat_impshield_p1_beammat0";
   width[0] = "1.5";
	material[1] = "xa_notc_core_shapes_standardcat_impshield_p1_beammat1";
   width[1] = "0.001";
 
	renderMode = $MultiNodeLaserBeamRenderMode::FaceViewer;
	fadeTime = "128";
	windCoefficient = 0.0;

   // node x movement...
   nodeMoveMode[0]     = $NodeMoveMode::None;
   nodeMoveSpeed[0]    = -0.002;
   nodeMoveSpeedAdd[0] =  0.004;
   // node y movement...
   nodeMoveMode[1]     = $NodeMoveMode::None;
   nodeMoveSpeed[1]    = -0.002;
   nodeMoveSpeedAdd[1] =  0.004;
   // node z movement...
   nodeMoveMode[2]     = $NodeMoveMode::None;
   nodeMoveSpeed[2]    = 0.5;
   nodeMoveSpeedAdd[2] = 1.0;

   //nodeDistance = 0.5;
};

datablock ParticleData(ItemImpShieldDropExplosion_Cloud)
{
	dragCoeffiecient	  = 0.4;
	gravityCoefficient	= 0;
	inheritedVelFactor	= 0.025;

	lifetimeMS			  = 250;
	lifetimeVarianceMS	= 0;

	useInvAlpha = false;
	spinRandomMin = -200.0;
	spinRandomMax =  200.0;

	textureName = "content/o/rotc/p.5.4/textures/rotc/corona.png";

	colors[0]	  = "1.0 1.0 1.0 1.0";
	colors[1]	  = "1.0 1.0 1.0 0.5";
	colors[2]	  = "1.0 1.0 1.0 0.0";
	sizes[0]		= 7.0;
	sizes[1]		= 2.5;
	sizes[2]		= 0.0;
	times[0]		= 0.0;
	times[1]		= 0.5;
	times[2]		= 1.0;

	allowLighting = false;
};

datablock ParticleEmitterData(ItemImpShieldDropExplosion_CloudEmitter)
{
	ejectionPeriodMS = 1;
	periodVarianceMS = 0;

	ejectionVelocity = 0.25;
	velocityVariance = 0.25;

	thetaMin			= 0.0;
	thetaMax			= 90.0;

	lifetimeMS		 = 100;

	particles = "ItemImpShieldDropExplosion_Cloud";
};

datablock ParticleData(ItemImpShieldDropExplosion_Dust)
{
	dragCoefficient		= 1.0;
	gravityCoefficient	= -0.01;
	inheritedVelFactor	= 0.0;
	constantAcceleration = 0.0;
	lifetimeMS			  = 2000;
	lifetimeVarianceMS	= 100;
	useInvAlpha			 = true;
	spinRandomMin		  = -90.0;
	spinRandomMax		  = 500.0;
	textureName			 = "content/o/rotc/p.5.4/textures/rotc/smoke_particle.png";
	colors[0]	  = "0.9 0.9 0.9 0.5";
	colors[1]	  = "0.9 0.9 0.9 0.5";
	colors[2]	  = "0.9 0.9 0.9 0.0";
	sizes[0]		= 0.9;
	sizes[1]		= 1.5;
	sizes[2]		= 1.6;
	times[0]		= 0.0;
	times[1]		= 0.7;
	times[2]		= 1.0;
	allowLighting = true;
};

datablock ParticleEmitterData(ItemImpShieldDropExplosion_DustEmitter)
{
	ejectionPeriodMS = 5;
	periodVarianceMS = 0;
	ejectionVelocity = 2.0;
	velocityVariance = 0.0;
	ejectionOffset	= 0.0;
	thetaMin			= 0;
	thetaMax			= 180;
	phiReferenceVel  = 0;
	phiVariance		= 360;
	overrideAdvances = false;
	lifetimeMS		 = 50;
	particles = "ItemImpShieldDropExplosion_Dust";
};


datablock ParticleData(ItemImpShieldDropExplosion_Smoke)
{
	dragCoeffiecient	  = 0.4;
	gravityCoefficient	= -0.1;	// rises slowly
	inheritedVelFactor	= 0.025;

	lifetimeMS			  = 2000;
	lifetimeVarianceMS	= 0;

	useInvAlpha =  true;
	spinRandomMin = -200.0;
	spinRandomMax =  200.0;

	textureName = "content/o/rotc/p.5.4/textures/rotc/smoke_particle.png";

	colors[0]	  = "0.4 0.4 0.4 0.4";
	colors[1]	  = "0.4 0.4 0.4 0.2";
	colors[2]	  = "0.4 0.4 0.4 0.0";
	sizes[0]		= 0.0;
	sizes[1]		= 6.0;
	sizes[2]		= 0.6;
	times[0]		= 0.0;
	times[1]		= 0.5;
	times[2]		= 1.0;

	allowLighting = false;
};

datablock ParticleEmitterData(ItemImpShieldDropExplosion_SmokeEmitter)
{
	ejectionPeriodMS = 5;
	periodVarianceMS = 0;

	ejectionVelocity = 2.0;
	velocityVariance = 0.25;

	thetaMin			= 0.0;
	thetaMax			= 180.0;

	lifetimeMS		 = 50;

	particles = "ItemImpShieldDropExplosion_Smoke";
};

datablock ParticleData(ItemImpShieldDropExplosion_Shrapnel)
{
	dragCoefficient		= 1;
	gravityCoefficient	= "4";
	windCoefficient	= 0.0;
	inheritedVelFactor	= "0";
	constantAcceleration = 0.0;
	lifetimeMS			  = 1000;
	lifetimeVarianceMS	= 0;
	textureName			 = "content/o/rotc/p.5.4/textures/rotc/corona.png";
	colors[0]	  = "1 1 1 1";
	colors[1]	  = "1 1 1 0.5";
	colors[2]	  = "1 1 1 0";
	sizes[0]		= 0.5;
	sizes[1]		= 0.5;
	sizes[2]		= 0.5;
	times[0]		= 0.0;
	times[1]		= 0.5;
	times[2]		= 1.0;
	allowLighting = 0;
	renderDot = 0;
   animTexName = "content/o/rotc/p.5.4/textures/rotc/corona.png";
};

datablock ParticleEmitterData(ItemImpShieldDropExplosion_ShrapnelEmitter)
{
	ejectionPeriodMS = 1;
	periodVarianceMS = 0;
	ejectionVelocity = 5;
	velocityVariance = 0;
	ejectionOffset	= "1";
	thetaMin			= "50";
	thetaMax			= "90";
	phiReferenceVel  = 0;
	phiVariance		= 360;
	overrideAdvances = 0;
	orientParticles  = false;
	lifetimeMS		 = 50;
	particles = "ItemImpShieldDropExplosion_Shrapnel";
   blendStyle = "ADDITIVE";
};

datablock ParticleData(ItemImpShieldDropExplosion_Sparks)
{
	dragCoefficient		= 1;
	gravityCoefficient	= 0.0;
	inheritedVelFactor	= 0.2;
	constantAcceleration = 0.0;
	lifetimeMS			  = 500;
	lifetimeVarianceMS	= 350;
	textureName			 = "content/o/rotc/p.5.4/textures/rotc/particle1.png";
	colors[0]	  = "0.56 0.36 0.26 1.0";
	colors[1]	  = "0.56 0.36 0.26 1.0";
	colors[2]	  = "1.0 0.36 0.26 0.0";
	sizes[0]		= 0.5;
	sizes[1]		= 0.5;
	sizes[2]		= 0.75;
	times[0]		= 0.0;
	times[1]		= 0.5;
	times[2]		= 1.0;
	allowLighting = false;
};

datablock ParticleEmitterData(ItemImpShieldDropExplosion_SparksEmitter)
{
	ejectionPeriodMS = 10;
	periodVarianceMS = 0;
	ejectionVelocity = 4;
	velocityVariance = 1;
	ejectionOffset	= 0.0;
	thetaMin			= 0;
	thetaMax			= 60;
	phiReferenceVel  = 0;
	phiVariance		= 360;
	overrideAdvances = false;
	orientParticles  = true;
	lifetimeMS		 = 100;
	particles = "ItemImpShieldDropExplosion_Sparks";
};

datablock ExplosionData(ItemImpShieldDropExplosion)
{
	soundProfile = ItemImpShieldDropExplosionSound;
	lifetimeMS = 200;
	emitter[0] = ItemImpShieldDropExplosion_ShrapnelEmitter;
};


