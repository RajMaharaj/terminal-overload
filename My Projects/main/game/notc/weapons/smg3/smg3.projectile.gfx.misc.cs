// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

datablock ParticleData(WpnSMG3ProjectileParticleEmitter_Particles)
{
	dragCoefficient      = "0.997067";
	gravityCoefficient   = 0.0;
	windCoefficient      = 0.0;
	inheritedVelFactor	= 0.0;
	constantAcceleration = 0.0;
	lifetimeMS			   = 30;
	lifetimeVarianceMS	= 0;
	spinRandomMin        = 0;
	spinRandomMax        = 0;
	textureName			   = "content/o/rotc/p.5.4/textures/rotc/corona";
	colors[0]            = "0.94902 0.996078 0.00784314 0.021";
	colors[1]            = "1 0.627451 0 0";
	sizes[0]             = "4.99908";
	sizes[1]             = "4.99908";
	times[0]             = 0.0;
	times[1]             = 1.0;
	useInvAlpha          = false;
	renderDot            = 0;
   animTexName = "content/o/rotc/p.5.4/textures/rotc/corona";
   times[2] = "1";
};

datablock ParticleEmitterData(WpnSMG3ProjectileParticleEmitter)
{
	ejectionPeriodMS = 1;
	periodVarianceMS = 0;
	ejectionVelocity = 0;
	velocityVariance = 2.5;
	ejectionOffset   = 0.0;
	thetaMin         = 0;
	thetaMax         = 0;
	phiReferenceVel  = 0;
	phiVariance      = 0;
	overrideAdvances = false;
	orientParticles  = false;
	//lifetimeMS		 = 1000;
	particles = "WpnSMG3ProjectileParticleEmitter_Particles";
};

datablock LightDescription(WpnSMG3ProjectileLightDesc : BulletProjectileLightDesc)
{
   color = "0 1 0.952941 1";
   range = "10";
   brightness = "1";
};

datablock DecalData(WpnSMG3ProjectileDecal)
{
   Material = "xa_notc_core_shapes_smg2_decal_p1_mat0";
   size = "1";
   lifeSpan = "2000";
   randomize = "1";
   screenStartRadius = "20";
   screenEndRadius = "5";
   clippingAngle = "180";
   textureCoordCount = "0";
   textureCoords[0] = "0 0 1 1";
   paletteSlot = 0;
   textureCoords[1] = "0.25 0 0.25 0.5";
   textureCoords[2] = "0.5 0 0.25 0.5";
   textureCoords[3] = "0.75 0 0.25 0.5";
   textureCoords[4] = "0 0.5 0.25 0.5";
   textureCoords[5] = "0.25 0.5 0.25 0.5";
   textureCoords[6] = "0.5 0.5 0.25 0.5";
   textureCoords[7] = "0.75 0.5 0.25 0.5";
};

datablock MultiNodeLaserBeamData(WpnSMG3ProjectileLaserTrail0)
{
	material[0] = "xa_notc_core_shapes_smg3_trail_p1_lasertrail0mat0";
   width[0] = "0.00001";
	material[1] = "xa_notc_core_shapes_smg3_trail_p1_lasertrail0mat1";
   width[1] = "0.025";
	material[2] = "xa_notc_core_shapes_smg3_trail_p1_lasertrail0mat2";
   width[2] = "0.1";
   
	renderMode = $MultiNodeLaserBeamRenderMode::FaceViewer;
	fadeTime = 100;
	windCoefficient = 0.0;

	// node x movement...
	nodeMoveMode[0]     = $NodeMoveMode::DynamicSpeed;
	nodeMoveSpeed[0]    = 5.0;
	nodeMoveSpeedAdd[0] = -10.0;
	// node y movement...
	nodeMoveMode[1]     = $NodeMoveMode::DynamicSpeed;
	nodeMoveSpeed[1]    = 5.0;
	nodeMoveSpeedAdd[1] = -10.0;
	// node z movement...
	nodeMoveMode[2]     = $NodeMoveMode::DynamicSpeed;
	nodeMoveSpeed[2]    = 5.0;
	nodeMoveSpeedAdd[2] = -10.0;

   nodeDistance = 2;
};


