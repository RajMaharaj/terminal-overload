// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "T3D/player.h"

#include "platform/profiler.h"
#include "math/mMath.h"
#include "math/mathIO.h"
#include "core/resourceManager.h"
#include "core/stringTable.h"
#include "core/volume.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "console/engineAPI.h"
#include "collision/extrudedPolyList.h"
#include "collision/clippedPolyList.h"
#include "collision/earlyOutPolyList.h"
#include "ts/tsShapeInstance.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxTrack.h"
#include "sfx/sfxSource.h"
#include "sfx/sfxTypes.h"
#include "scene/sceneManager.h"
#include "scene/sceneRenderState.h"
#include "T3D/gameBase/gameConnection.h"
#include "T3D/trigger.h"
#include "T3D/physicalZone.h"
#include "T3D/item.h"
#include "T3D/missionArea.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/fx/cameraFXMgr.h"
#include "T3D/fx/splash.h"
#include "T3D/tsStatic.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsPlayer.h"
#include "T3D/decal/decalManager.h"
#include "T3D/decal/decalData.h"
#include "materials/baseMatInstance.h"
#include "NOTC/tacticalzone.h"

#ifdef TORQUE_EXTENDED_MOVE
   #include "T3D/gameBase/extended/extendedMove.h"
#endif

// Amount of time if takes to transition to a new action sequence.
static F32 sAnimationTransitionTime = 0.25f;
static bool sUseAnimationTransitions = true;
static F32 sLandReverseScale = 0.25f;
static F32 sSlowStandThreshSquared = 1.69f;
static S32 sRenderMyPlayer = true;
static S32 sRenderMyItems = true;
static bool sRenderPlayerCollision = false;

// Chooses new action animations every n ticks.
static const F32 sNewAnimationTickTime = 4.0f;
static const F32 sMountPendingTickWait = 13.0f * F32(TickMs);

// Number of ticks before we pick non-contact animations
static const S32 sContactTickTime = 10;

// Movement constants
static F32 sVerticalStepDot = 0.173f;   // 80
static F32 sMinFaceDistance = 0.01f;
static F32 sTractionDistance = 0.04f;
static F32 sNormalElasticity = 0.01f;
static U32 sMoveRetryCount = 5;
static F32 sMaxImpulseVelocity = 200.0f;

// Move triggers
static S32 sSlideTrigger = 6;
static S32 sJumpTrigger = 2;
static S32 sCrouchTrigger = 3;
static S32 sProneTrigger = 4;
static S32 sSprintTrigger = 5;
static S32 sImageTrigger0 = 0;
static S32 sImageTrigger1 = 1;
static S32 sJumpJetTrigger = 1;
static S32 sVehicleDismountTrigger = 2;
static S32 sChargedXJumpTrigger = 7;
static S32 sInstantXJumpTrigger = 2;

// Client prediction
static F32 sMinWarpTicks = 0.5f;       // Fraction of tick at which instant warp occurs
static S32 sMaxWarpTicks = 3;          // Max warp duration in ticks
static S32 sMaxPredictionTicks = 30;   // Number of ticks to predict

S32 Player::smExtendedMoveHeadPosRotIndex = 0;  // The ExtendedMove position/rotation index used for head movements

// Anchor point compression
const F32 sAnchorMaxDistance = 32.0f;

//
static U32 sCollisionMoveMask =  TerrainObjectType       |
                                 WaterObjectType         | 
                                 PlayerObjectType        |
                                 StaticShapeObjectType   | 
                                 VehicleObjectType       |
                                 PhysicalZoneObjectType;

static U32 sServerCollisionContactMask = sCollisionMoveMask      |
                                         ItemObjectType          |
                                         TriggerObjectType       |
                                         TacticalZoneObjectType  |
                                         CorpseObjectType;

static U32 sClientCollisionContactMask = sCollisionMoveMask      |
                                         TriggerObjectType;

enum PlayerConstants {
   JumpSkipContactsMax = 8
};

//----------------------------------------------------------------------------
// Player shape animation sequences:

// Action Animations:
PlayerData::ActionAnimationDef PlayerData::ActionAnimationList[NumTableActionAnims] =
{
   // *** WARNING ***
   // This array is indexed using the enum values defined in player.h

   // Root is the default animation
   { "root" },       // RootAnim,

   // These are selected in the move state based on velocity
   { "run",          { 0.0f, 1.0f, 0.0f } },       // RunForwardAnim,
   { "back",         { 0.0f,-1.0f, 0.0f } },       // BackBackwardAnim
   { "side",         {-1.0f, 0.0f, 0.0f } },       // SideLeftAnim,
   { "side_right",   { 1.0f, 0.0f, 0.0f } },       // SideRightAnim,

   { "sprint_root" },
   { "sprint_forward",  { 0.0f, 1.0f, 0.0f } },
   { "sprint_backward", { 0.0f,-1.0f, 0.0f } },
   { "sprint_side",     {-1.0f, 0.0f, 0.0f } },
   { "sprint_right",    { 1.0f, 0.0f, 0.0f } },

   { "crouch_root" },
   { "crouch_forward",  { 0.0f, 1.0f, 0.0f } },
   { "crouch_backward", { 0.0f,-1.0f, 0.0f } },
   { "crouch_side",     {-1.0f, 0.0f, 0.0f } },
   { "crouch_right",    { 1.0f, 0.0f, 0.0f } },

   { "prone_root" },
   { "prone_forward",   { 0.0f, 1.0f, 0.0f } },
   { "prone_backward",  { 0.0f,-1.0f, 0.0f } },

   { "swim_root" },
   { "swim_forward",    { 0.0f, 1.0f, 0.0f } },
   { "swim_backward",   { 0.0f,-1.0f, 0.0f }  },
   { "swim_left",       {-1.0f, 0.0f, 0.0f }  },
   { "swim_right",      { 1.0f, 0.0f, 0.0f }  },

   // These are set explicitly based on player actions
   { "fall" },       // FallAnim
   { "slide" },      // SlideAnim 
   { "jump" },       // JumpAnim
   { "standjump" },  // StandJumpAnim
   { "land" },       // LandAnim
   { "jet" },        // JetAnim
};


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(PlayerData);

ConsoleDocClass( PlayerData,
   "@brief Defines properties for a Player object.\n\n"
   "@see Player\n"
   "@ingroup gameObjects\n"
);

IMPLEMENT_CALLBACK( PlayerData, onPoseChange, void, ( Player* obj, const char* oldPose, const char* newPose ), ( obj, oldPose, newPose ),
   "@brief Called when the player changes poses.\n\n"
   "@param obj The Player object\n"
   "@param oldPose The pose the player is switching from.\n"
   "@param newPose The pose the player is switching to.\n");

IMPLEMENT_CALLBACK( PlayerData, onStartSwim, void, ( Player* obj ), ( obj ),
   "@brief Called when the player starts swimming.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onStopSwim, void, ( Player* obj ), ( obj ),
   "@brief Called when the player stops swimming.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onStartSprintMotion, void, ( Player* obj ), ( obj ),
   "@brief Called when the player starts moving while in a Sprint pose.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onStopSprintMotion, void, ( Player* obj ), ( obj ),
   "@brief Called when the player stops moving while in a Sprint pose.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onStartSliding, void, ( Player* obj ), ( obj ),
   "@brief Called when the player starts sliding.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onStopSliding, void, ( Player* obj ), ( obj ),
   "@brief Called when the player stops sliding.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onJump, void, ( Player* obj ), ( obj ),
   "@brief Called when the player jumps.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onXJumpChargeStart, void, ( Player* obj ), ( obj ),
   "@brief Called when the player starts charching an x-jump.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onXJump, void, ( Player* obj, Point3F dir ), ( obj, dir ),
   "@brief Called when the player x-jumps.\n\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, doDismount, void, ( Player* obj ), ( obj ),
   "@brief Called when attempting to dismount the player from a vehicle.\n\n"
   "It is up to the doDismount() method to actually perform the dismount.  Often "
   "there are some conditions that prevent this, such as the vehicle moving too fast.\n"
   "@param obj The Player object\n" );

IMPLEMENT_CALLBACK( PlayerData, onEnterLiquid, void, ( Player* obj, F32 coverage, const char* type ), ( obj, coverage, type ),
   "@brief Called when the player enters a liquid.\n\n"
   "@param obj The Player object\n"
   "@param coverage Percentage of the player's bounding box covered by the liquid\n"
   "@param type The type of liquid the player has entered\n" );

IMPLEMENT_CALLBACK( PlayerData, onLeaveLiquid, void, ( Player* obj, const char* type ), ( obj, type ),
   "@brief Called when the player leaves a liquid.\n\n"
   "@param obj The Player object\n"
   "@param type The type of liquid the player has left\n" );

IMPLEMENT_CALLBACK( PlayerData, animationDone, void, ( Player* obj ), ( obj ),
   "@brief Called on the server when a scripted animation completes.\n\n"
   "@param obj The Player object\n"
   "@see Player::setActionThread() for setting a scripted animation and its 'hold' parameter to "
   "determine if this callback is used.\n" );

IMPLEMENT_CALLBACK( PlayerData, onEnterMissionArea, void, ( Player* obj ), ( obj ),
   "@brief Called when the player enters the mission area.\n\n"
   "@param obj The Player object\n"
   "@see MissionArea\n" );

IMPLEMENT_CALLBACK( PlayerData, onLeaveMissionArea, void, ( Player* obj ), ( obj ),
   "@brief Called when the player leaves the mission area.\n"
   "@param obj The Player object\n"
   "@see MissionArea\n" );

PlayerData::PlayerData()
{
   shadowEnable = true;
   shadowSize = 256;
   shadowProjectionDistance = 14.0f;

   renderFirstPerson = true;
   firstPersonShadows = false;

   // Used for third person image rendering
   imageAnimPrefix = StringTable->insert("");

   allowImageStateAnimation = false;

   // Used for first person image rendering
   imageAnimPrefixFP = StringTable->insert("");
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      shapeNameFP[i] = StringTable->insert("");
      mCRCFP[i] = 0;
      mValidShapeFP[i] = false;
   }

   pickupRadius = 0.0f;
   minLookAngle = -1.4f;
   maxLookAngle = 1.4f;
   maxFreelookAngle = 3.0f;
   maxTimeScale = 1.5f;

   mass = 9.0f;         // from ShapeBase
   for(U32 i = 0; i < ShapeBaseData::MaxEnergySlots; i++)
      maxEnergy[i] = 60.0f;   // from ShapeBase
   drag = 0.3f;         // from ShapeBase
   density = 1.1f;      // from ShapeBase

   gravityMod = 1.0;

   skidSpeed = 9999;
   skidFactor = 1.0;

   maxStepHeight = 1.0f;
   runSurfaceAngle = 80.0f;

   fallingSpeedThreshold = -10.0f;

   recoverDelay = 30;
   recoverRunForceScale = 1.0f;
   landSequenceTime = 0.0f;
   transitionToLand = false;

   // Running/Walking
   runForce = 40.0f * 9.0f;
   runEnergyDrain = 0.0f;
   minRunEnergy = 0.0f;
   maxForwardSpeed = 10.0f;
   maxBackwardSpeed = 10.0f;
   maxSideSpeed = 10.0f;

	// Sliding
   slideForce = 20.0f * 9.0f;
   slideEnergyDrain = 0.0f;
   slideEnergySlot = -1;
   minSlideEnergy = 0.0f;
   maxSlideForwardSpeed = 15.0f;  
   maxSlideBackwardSpeed = 10.0f; 
   maxSlideSideSpeed = 10.0f;

   // Jumping
   jumpForce = 75.0f;
   jumpEnergyDrain = 0.0f;
   minJumpEnergy = 0.0f;
   jumpSurfaceAngle = 78.0f;
   jumpDelay = 30;
   minJumpSpeed = 500.0f;
   maxJumpSpeed = 2.0f * minJumpSpeed;

   // xJump
   xJumpEnergySlot = 1;
   xJumpChargeRate = 0.004;
   xJumpSpeedFactorD = 25;
   xJumpPowerBaseConst = 1000;
   xJumpPowerMultConst = 2000;
   xJumpInstantEnergy = 0.15;

   // Sprinting
   sprintForce = 50.0f * 9.0f;
   sprintEnergyDrain = 0.0f;
   minSprintEnergy = 0.0f;
   maxSprintForwardSpeed = 15.0f;
   maxSprintBackwardSpeed = 10.0f;
   maxSprintSideSpeed = 10.0f;
   sprintStrafeScale = 1.0f;
   sprintYawScale = 1.0f;
   sprintPitchScale = 1.0f;
   sprintCanJump = true;

   // Swimming
   swimForce = 55.0f * 9.0f;  
   maxUnderwaterForwardSpeed = 6.0f;
   maxUnderwaterBackwardSpeed = 6.0f;
   maxUnderwaterSideSpeed = 6.0f;

   // Crouching
   crouchForce = 45.0f * 9.0f;
   maxCrouchForwardSpeed = 4.0f;
   maxCrouchBackwardSpeed = 4.0f;
   maxCrouchSideSpeed = 4.0f;    

   // Prone
   proneForce = 45.0f * 9.0f;            
   maxProneForwardSpeed = 2.0f;  
   maxProneBackwardSpeed = 2.0f; 
   maxProneSideSpeed = 0.0f;     

   // Jetting
   jetJumpForce = 0;
   jetJumpEnergyDrain = 0;
   jetMinJumpEnergy = 0;
   jetJumpSurfaceAngle = 78;
   jetMinJumpSpeed = 20;
   jetMaxJumpSpeed = 100;

   horizMaxSpeed = 80.0f;
   horizResistSpeed = 38.0f;
   horizResistFactor = 1.0f;

   upMaxSpeed = 80.0f;
   upResistSpeed = 38.0f;
   upResistFactor = 1.0f;

   minImpactSpeed = 25.0f;
   minLateralImpactSpeed = 25.0f;

   decalData      = NULL;
   decalID        = 0;
   decalOffset      = 0.0f;

   actionCount = 0;
   slideDecal = NULL;
	slideDecalID = 0;

   skidDecal = NULL;
	skidDecalID = 0;

   lookAction = 0;

   // size of bounding box
   boxSize.set(1.0f, 1.0f, 2.3f);
   crouchBoxSize.set(1.0f, 1.0f, 2.0f);
   proneBoxSize.set(1.0f, 2.3f, 1.0f);
   swimBoxSize.set(1.0f, 2.3f, 1.0f);

   // location of head, torso, legs
   boxHeadPercentage = 0.85f;
   boxTorsoPercentage = 0.55f;

   // damage locations
   boxHeadLeftPercentage  = 0;
   boxHeadRightPercentage = 1;
   boxHeadBackPercentage  = 0;
   boxHeadFrontPercentage = 1;

   for (S32 i = 0; i < MaxSounds; i++)
      sound[i] = NULL;

   footPuffEmitter = NULL;
   footPuffID = 0;
   footPuffNumParts = 15;
   footPuffRadius = .25f;

   dustEmitter = NULL;
   dustID = 0;

   splash = NULL;
   splashId = 0;
   splashVelocity = 1.0f;
   splashAngle = 45.0f;
   splashFreqMod = 300.0f;
   splashVelEpsilon = 0.25f;
   bubbleEmitTime = 0.4f;

   medSplashSoundVel = 2.0f;
   hardSplashSoundVel = 3.0f;
   exitSplashSoundVel = 2.0f;
   footSplashHeight = 0.1f;

   dMemset( splashEmitterList, 0, sizeof( splashEmitterList ) );
   dMemset( splashEmitterIDList, 0, sizeof( splashEmitterIDList ) );

   for (S32 i = 0; i < NumSlideEmitters; i++)
      slideEmitter[i] = NULL;

   for (S32 i = 0; i < NumSkidEmitters; i++)
      skidEmitter[i] = NULL;

   groundImpactMinSpeed = 10.0f;
   groundImpactShakeFreq.set( 10.0f, 10.0f, 10.0f );
   groundImpactShakeAmp.set( 20.0f, 20.0f, 20.0f );
   groundImpactShakeDuration = 1.0f;
   groundImpactShakeFalloff = 10.0f;

   // Air control
	glideForce = 0.0f;
   airControl = 0.0f;

   jumpTowardsNormal = true;

   physicsPlayerType = StringTable->insert("");

   dMemset( actionList, 0, sizeof(actionList) );
}

bool PlayerData::preload(bool server, String &errorStr)
{
   if(!Parent::preload(server, errorStr))
      return false;

   // Resolve objects transmitted from server
   if( !server )
   {
      for( U32 i = 0; i < MaxSounds; ++ i )
      {
         String sfxErrorStr;
         if( !sfxResolve( &sound[ i ], sfxErrorStr ) )
            Con::errorf( "PlayerData::preload: %s", sfxErrorStr.c_str() );
      }

      for(int i=0; i < NumSlideEmitters; i++)
         if(slideEmitter[i])
            Sim::findObject(SimObjectId(slideEmitter[i]), slideEmitter[i]);

      for(int i=0; i < NumSkidEmitters; i++)
         if(skidEmitter[i])
            Sim::findObject(SimObjectId(skidEmitter[i]), skidEmitter[i]);
   }

   //
   runSurfaceCos = mCos(mDegToRad(runSurfaceAngle));
   jumpSurfaceCos = mCos(mDegToRad(jumpSurfaceAngle));
   if (minJumpEnergy < jumpEnergyDrain)
      minJumpEnergy = jumpEnergyDrain;   

   // Jetting
   if (jetMinJumpEnergy < jetJumpEnergyDrain)
      jetMinJumpEnergy = jetJumpEnergyDrain;

   // Validate some of the data
   if (fallingSpeedThreshold > 0.0f)
      Con::printf("PlayerData:: Falling speed threshold should be downwards (negative)");

   if (recoverDelay > (1 << RecoverDelayBits) - 1) {
      recoverDelay = (1 << RecoverDelayBits) - 1;
      Con::printf("PlayerData:: Recover delay exceeds range (0-%d)",recoverDelay);
   }
   if (jumpDelay > (1 << JumpDelayBits) - 1) {
      jumpDelay = (1 << JumpDelayBits) - 1;
      Con::printf("PlayerData:: Jump delay exceeds range (0-%d)",jumpDelay);
   }

   // If we don't have a shape don't crash out trying to
   // setup animations and sequences.
   if ( mShape )
   {
      // Go ahead a pre-load the player shape
      TSShapeInstance* si = new TSShapeInstance(mShape, false);
      TSThread* thread = si->addThread();

      // Extract ground transform velocity from animations
      // Get the named ones first so they can be indexed directly.
      ActionAnimation *dp = &actionList[0];
      for (S32 i = 0; i < NumTableActionAnims; i++,dp++)
      {
         ActionAnimationDef *sp = &ActionAnimationList[i];
         dp->name          = sp->name;
         dp->dir.set(sp->dir.x,sp->dir.y,sp->dir.z);
         dp->sequence      = mShape->findSequence(sp->name);

         // If this is a sprint action and is missing a sequence, attempt to use
         // the standard run ones.
         if(dp->sequence == -1 && i >= SprintRootAnim && i <= SprintRightAnim)
         {
            S32 offset = i-SprintRootAnim;
            ActionAnimationDef *standDef = &ActionAnimationList[RootAnim+offset];
            dp->sequence = mShape->findSequence(standDef->name);
         }

         dp->velocityScale = true;
         dp->death         = false;
         if (dp->sequence != -1)
            getGroundInfo(si,thread,dp);
      }
      for (S32 b = 0; b < mShape->sequences.size(); b++)
      {
         if (!isTableSequence(b))
         {
            dp->sequence      = b;
            dp->name          = mShape->getName(mShape->sequences[b].nameIndex);
            dp->velocityScale = false;
            getGroundInfo(si,thread,dp++);
         }
      }
      actionCount = dp - actionList;
      AssertFatal(actionCount <= NumActionAnims, "Too many action animations!");
      delete si;

      // Resolve lookAction index
      dp = &actionList[0];
      String lookName("look");
      for (S32 c = 0; c < actionCount; c++,dp++)
         if( dStricmp( dp->name, lookName ) == 0 )
            lookAction = c;

      // Resolve spine
      spineNode[0] = mShape->findNode("Bip01 Pelvis");
      spineNode[1] = mShape->findNode("Bip01 Spine");
      spineNode[2] = mShape->findNode("Bip01 Spine1");
      spineNode[3] = mShape->findNode("Bip01 Spine2");
      spineNode[4] = mShape->findNode("Bip01 Neck");
      spineNode[5] = mShape->findNode("Bip01 Head");

      // Recoil animations
      recoilSequence[0] = mShape->findSequence("light_recoil");
      recoilSequence[1] = mShape->findSequence("medium_recoil");
      recoilSequence[2] = mShape->findSequence("heavy_recoil");

      // Wings animation
      wingsSequence = mShape->findSequence("wings");
   }

   // Convert pickupRadius to a delta of boundingBox
   //
   // NOTE: it is not really correct to precalculate a pickupRadius based 
   // on boxSize since the actual player's bounds can vary by "pose".
   //
   F32 dr = (boxSize.x > boxSize.y)? boxSize.x: boxSize.y;
   if (pickupRadius < dr)
      pickupRadius = dr;
   else
      if (pickupRadius > 2.0f * dr)
         pickupRadius = 2.0f * dr;
   pickupDelta = (S32)(pickupRadius - dr);

   // Validate jump speed
   if (maxJumpSpeed <= minJumpSpeed)
      maxJumpSpeed = minJumpSpeed + 0.1f;

   // Load up all the emitters
   if (!footPuffEmitter && footPuffID != 0)
      if (!Sim::findObject(footPuffID, footPuffEmitter))
         Con::errorf(ConsoleLogEntry::General, "PlayerData::preload - Invalid packet, bad datablockId(footPuffEmitter): 0x%x", footPuffID);

   if (!decalData && decalID != 0 )
      if (!Sim::findObject(decalID, decalData))
         Con::errorf(ConsoleLogEntry::General, "PlayerData::preload Invalid packet, bad datablockId(decalData): 0x%x", decalID);

   if(!slideDecal && slideDecalID != 0)
      if(!Sim::findObject(slideDecalID, slideDecal))
         Con::errorf(ConsoleLogEntry::General, "PlayerData::preload Invalid packet, bad datablockId(slideDecal): 0x%x", slideDecalID);

   if(!skidDecal && skidDecalID != 0)
      if(!Sim::findObject(skidDecalID, skidDecal))
         Con::errorf(ConsoleLogEntry::General, "PlayerData::preload Invalid packet, bad datablockId(skidDecal): 0x%x", skidDecalID);

   if (!dustEmitter && dustID != 0 )
      if (!Sim::findObject(dustID, dustEmitter))
         Con::errorf(ConsoleLogEntry::General, "PlayerData::preload - Invalid packet, bad datablockId(dustEmitter): 0x%x", dustID);

   for (S32 i=0; i<NUM_SPLASH_EMITTERS; i++)
      if( !splashEmitterList[i] && splashEmitterIDList[i] != 0 )
         if( Sim::findObject( splashEmitterIDList[i], splashEmitterList[i] ) == false)
            Con::errorf(ConsoleLogEntry::General, "PlayerData::onAdd - Invalid packet, bad datablockId(particle emitter): 0x%x", splashEmitterIDList[i]);

   // First person mounted image shapes.
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      bool shapeError = false;

      if (shapeNameFP[i] && shapeNameFP[i][0])
      {
         mShapeFP[i] = ResourceManager::get().load(shapeNameFP[i]);
         if (bool(mShapeFP[i]) == false)
         {
            errorStr = String::ToString("PlayerData: Couldn't load mounted image %d shape \"%s\"",i,shapeNameFP[i]);
            return false;
         }

         if(!server && !mShapeFP[i]->preloadMaterialList(mShapeFP[i].getPath()) && NetConnection::filesWereDownloaded())
            shapeError = true;

         if(computeCRC)
         {
            Con::printf("Validation required for mounted image %d shape: %s", i, shapeNameFP[i]);

            Torque::FS::FileNodeRef    fileRef = Torque::FS::GetFileNode(mShapeFP[i].getPath());

            if (!fileRef)
            {
               errorStr = String::ToString("PlayerData: Mounted image %d loading failed, shape \"%s\" is not found.",i,mShapeFP[i].getPath().getFullPath().c_str());
               return false;
            }

            if(server)
               mCRCFP[i] = fileRef->getChecksum();
            else if(mCRCFP[i] != fileRef->getChecksum())
            {
               errorStr = String::ToString("PlayerData: Mounted image %d shape \"%s\" does not match version on server.",i,shapeNameFP[i]);
               return false;
            }
         }

         mValidShapeFP[i] = true;
      }
   }

   return true;
}

void PlayerData::getGroundInfo(TSShapeInstance* si, TSThread* thread,ActionAnimation *dp)
{
   dp->death = !dStrnicmp(dp->name, "death", 5);
   if (dp->death)
   {
      // Death animations use roll frame-to-frame changes in ground transform into position
      dp->speed = 0.0f;
      dp->dir.set(0.0f, 0.0f, 0.0f);

      // Death animations MUST define ground transforms, so add dummy ones if required
      if (si->getShape()->sequences[dp->sequence].numGroundFrames == 0)
         si->getShape()->setSequenceGroundSpeed(dp->name, Point3F(0, 0, 0), Point3F(0, 0, 0));
   }
   else
   {
      VectorF save = dp->dir;
      si->setSequence(thread,dp->sequence,0);
      si->animate();
      si->advanceTime(1);
      si->animateGround();
      si->getGroundTransform().getColumn(3,&dp->dir);
      if ((dp->speed = dp->dir.len()) < 0.01f)
      {
         // No ground displacement... In this case we'll use the
         // default table entry, if there is one.
         if (save.len() > 0.01f)
         {
            dp->dir = save;
            dp->speed = 1.0f;
            dp->velocityScale = false;
         }
         else
            dp->speed = 0.0f;
      }
      else
         dp->dir *= 1.0f / dp->speed;
   }
}

bool PlayerData::isTableSequence(S32 seq)
{
   // The sequences from the table must already have
   // been loaded for this to work.
   for (S32 i = 0; i < NumTableActionAnims; i++)
      if (actionList[i].sequence == seq)
         return true;
   return false;
}

bool PlayerData::isJumpAction(U32 action)
{
   return (action == JumpAnim || action == StandJumpAnim);
}

void PlayerData::initPersistFields()
{
   addField( "pickupRadius", TypeF32, Offset(pickupRadius, PlayerData),
      "@brief Radius around the player to collide with Items in the scene (on server).\n\n"
      "Internally the pickupRadius is added to the larger side of the initial bounding box "
      "to determine the actual distance, to a maximum of 2 times the bounding box size.  The "
      "initial bounding box is that used for the root pose, and therefore doesn't take into "
      "account the change in pose.\n");
   addField( "maxTimeScale", TypeF32, Offset(maxTimeScale, PlayerData),
      "@brief Maximum time scale for action animations.\n\n"
      "If an action animation has a defined ground frame, it is automatically scaled to match the "
      "player's ground velocity.  This field limits the maximum time scale used even if "
      "the player's velocity exceeds it." );

   addGroup( "Camera" );

      addField( "renderFirstPerson", TypeBool, Offset(renderFirstPerson, PlayerData),
         "@brief Flag controlling whether to render the player shape in first person view.\n\n" );

      addField( "firstPersonShadows", TypeBool, Offset(firstPersonShadows, PlayerData),
         "@brief Forces shadows to be rendered in first person when renderFirstPerson is disabled.  Defaults to false.\n\n" );

      addField( "minLookAngle", TypeF32, Offset(minLookAngle, PlayerData),
         "@brief Lowest angle (in radians) the player can look.\n\n"
         "@note An angle of zero is straight ahead, with positive up and negative down." );
      addField( "maxLookAngle", TypeF32, Offset(maxLookAngle, PlayerData),
         "@brief Highest angle (in radians) the player can look.\n\n"
         "@note An angle of zero is straight ahead, with positive up and negative down." );
      addField( "maxFreelookAngle", TypeF32, Offset(maxFreelookAngle, PlayerData),
         "@brief Defines the maximum left and right angles (in radians) the player can "
         "look in freelook mode.\n\n" );

   endGroup( "Camera" );

   addGroup( "Movement" );

      addField( "gravityMod", TypeF32, Offset(gravityMod, PlayerData),
         "@brief Gravity modifier.\n\n" );

      addField( "skidSpeed", TypeF32, Offset(skidSpeed, PlayerData),
         "@brief Velocity at which player starts skidding.\n\n" );

      addField( "skidFactor", TypeF32, Offset(skidFactor, PlayerData),
         "@brief FIXME: add description.\n\n" );

      addField( "maxStepHeight", TypeF32, Offset(maxStepHeight, PlayerData),
         "@brief Maximum height the player can step up.\n\n"
         "The player will automatically step onto changes in ground height less "
         "than maxStepHeight.  The player will collide with ground height changes "
         "greater than this." );

      addField( "runForce", TypeF32, Offset(runForce, PlayerData),
         "@brief Force used to accelerate the player when running.\n\n" );

      addField( "runEnergyDrain", TypeF32, Offset(runEnergyDrain, PlayerData),
         "@brief Energy value drained each tick that the player is moving.\n\n"
         "The player will not be able to move when his energy falls below "
         "minRunEnergy.\n"
         "@note Setting this to zero will disable any energy drain.\n"
         "@see minRunEnergy\n");
      addField( "minRunEnergy", TypeF32, Offset(minRunEnergy, PlayerData),
         "@brief Minimum energy level required to run or swim.\n\n"
         "@see runEnergyDrain\n");

      addField( "maxForwardSpeed", TypeF32, Offset(maxForwardSpeed, PlayerData),
         "@brief Maximum forward speed when running." );
      addField( "maxBackwardSpeed", TypeF32, Offset(maxBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when running." );
      addField( "maxSideSpeed", TypeF32, Offset(maxSideSpeed, PlayerData),
         "@brief Maximum sideways speed when running." );

      addField( "runSurfaceAngle", TypeF32, Offset(runSurfaceAngle, PlayerData),
         "@brief Maximum angle from vertical (in degrees) the player can run up.\n\n" );

      addField( "minImpactSpeed", TypeF32, Offset(minImpactSpeed, PlayerData),
         "@brief Minimum impact speed to apply falling damage.\n\n"
         "This field also sets the minimum speed for the onImpact callback "
         "to be invoked.\n"
         "@see ShapeBaseData::onImpact()\n");
      addField( "minLateralImpactSpeed", TypeF32, Offset(minLateralImpactSpeed, PlayerData),
         "@brief Minimum impact speed to apply non-falling damage.\n\n"
         "This field also sets the minimum speed for the onLateralImpact callback "
         "to be invoked.\n"
         "@see ShapeBaseData::onLateralImpact()\n");

      addField( "horizMaxSpeed", TypeF32, Offset(horizMaxSpeed, PlayerData),
         "@brief Maximum horizontal speed.\n\n"
         "@note This limit is only enforced if the player's horizontal speed "
         "exceeds horizResistSpeed.\n"
         "@see horizResistSpeed\n"
         "@see horizResistFactor\n" );
      addField( "horizResistSpeed", TypeF32, Offset(horizResistSpeed, PlayerData),
         "@brief Horizontal speed at which resistence will take place.\n\n"
         "@see horizMaxSpeed\n"
         "@see horizResistFactor\n" );
      addField( "horizResistFactor", TypeF32, Offset(horizResistFactor, PlayerData),
         "@brief Factor of resistence once horizResistSpeed has been reached.\n\n"
         "@see horizMaxSpeed\n"
         "@see horizResistSpeed\n" );

      addField( "upMaxSpeed", TypeF32, Offset(upMaxSpeed, PlayerData),
         "@brief Maximum upwards speed.\n\n"
         "@note This limit is only enforced if the player's upward speed exceeds "
         "upResistSpeed.\n"
         "@see upResistSpeed\n"
         "@see upResistFactor\n" );
      addField( "upResistSpeed", TypeF32, Offset(upResistSpeed, PlayerData),
         "@brief Upwards speed at which resistence will take place.\n\n"
         "@see upMaxSpeed\n"
         "@see upResistFactor\n" );
      addField( "upResistFactor", TypeF32, Offset(upResistFactor, PlayerData),
         "@brief Factor of resistence once upResistSpeed has been reached.\n\n"
         "@see upMaxSpeed\n"
         "@see upResistSpeed\n" );

   endGroup( "Movement" );

   addGroup( "Movement: Sliding" );

      addField( "slideForce", TypeF32, Offset(slideForce, PlayerData),
         "@brief Force used to accelerate the player when sliding.\n\n" );
      addField( "slideEnergyDrain", TypeF32, Offset(slideEnergyDrain, PlayerData),
         "@brief Energy value drained each tick that the player is sliding.\n\n"
         "The player will not be able to slide when his energy falls below "
         "minSlideEnergy.\n"
         "@note Setting this to zero will disable any energy drain.\n"
         "@see minSlideEnergy\n");
      addField( "slideEnergySlot", TypeS32, Offset(slideEnergySlot, PlayerData),
         "@brief Energy slot used by slideEnergyDrain.\n\n" );
      addField( "minSlideEnergy", TypeF32, Offset(minSlideEnergy, PlayerData),
         "@brief Minimum energy level required to slide.\n\n"
         "@see slideEnergyDrain\n");
      addField( "maxSlideForwardSpeed", TypeF32, Offset(maxSlideForwardSpeed, PlayerData),
         "@brief Maximum forward speed when sliding." );
      addField( "maxSlideBackwardSpeed", TypeF32, Offset(maxSlideBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when sliding." );
      addField( "maxSlideSideSpeed", TypeF32, Offset(maxSlideSideSpeed, PlayerData),
         "@brief Maximum sideways speed when sliding." );

   endGroup( "Movement: Sliding" );
   
   addGroup( "Movement: Jumping" );

      addField( "jumpForce", TypeF32, Offset(jumpForce, PlayerData),
         "@brief Force used to accelerate the player when a jump is initiated.\n\n" );

      addField( "jumpEnergyDrain", TypeF32, Offset(jumpEnergyDrain, PlayerData),
         "@brief Energy level drained each time the player jumps.\n\n"
         "@note Setting this to zero will disable any energy drain\n"
         "@see minJumpEnergy\n");
      addField( "minJumpEnergy", TypeF32, Offset(minJumpEnergy, PlayerData),
         "@brief Minimum energy level required to jump.\n\n"
         "@see jumpEnergyDrain\n");

      addField( "minJumpSpeed", TypeF32, Offset(minJumpSpeed, PlayerData),
         "@brief Minimum speed needed to jump.\n\n"
         "If the player's own z velocity is greater than this, then it is used to scale "
         "the jump speed, up to maxJumpSpeed.\n"
         "@see maxJumpSpeed\n");
      addField( "maxJumpSpeed", TypeF32, Offset(maxJumpSpeed, PlayerData),
         "@brief Maximum vertical speed before the player can no longer jump.\n\n" );
      addField( "jumpSurfaceAngle", TypeF32, Offset(jumpSurfaceAngle, PlayerData),
         "@brief Angle from vertical (in degrees) where the player can jump.\n\n" );
      addField( "jumpDelay", TypeS32, Offset(jumpDelay, PlayerData),
         "@brief Delay time in number of ticks ticks between jumps.\n\n" );
      addField( "glideForce", TypeF32, Offset(glideForce, PlayerData),
         "@brief Force used to accel player while in air and not sliding.\n\n");
      addField( "airControl", TypeF32, Offset(airControl, PlayerData),
         "@brief Amount of movement control the player has when in the air.\n\n"
			"Only used when glideForce is 0.\n"
         "This is applied as a multiplier to the player's x and y motion.\n");
      addField( "jumpTowardsNormal", TypeBool, Offset(jumpTowardsNormal, PlayerData),
         "@brief Controls the direction of the jump impulse.\n"
         "When false, jumps are always in the vertical (+Z) direction. When true "
         "jumps are in the direction of the ground normal so long as the player is not "
         "directly facing the surface.  If the player is directly facing the surface, then "
         "they will jump straight up.\n" );
   
   endGroup( "Movement: Jumping" );

   addGroup( "Movement: xJump" );

      addField( "xJumpEnergySlot", TypeS32, Offset(xJumpEnergySlot, PlayerData),
         "@brief Energy slot used to power xJump.\n\n" );
      addField( "xJumpChargeRate", TypeF32, Offset(xJumpChargeRate, PlayerData),
         "@brief How fast xJump charges.\n\n" );
      addField( "xJumpSpeedFactorD", TypeF32, Offset(xJumpSpeedFactorD, PlayerData),
         "@brief xJump speed factor divisor.\n\n" );
      addField( "xJumpPowerBaseConst", TypeF32, Offset(xJumpPowerBaseConst, PlayerData),
         "@brief xJump power base constant.\n\n" );
      addField( "xJumpPowerMultConst", TypeF32, Offset(xJumpPowerMultConst, PlayerData),
         "@brief xJump power multiplier constant.\n\n" );
      addField( "xJumpInstantEnergy", TypeF32, Offset(xJumpInstantEnergy, PlayerData),
         "@brief Energy required for instant xJump.\n\n" );

   endGroup( "Movement: xJump" );
   
   addGroup( "Movement: Sprinting" );

      addField( "sprintForce", TypeF32, Offset(sprintForce, PlayerData),
         "@brief Force used to accelerate the player when sprinting.\n\n" );

      addField( "sprintEnergyDrain", TypeF32, Offset(sprintEnergyDrain, PlayerData),
         "@brief Energy value drained each tick that the player is sprinting.\n\n"
         "The player will not be able to move when his energy falls below "
         "sprintEnergyDrain.\n"
         "@note Setting this to zero will disable any energy drain.\n"
         "@see minSprintEnergy\n");
      addField( "minSprintEnergy", TypeF32, Offset(minSprintEnergy, PlayerData),
         "@brief Minimum energy level required to sprint.\n\n"
         "@see sprintEnergyDrain\n");

      addField( "maxSprintForwardSpeed", TypeF32, Offset(maxSprintForwardSpeed, PlayerData),
         "@brief Maximum forward speed when sprinting." );
      addField( "maxSprintBackwardSpeed", TypeF32, Offset(maxSprintBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when sprinting." );
      addField( "maxSprintSideSpeed", TypeF32, Offset(maxSprintSideSpeed, PlayerData),
         "@brief Maximum sideways speed when sprinting." );

      addField( "sprintStrafeScale", TypeF32, Offset(sprintStrafeScale, PlayerData),
         "@brief Amount to scale strafing motion vector while sprinting." );
      addField( "sprintYawScale", TypeF32, Offset(sprintYawScale, PlayerData),
         "@brief Amount to scale yaw motion while sprinting." );
      addField( "sprintPitchScale", TypeF32, Offset(sprintPitchScale, PlayerData),
         "@brief Amount to scale pitch motion while sprinting." );

      addField( "sprintCanJump", TypeBool, Offset(sprintCanJump, PlayerData),
         "@brief Can the player jump while sprinting." );

   endGroup( "Movement: Sprinting" );

   addGroup( "Movement: Swimming" );

      addField( "swimForce", TypeF32, Offset(swimForce, PlayerData),
         "@brief Force used to accelerate the player when swimming.\n\n" );
      addField( "maxUnderwaterForwardSpeed", TypeF32, Offset(maxUnderwaterForwardSpeed, PlayerData),
         "@brief Maximum forward speed when underwater.\n\n" );
      addField( "maxUnderwaterBackwardSpeed", TypeF32, Offset(maxUnderwaterBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when underwater.\n\n" );
      addField( "maxUnderwaterSideSpeed", TypeF32, Offset(maxUnderwaterSideSpeed, PlayerData),
         "@brief Maximum sideways speed when underwater.\n\n" );

   endGroup( "Movement: Swimming" );

   addGroup( "Movement: Crouching" );

      addField( "crouchForce", TypeF32, Offset(crouchForce, PlayerData),
         "@brief Force used to accelerate the player when crouching.\n\n" );
      addField( "maxCrouchForwardSpeed", TypeF32, Offset(maxCrouchForwardSpeed, PlayerData),
         "@brief Maximum forward speed when crouching.\n\n" );
      addField( "maxCrouchBackwardSpeed", TypeF32, Offset(maxCrouchBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when crouching.\n\n" );
      addField( "maxCrouchSideSpeed", TypeF32, Offset(maxCrouchSideSpeed, PlayerData),
         "@brief Maximum sideways speed when crouching.\n\n" );

   endGroup( "Movement: Crouching" );

   addGroup( "Movement: Prone" );

      addField( "proneForce", TypeF32, Offset(proneForce, PlayerData),
         "@brief Force used to accelerate the player when prone (laying down).\n\n" );
      addField( "maxProneForwardSpeed", TypeF32, Offset(maxProneForwardSpeed, PlayerData),
         "@brief Maximum forward speed when prone (laying down).\n\n" );
      addField( "maxProneBackwardSpeed", TypeF32, Offset(maxProneBackwardSpeed, PlayerData),
         "@brief Maximum backward speed when prone (laying down).\n\n" );
      addField( "maxProneSideSpeed", TypeF32, Offset(maxProneSideSpeed, PlayerData),
         "@brief Maximum sideways speed when prone (laying down).\n\n" );

   endGroup( "Movement: Prone" );

   addGroup( "Movement: Jetting" );

      addField( "jetJumpForce", TypeF32, Offset(jetJumpForce, PlayerData),
         "@brief Force used to accelerate the player when a jet jump is initiated.\n\n" );

      addField( "jetJumpEnergyDrain", TypeF32, Offset(jetJumpEnergyDrain, PlayerData),
         "@brief Energy level drained each time the player jet jumps.\n\n"
         "@note Setting this to zero will disable any energy drain\n"
         "@see jetMinJumpEnergy\n");
      addField( "jetMinJumpEnergy", TypeF32, Offset(jetMinJumpEnergy, PlayerData),
         "@brief Minimum energy level required to jet jump.\n\n"
         "@see jetJumpEnergyDrain\n");

      addField( "jetMinJumpSpeed", TypeF32, Offset(jetMinJumpSpeed, PlayerData),
         "@brief Minimum speed needed to jet jump.\n\n"
         "If the player's own z velocity is greater than this, then it is used to scale "
         "the jet jump speed, up to jetMaxJumpSpeed.\n"
         "@see jetMaxJumpSpeed\n");
      addField( "jetMaxJumpSpeed", TypeF32, Offset(jetMaxJumpSpeed, PlayerData),
         "@brief Maximum vertical speed before the player can no longer jet jump.\n\n" );
      addField( "jetJumpSurfaceAngle", TypeF32, Offset(jetJumpSurfaceAngle, PlayerData),
         "@brief Angle from vertical (in degrees) where the player can jet jump.\n\n" );

   endGroup( "Movement: Jetting" );

   addGroup( "Falling" );

      addField( "fallingSpeedThreshold", TypeF32, Offset(fallingSpeedThreshold, PlayerData),
         "@brief Downward speed at which we consider the player falling.\n\n" );

      addField( "recoverDelay", TypeS32, Offset(recoverDelay, PlayerData),
         "@brief Number of ticks for the player to recover from falling.\n\n" );

      addField( "recoverRunForceScale", TypeF32, Offset(recoverRunForceScale, PlayerData),
         "@brief Scale factor applied to runForce while in the recover state.\n\n"
         "This can be used to temporarily slow the player's movement after a fall, or "
         "prevent the player from moving at all if set to zero.\n" );

      addField( "landSequenceTime", TypeF32, Offset(landSequenceTime, PlayerData),
         "@brief Time of land sequence play back when using new recover system.\n\n"
         "If greater than 0 then the legacy fall recovery system will be bypassed "
         "in favour of just playing the player's land sequence.  The time to "
         "recover from a fall then becomes this parameter's time and the land "
         "sequence's playback will be scaled to match.\n"
         "@see transitionToLand\n" );

      addField( "transitionToLand", TypeBool, Offset(transitionToLand, PlayerData),
         "@brief When going from a fall to a land, should we transition between the two.\n\n"
         "@note Only takes affect when landSequenceTime is greater than 0.\n"
         "@see landSequenceTime\n" );

   endGroup( "Falling" );

   addGroup( "Collision" );

      addField( "boundingBox", TypePoint3F, Offset(boxSize, PlayerData),
         "@brief Size of the bounding box used by the player for collision.\n\n"
         "Dimensions are given as \"width depth height\"." );
      addField( "crouchBoundingBox", TypePoint3F, Offset(crouchBoxSize, PlayerData),
         "@brief Collision bounding box used when the player is crouching.\n\n"
         "@see boundingBox" );
      addField( "proneBoundingBox", TypePoint3F, Offset(proneBoxSize, PlayerData),
         "@brief Collision bounding box used when the player is prone (laying down).\n\n"
         "@see boundingBox" );
      addField( "swimBoundingBox", TypePoint3F, Offset(swimBoxSize, PlayerData),
         "@brief Collision bounding box used when the player is swimming.\n\n"
         "@see boundingBox" );

      addField( "boxHeadPercentage", TypeF32, Offset(boxHeadPercentage, PlayerData),
         "@brief Percentage of the player's bounding box height that represents the head.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );
      addField( "boxTorsoPercentage", TypeF32, Offset(boxTorsoPercentage, PlayerData),
         "@brief Percentage of the player's bounding box height that represents the torso.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );
      addField( "boxHeadLeftPercentage", TypeF32, Offset(boxHeadLeftPercentage, PlayerData),
         "@brief Percentage of the player's bounding box width that represents the left side of the head.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );
      addField( "boxHeadRightPercentage", TypeF32, Offset(boxHeadRightPercentage, PlayerData),
         "@brief Percentage of the player's bounding box width that represents the right side of the head.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );
      addField( "boxHeadBackPercentage", TypeF32, Offset(boxHeadBackPercentage, PlayerData),
         "@brief Percentage of the player's bounding box depth that represents the back side of the head.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );
      addField( "boxHeadFrontPercentage", TypeF32, Offset(boxHeadFrontPercentage, PlayerData),
         "@brief Percentage of the player's bounding box depth that represents the front side of the head.\n\n"
         "Used when computing the damage location.\n"
         "@see Player::getDamageLocation" );

   endGroup( "Collision" );
   
   addGroup( "Interaction: Footsteps" );

      addField( "footPuffEmitter", TYPEID< ParticleEmitterData >(), Offset(footPuffEmitter, PlayerData),
         "@brief Particle emitter used to generate footpuffs (particles created as the player "
         "walks along the ground).\n\n"
         "@note The generation of foot puffs requires the appropriate triggeres to be defined in the "
         "player's animation sequences.  Without these, no foot puffs will be generated.\n");
      addField( "footPuffNumParts", TypeS32, Offset(footPuffNumParts, PlayerData),
         "@brief Number of footpuff particles to generate each step.\n\n"
         "Each foot puff is randomly placed within the defined foot puff radius.  This "
         "includes having footPuffNumParts set to one.\n"
         "@see footPuffRadius\n");
      addField( "footPuffRadius", TypeF32, Offset(footPuffRadius, PlayerData),
         "@brief Particle creation radius for footpuff particles.\n\n"
         "This is applied to each foot puff particle, even if footPuffNumParts is set to one.  So "
         "set this value to zero if you want a single foot puff placed at exactly the same location "
         "under the player each time.\n");
      addField( "dustEmitter", TYPEID< ParticleEmitterData >(), Offset(dustEmitter, PlayerData),
         "@brief Emitter used to generate dust particles.\n\n"
         "@note Currently unused." );

      addField( "decalData", TYPEID< DecalData >(), Offset(decalData, PlayerData),
         "@brief Decal to place on the ground for player footsteps.\n\n" );
      addField( "decalOffset",TypeF32, Offset(decalOffset, PlayerData),
         "@brief Distance from the center of the model to the right foot.\n\n"
         "While this defines the distance to the right foot, it is also used to place "
         "the left foot decal as well.  Just on the opposite side of the player." );

   endGroup( "Interaction: Footsteps" );

   addGroup( "Interaction: Sounds" );

      addField( "FootSoftSound", TypeSFXTrackName, Offset(sound[FootSoft], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 0.\n\n" );
      addField( "FootHardSound", TypeSFXTrackName, Offset(sound[FootHard], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 1.\n\n" );
      addField( "FootMetalSound", TypeSFXTrackName, Offset(sound[FootMetal], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 2.\n\n" );
      addField( "FootSnowSound", TypeSFXTrackName, Offset(sound[FootSnow], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 3.\n\n" );

      addField( "FootShallowSound", TypeSFXTrackName, Offset(sound[FootShallowSplash], PlayerData),
         "@brief Sound to play when walking in water and coverage is less than "
         "footSplashHeight.\n\n"
         "@see footSplashHeight\n" );
      addField( "FootWadingSound", TypeSFXTrackName, Offset(sound[FootWading], PlayerData),
         "@brief Sound to play when walking in water and coverage is less than 1, "
         "but > footSplashHeight.\n\n"
         "@see footSplashHeight\n" );
      addField( "FootUnderwaterSound", TypeSFXTrackName, Offset(sound[FootUnderWater], PlayerData),
         "@brief Sound to play when walking in water and coverage equals 1.0 "
         "(fully underwater).\n\n" );
      addField( "FootBubblesSound", TypeSFXTrackName, Offset(sound[FootBubbles], PlayerData),
         "@brief Sound to play when walking in water and coverage equals 1.0 "
         "(fully underwater).\n\n" );

      addField( "OtherFootSoftSound", TypeSFXTrackName, Offset(sound[OtherFootSoft], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 0.\n\n" );
      addField( "OtherFootHardSound", TypeSFXTrackName, Offset(sound[OtherFootHard], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 1.\n\n" );
      addField( "OtherFootMetalSound", TypeSFXTrackName, Offset(sound[OtherFootMetal], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 2.\n\n" );
      addField( "OtherFootSnowSound", TypeSFXTrackName, Offset(sound[OtherFootSnow], PlayerData),
         "@brief Sound to play when walking on a surface with Material footstepSoundId 3.\n\n" );
      addField( "OtherFootShallowSound", TypeSFXTrackName, Offset(sound[OtherFootShallowSplash], PlayerData),
         "@brief Sound to play when walking in water and coverage is less than "
         "footSplashHeight.\n\n"
         "@see footSplashHeight\n" );
      addField( "OtherFootWadingSound", TypeSFXTrackName, Offset(sound[OtherFootWading], PlayerData),
         "@brief Sound to play when walking in water and coverage is less than 1, "
         "but > footSplashHeight.\n\n"
         "@see footSplashHeight\n" );
      addField( "OtherFootUnderwaterSound", TypeSFXTrackName, Offset(sound[OtherFootUnderWater], PlayerData),
         "@brief Sound to play when walking in water and coverage equals 1.0 "
         "(fully underwater).\n\n" );
      addField( "OtherFootBubblesSound", TypeSFXTrackName, Offset(sound[OtherFootBubbles], PlayerData),
         "@brief Sound to play when walking in water and coverage equals 1.0 "
         "(fully underwater).\n\n" );

      addField( "movingBubblesSound", TypeSFXTrackName, Offset(sound[MoveBubbles], PlayerData),
         "@brief Sound to play when in water and coverage equals 1.0 (fully underwater).\n\n"
         "Note that unlike FootUnderwaterSound, this sound plays even if the "
         "player is not moving around in the water.\n" );
      addField( "waterBreathSound", TypeSFXTrackName, Offset(sound[WaterBreath], PlayerData),
         "@brief Sound to play when in water and coverage equals 1.0 (fully underwater).\n\n"
         "Note that unlike FootUnderwaterSound, this sound plays even if the "
         "player is not moving around in the water.\n" );

      addField( "impactSoftSound", TypeSFXTrackName, Offset(sound[ImpactSoft], PlayerData),
         "@brief Sound to play after falling on a surface with Material footstepSoundId 0.\n\n" );
      addField( "impactHardSound", TypeSFXTrackName, Offset(sound[ImpactHard], PlayerData),
         "@brief Sound to play after falling on a surface with Material footstepSoundId 1.\n\n" );
      addField( "impactMetalSound", TypeSFXTrackName, Offset(sound[ImpactMetal], PlayerData),
         "@brief Sound to play after falling on a surface with Material footstepSoundId 2.\n\n" );
      addField( "impactSnowSound", TypeSFXTrackName, Offset(sound[ImpactSnow], PlayerData),
         "@brief Sound to play after falling on a surface with Material footstepSoundId 3.\n\n" );

      addField( "impactWaterEasy", TypeSFXTrackName, Offset(sound[ImpactWaterEasy], PlayerData),
         "@brief Sound to play when entering the water with velocity < "
         "mediumSplashSoundVelocity.\n\n"
         "@see mediumSplashSoundVelocity\n");
      addField( "impactWaterMedium", TypeSFXTrackName, Offset(sound[ImpactWaterMedium], PlayerData),
         "@brief Sound to play when entering the water with velocity >= "
         "mediumSplashSoundVelocity and < hardSplashSoundVelocity.\n\n"
         "@see mediumSplashSoundVelocity\n"
         "@see hardSplashSoundVelocity\n");
      addField( "impactWaterHard", TypeSFXTrackName, Offset(sound[ImpactWaterHard], PlayerData),
         "@brief Sound to play when entering the water with velocity >= "
         "hardSplashSoundVelocity.\n\n"
         "@see hardSplashSoundVelocity\n");
      addField( "exitingWater", TypeSFXTrackName, Offset(sound[ExitWater], PlayerData),
         "@brief Sound to play when exiting the water with velocity >= exitSplashSoundVelocity.\n\n"
         "@see exitSplashSoundVelocity\n");

      addField( "slideSound", TypeSFXTrackName, Offset(sound[Slide], PlayerData),
         "@brief Sound to play while sliding.\n\n" );
      addField( "slideContactSound", TypeSFXTrackName, Offset(sound[SlideContact], PlayerData),
         "@brief Sound to play while sliding and being in contact with a surface.\n\n" );

      addField( "skidSound", TypeSFXTrackName, Offset(sound[Skid], PlayerData),
         "@brief Sound to play while skidding.\n\n" );

      addField( "xJumpSound", TypeSFXTrackName, Offset(sound[XJump], PlayerData),
         "@brief Sound to play when performing an x-jump.\n\n" );
      addField( "xJumpChargeSound", TypeSFXTrackName, Offset(sound[XJumpCharge], PlayerData),
         "@brief Sound to play while charging an x-jump.\n\n" );

   endGroup( "Interaction: Sounds" );

   addGroup( "Interaction: Splashes" );

      addField( "splash", TYPEID< SplashData >(), Offset(splash, PlayerData),
         "@brief SplashData datablock used to create splashes when the player moves "
         "through water.\n\n" );
      addField( "splashVelocity", TypeF32, Offset(splashVelocity, PlayerData),
         "@brief Minimum velocity when moving through water to generate splashes.\n\n" );
      addField( "splashAngle", TypeF32, Offset(splashAngle, PlayerData),
         "@brief Maximum angle (in degrees) from pure vertical movement in water to "
         "generate splashes.\n\n" );

      addField( "splashFreqMod", TypeF32, Offset(splashFreqMod, PlayerData),
         "@brief Multipled by speed to determine the number of splash particles to generate.\n\n" );
      addField( "splashVelEpsilon", TypeF32, Offset(splashVelEpsilon, PlayerData),
         "@brief Minimum speed to generate splash particles.\n\n" );
      addField( "bubbleEmitTime", TypeF32, Offset(bubbleEmitTime, PlayerData),
         "@brief Time in seconds to generate bubble particles after entering the water.\n\n" );
      addField( "splashEmitter", TYPEID< ParticleEmitterData >(), Offset(splashEmitterList, PlayerData), NUM_SPLASH_EMITTERS,
         "@brief Particle emitters used to generate splash particles.\n\n" );

      addField( "footstepSplashHeight", TypeF32, Offset(footSplashHeight, PlayerData),
         "@brief Water coverage level to choose between FootShallowSound and FootWadingSound.\n\n"
         "@see FootShallowSound\n"
         "@see FootWadingSound\n");

      addField( "mediumSplashSoundVelocity", TypeF32, Offset(medSplashSoundVel, PlayerData),
         "@brief Minimum velocity when entering the water for choosing between the impactWaterEasy and "
         "impactWaterMedium sounds to play.\n\n"
         "@see impactWaterEasy\n"
         "@see impactWaterMedium\n" );
      addField( "hardSplashSoundVelocity", TypeF32, Offset(hardSplashSoundVel, PlayerData),
         "@brief Minimum velocity when entering the water for choosing between the impactWaterMedium and "
         "impactWaterHard sound to play.\n\n"
         "@see impactWaterMedium\n"
         "@see impactWaterHard\n" );
      addField( "exitSplashSoundVelocity", TypeF32, Offset(exitSplashSoundVel, PlayerData),
         "@brief Minimum velocity when leaving the water for the exitingWater sound to "
         "play.\n\n"
         "@see exitingWater");

   endGroup( "Interaction: Splashes" );

   addGroup( "Interaction: Ground Impact" );

      addField( "groundImpactMinSpeed", TypeF32, Offset(groundImpactMinSpeed, PlayerData),
         "@brief Minimum falling impact speed to apply damage and initiate the camera "
         "shaking effect.\n\n" );
      addField( "groundImpactShakeFreq", TypePoint3F, Offset(groundImpactShakeFreq, PlayerData),
         "@brief Frequency of the camera shake effect after falling.\n\n"
         "This is how fast to shake the camera.\n");
      addField( "groundImpactShakeAmp", TypePoint3F, Offset(groundImpactShakeAmp, PlayerData),
         "@brief Amplitude of the camera shake effect after falling.\n\n"
         "This is how much to shake the camera.\n");
      addField( "groundImpactShakeDuration", TypeF32, Offset(groundImpactShakeDuration, PlayerData),
         "@brief Duration (in seconds) of the camera shake effect after falling.\n\n"
         "This is how long to shake the camera.\n");
      addField( "groundImpactShakeFalloff", TypeF32, Offset(groundImpactShakeFalloff, PlayerData),
         "@brief Falloff factor of the camera shake effect after falling.\n\n"
         "This is how to fade the camera shake over the duration.\n");

   endGroup( "Interaction: Ground Impact" );

   addGroup( "Interaction: Sliding" );

      addField( "slideParticleTrailEmitter", TYPEID< ParticleEmitterData >(), Offset(slideEmitter[PlayerData::Trail1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );
      addField( "slideContactParticleTrailEmitter", TYPEID< ParticleEmitterData >(), Offset(slideEmitter[PlayerData::ContactTrail1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );
      addField( "slideParticleFootEmitter", TYPEID< ParticleEmitterData >(), Offset(slideEmitter[PlayerData::Relative1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );
      addField( "slideContactParticleFootEmitter", TYPEID< ParticleEmitterData >(), Offset(slideEmitter[PlayerData::ContactRelative1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );

      addField( "slideDecal", TYPEID< DecalData >(), Offset(slideDecal, PlayerData),
         "@brief Decal to place on the ground when sliding.\n\n" );

	endGroup( "Interaction: Sliding" );

   addGroup( "Interaction: Skidding" );

      addField( "skidParticleTrailEmitter", TYPEID< ParticleEmitterData >(), Offset(skidEmitter[PlayerData::SkidTrail1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );
      addField( "skidParticleFootEmitter", TYPEID< ParticleEmitterData >(), Offset(skidEmitter[PlayerData::SkidRelative1], PlayerData), 3,
         "@brief Particle emitters.\n\n" );

      addField( "skidDecal", TYPEID< DecalData >(), Offset(skidDecal, PlayerData),
         "@brief Decal to place on the ground when skidding.\n\n" );

	endGroup( "Interaction: Skidding" );

   addGroup( "Physics" );

      // PhysicsPlayer
      addField( "physicsPlayerType", TypeString, Offset(physicsPlayerType, PlayerData),
         "@brief Specifies the type of physics used by the player.\n\n"
         "This depends on the physics module used.  An example is 'Capsule'.\n"
         "@note Not current used.\n");

   endGroup( "Physics" );

   addGroup( "First Person Arms" );

      addField( "imageAnimPrefixFP", TypeCaseString, Offset(imageAnimPrefixFP, PlayerData),
         "@brief Optional prefix to all mounted image animation sequences in first person.\n\n"
         "This defines a prefix that will be added when looking up mounted image "
         "animation sequences while in first person.  It allows for the customization "
         "of a first person image based on the type of player.\n");

      // Mounted images arrays
      addArray( "Mounted Images", ShapeBase::MaxMountedImages );

         addField( "shapeNameFP", TypeShapeFilename, Offset(shapeNameFP, PlayerData), ShapeBase::MaxMountedImages,
            "@brief File name of this player's shape that will be used in conjunction with the corresponding mounted image.\n\n"
            "These optional parameters correspond to each mounted image slot to indicate a shape that is rendered "
            "in addition to the mounted image shape.  Typically these are a player's arms (or arm) that is "
            "animated along with the mounted image's state animation sequences.\n");

      endArray( "Mounted Images" );

   endGroup( "First Person Arms" );

   addGroup( "Third Person" );

      addField( "imageAnimPrefix", TypeCaseString, Offset(imageAnimPrefix, PlayerData),
         "@brief Optional prefix to all mounted image animation sequences in third person.\n\n"
         "This defines a prefix that will be added when looking up mounted image "
         "animation sequences while in third person.  It allows for the customization "
         "of a third person image based on the type of player.\n");

      addField( "allowImageStateAnimation", TypeBool, Offset(allowImageStateAnimation, PlayerData),
         "@brief Allow mounted images to request a sequence be played on the Player.\n\n"
         "When true a new thread is added to the player to allow for "
         "mounted images to request a sequence be played on the player "
         "through the image's state machine.  It is only optional so "
         "that we don't create a TSThread on the player if we don't "
         "need to.\n");

   endGroup( "Third Person" );

   Parent::initPersistFields();
}

void PlayerData::packData(BitStream* stream)
{
   Parent::packData(stream);

   stream->writeFlag(renderFirstPerson);
   stream->writeFlag(firstPersonShadows);
   
   stream->write(minLookAngle);
   stream->write(maxLookAngle);
   stream->write(maxFreelookAngle);
   stream->write(maxTimeScale);

   stream->write(mass);
   for(U32 i = 0; i < ShapeBaseData::MaxEnergySlots; i++)
      stream->write(maxEnergy[i]);
   stream->write(drag);
   stream->write(density);

   stream->write(gravityMod);

   stream->write(skidSpeed);
   stream->write(skidFactor);

   stream->write(maxStepHeight);

   stream->write(runForce);
   stream->write(runEnergyDrain);
   stream->write(minRunEnergy);
   stream->write(maxForwardSpeed);
   stream->write(maxBackwardSpeed);
   stream->write(maxSideSpeed);
   stream->write(runSurfaceAngle);

   stream->write(fallingSpeedThreshold);

   stream->write(recoverDelay);
   stream->write(recoverRunForceScale);
   stream->write(landSequenceTime);
   stream->write(transitionToLand);

	// Sliding
   stream->write(slideForce);
   stream->write(slideEnergyDrain);
   stream->write(slideEnergySlot);
   stream->write(minSlideEnergy);
	stream->write(maxSlideForwardSpeed);
	stream->write(maxSlideBackwardSpeed);
	stream->write(maxSlideSideSpeed);

   // Jumping
   stream->write(jumpForce);
   stream->write(jumpEnergyDrain);
   stream->write(minJumpEnergy);
   stream->write(minJumpSpeed);
   stream->write(maxJumpSpeed);
   stream->write(jumpSurfaceAngle);
   stream->writeInt(jumpDelay,JumpDelayBits);

   // xJump
   stream->write(xJumpEnergySlot);
   stream->write(xJumpChargeRate);
   stream->write(xJumpSpeedFactorD);
   stream->write(xJumpPowerBaseConst);
   stream->write(xJumpPowerMultConst);
   stream->write(xJumpInstantEnergy);

   // Sprinting
   stream->write(sprintForce);
   stream->write(sprintEnergyDrain);
   stream->write(minSprintEnergy);
   stream->write(maxSprintForwardSpeed);
   stream->write(maxSprintBackwardSpeed);
   stream->write(maxSprintSideSpeed);
   stream->write(sprintStrafeScale);
   stream->write(sprintYawScale);
   stream->write(sprintPitchScale);
   stream->writeFlag(sprintCanJump);

   // Swimming
   stream->write(swimForce);   
   stream->write(maxUnderwaterForwardSpeed);
   stream->write(maxUnderwaterBackwardSpeed);
   stream->write(maxUnderwaterSideSpeed);

   // Crouching
   stream->write(crouchForce);   
   stream->write(maxCrouchForwardSpeed);
   stream->write(maxCrouchBackwardSpeed);
   stream->write(maxCrouchSideSpeed);

   // Prone
   stream->write(proneForce);   
   stream->write(maxProneForwardSpeed);
   stream->write(maxProneBackwardSpeed);
   stream->write(maxProneSideSpeed);

   // Jetting
   stream->write(jetJumpForce);
   stream->write(jetJumpEnergyDrain);
   stream->write(jetMinJumpEnergy);
   stream->write(jetMinJumpSpeed);
   stream->write(jetMaxJumpSpeed);
   stream->write(jetJumpSurfaceAngle);

   stream->write(horizMaxSpeed);
   stream->write(horizResistSpeed);
   stream->write(horizResistFactor);

   stream->write(upMaxSpeed);
   stream->write(upResistSpeed);
   stream->write(upResistFactor);

   stream->write(splashVelocity);
   stream->write(splashAngle);
   stream->write(splashFreqMod);
   stream->write(splashVelEpsilon);
   stream->write(bubbleEmitTime);

   stream->write(medSplashSoundVel);
   stream->write(hardSplashSoundVel);
   stream->write(exitSplashSoundVel);
   stream->write(footSplashHeight);
   // Don't need damage scale on the client
   stream->write(minImpactSpeed);
   stream->write(minLateralImpactSpeed);

   for( U32 i = 0; i < MaxSounds; i++)
      sfxWrite( stream, sound[ i ] );

   mathWrite(*stream, boxSize);
   mathWrite(*stream, crouchBoxSize);
   mathWrite(*stream, proneBoxSize);
   mathWrite(*stream, swimBoxSize);

   if( stream->writeFlag( footPuffEmitter ) )
   {
      stream->writeRangedU32( footPuffEmitter->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
   }

   stream->write( footPuffNumParts );
   stream->write( footPuffRadius );

   for(U32 i = 0; i < NumSlideEmitters; i++)
      if (stream->writeFlag(slideEmitter[i]))
         stream->writeRangedU32(packed? SimObjectId(slideEmitter[i]):
            slideEmitter[i]->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);

   for(U32 i = 0; i < NumSkidEmitters; i++)
      if (stream->writeFlag(skidEmitter[i]))
         stream->writeRangedU32(packed? SimObjectId(skidEmitter[i]):
            skidEmitter[i]->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);

   if( stream->writeFlag( decalData ) )
   {
      stream->writeRangedU32( decalData->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
   }
   stream->write(decalOffset);

   if( stream->writeFlag( slideDecal ) )
   {
      stream->writeRangedU32( slideDecal->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
   }

   if( stream->writeFlag( skidDecal ) )
   {
      stream->writeRangedU32( skidDecal->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
   }

   if( stream->writeFlag( dustEmitter ) )
   {
      stream->writeRangedU32( dustEmitter->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
   }


   if (stream->writeFlag( splash ))
   {
      stream->writeRangedU32(splash->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   for( U32 i=0; i<NUM_SPLASH_EMITTERS; i++ )
   {
      if( stream->writeFlag( splashEmitterList[i] != NULL ) )
      {
         stream->writeRangedU32( splashEmitterList[i]->getId(), DataBlockObjectIdFirst,  DataBlockObjectIdLast );
      }
   }


   stream->write(groundImpactMinSpeed);
   stream->write(groundImpactShakeFreq.x);
   stream->write(groundImpactShakeFreq.y);
   stream->write(groundImpactShakeFreq.z);
   stream->write(groundImpactShakeAmp.x);
   stream->write(groundImpactShakeAmp.y);
   stream->write(groundImpactShakeAmp.z);
   stream->write(groundImpactShakeDuration);
   stream->write(groundImpactShakeFalloff);

   // Air control
	stream->write(glideForce);
   stream->write(airControl);

   // Jump off at normal
   stream->writeFlag(jumpTowardsNormal);

   stream->writeString(physicsPlayerType);

   // Third person mounted image shapes
   stream->writeString(imageAnimPrefix);

   stream->writeFlag(allowImageStateAnimation);

   // First person mounted image shapes
   stream->writeString(imageAnimPrefixFP);
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      stream->writeString(shapeNameFP[i]);

      // computeCRC is handled in ShapeBaseData
      if (computeCRC)
      {
         stream->write(mCRCFP[i]);
      }
   }
}

void PlayerData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);

   renderFirstPerson = stream->readFlag();
   firstPersonShadows = stream->readFlag();

   stream->read(&minLookAngle);
   stream->read(&maxLookAngle);
   stream->read(&maxFreelookAngle);
   stream->read(&maxTimeScale);

   stream->read(&mass);
   for(U32 i = 0; i < ShapeBaseData::MaxEnergySlots; i++)
      stream->read(&maxEnergy[i]);
   stream->read(&drag);
   stream->read(&density);

   stream->read(&gravityMod);

   stream->read(&skidSpeed);
   stream->read(&skidFactor);

   stream->read(&maxStepHeight);

   stream->read(&runForce);
   stream->read(&runEnergyDrain);
   stream->read(&minRunEnergy);
   stream->read(&maxForwardSpeed);
   stream->read(&maxBackwardSpeed);
   stream->read(&maxSideSpeed);
   stream->read(&runSurfaceAngle);

   stream->read(&fallingSpeedThreshold);
   stream->read(&recoverDelay);
   stream->read(&recoverRunForceScale);
   stream->read(&landSequenceTime);
   stream->read(&transitionToLand);

	// Sliding
   stream->read(&slideForce);
   stream->read(&slideEnergyDrain);
   stream->read(&slideEnergySlot);
   stream->read(&minSlideEnergy);
   stream->read(&maxSlideForwardSpeed);
   stream->read(&maxSlideBackwardSpeed);
   stream->read(&maxSlideSideSpeed);

   // Jumping
   stream->read(&jumpForce);
   stream->read(&jumpEnergyDrain);
   stream->read(&minJumpEnergy);
   stream->read(&minJumpSpeed);
   stream->read(&maxJumpSpeed);
   stream->read(&jumpSurfaceAngle);
   jumpDelay = stream->readInt(JumpDelayBits);

   // xJump
   stream->read(&xJumpEnergySlot);
   stream->read(&xJumpChargeRate);
   stream->read(&xJumpSpeedFactorD);
   stream->read(&xJumpPowerBaseConst);
   stream->read(&xJumpPowerMultConst);
   stream->read(&xJumpInstantEnergy);

   // Sprinting
   stream->read(&sprintForce);
   stream->read(&sprintEnergyDrain);
   stream->read(&minSprintEnergy);
   stream->read(&maxSprintForwardSpeed);
   stream->read(&maxSprintBackwardSpeed);
   stream->read(&maxSprintSideSpeed);
   stream->read(&sprintStrafeScale);
   stream->read(&sprintYawScale);
   stream->read(&sprintPitchScale);
   sprintCanJump = stream->readFlag();

   // Swimming
   stream->read(&swimForce);
   stream->read(&maxUnderwaterForwardSpeed);
   stream->read(&maxUnderwaterBackwardSpeed);
   stream->read(&maxUnderwaterSideSpeed);   

   // Crouching
   stream->read(&crouchForce);
   stream->read(&maxCrouchForwardSpeed);
   stream->read(&maxCrouchBackwardSpeed);
   stream->read(&maxCrouchSideSpeed);

   // Prone
   stream->read(&proneForce);
   stream->read(&maxProneForwardSpeed);
   stream->read(&maxProneBackwardSpeed);
   stream->read(&maxProneSideSpeed);

   // Jetting
   stream->read(&jetJumpForce);
   stream->read(&jetJumpEnergyDrain);
   stream->read(&jetMinJumpEnergy);
   stream->read(&jetMinJumpSpeed);
   stream->read(&jetMaxJumpSpeed);
   stream->read(&jetJumpSurfaceAngle);

   stream->read(&horizMaxSpeed);
   stream->read(&horizResistSpeed);
   stream->read(&horizResistFactor);

   stream->read(&upMaxSpeed);
   stream->read(&upResistSpeed);
   stream->read(&upResistFactor);

   stream->read(&splashVelocity);
   stream->read(&splashAngle);
   stream->read(&splashFreqMod);
   stream->read(&splashVelEpsilon);
   stream->read(&bubbleEmitTime);

   stream->read(&medSplashSoundVel);
   stream->read(&hardSplashSoundVel);
   stream->read(&exitSplashSoundVel);
   stream->read(&footSplashHeight);

   stream->read(&minImpactSpeed);
   stream->read(&minLateralImpactSpeed);

   for( U32 i = 0; i < MaxSounds; i++)
      sfxRead( stream, &sound[ i ] );

   mathRead(*stream, &boxSize);
   mathRead(*stream, &crouchBoxSize);
   mathRead(*stream, &proneBoxSize);
   mathRead(*stream, &swimBoxSize);

   if( stream->readFlag() )
   {
      footPuffID = (S32) stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   stream->read(&footPuffNumParts);
   stream->read(&footPuffRadius);

	for(U32 i = 0; i < NumSlideEmitters; i++) 
	{
		slideEmitter[i] = NULL;
		if(stream->readFlag())
			slideEmitter[i] = (ParticleEmitterData*)stream->readRangedU32(DataBlockObjectIdFirst,
                               DataBlockObjectIdLast);
   }

   for(U32 i = 0; i < NumSkidEmitters; i++) 
   {
      skidEmitter[i] = NULL;
      if(stream->readFlag())
         skidEmitter[i] = (ParticleEmitterData*)stream->readRangedU32(DataBlockObjectIdFirst,
                           DataBlockObjectIdLast);
   }

   if( stream->readFlag() )
   {
      decalID = (S32) stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }
   stream->read(&decalOffset);

   if( stream->readFlag() )
   {
      slideDecalID = (S32) stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   if( stream->readFlag() )
   {
      skidDecalID = (S32) stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   if( stream->readFlag() )
   {
      dustID = (S32) stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }


   if (stream->readFlag())
   {
      splashId = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
   }

   for( U32 i=0; i<NUM_SPLASH_EMITTERS; i++ )
   {
      if( stream->readFlag() )
      {
         splashEmitterIDList[i] = stream->readRangedU32( DataBlockObjectIdFirst, DataBlockObjectIdLast );
      }
   }

   stream->read(&groundImpactMinSpeed);
   stream->read(&groundImpactShakeFreq.x);
   stream->read(&groundImpactShakeFreq.y);
   stream->read(&groundImpactShakeFreq.z);
   stream->read(&groundImpactShakeAmp.x);
   stream->read(&groundImpactShakeAmp.y);
   stream->read(&groundImpactShakeAmp.z);
   stream->read(&groundImpactShakeDuration);
   stream->read(&groundImpactShakeFalloff);

   // Air control
	stream->read(&glideForce);
   stream->read(&airControl);

   // Jump off at normal
   jumpTowardsNormal = stream->readFlag();

   physicsPlayerType = stream->readSTString();

   // Third person mounted image shapes
   imageAnimPrefix = stream->readSTString();

   allowImageStateAnimation = stream->readFlag();

   // First person mounted image shapes
   imageAnimPrefixFP = stream->readSTString();
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      shapeNameFP[i] = stream->readSTString();

      // computeCRC is handled in ShapeBaseData
      if (computeCRC)
      {
         stream->read(&(mCRCFP[i]));
      }
   }
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

ImplementEnumType( PlayerPose,
   "@brief The pose of the Player.\n\n"
   "@ingroup gameObjects\n\n")
   { Player::StandPose,    "Stand",    "Standard movement pose.\n" },
   { Player::SprintPose,   "Sprint",   "Sprinting pose.\n" },
   { Player::CrouchPose,   "Crouch",   "Crouch pose.\n" },
   { Player::PronePose,    "Prone",    "Prone pose.\n" },
   { Player::SwimPose,     "Swim",     "Swimming pose.\n" },
EndImplementEnumType;

//----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(Player);

ConsoleDocClass( Player,
   "@ingroup gameObjects\n"
);

F32 Player::mGravity = -20;

//----------------------------------------------------------------------------

Player::Player()
{
   mTypeMask |= PlayerObjectType | DynamicShapeObjectType;

   delta.pos = mAnchorPoint = Point3F(0,0,100);
   delta.rot = delta.head = Point3F(0,0,0);
   delta.rotOffset.set(0.0f,0.0f,0.0f);
   delta.warpOffset.set(0.0f,0.0f,0.0f);
   delta.posVec.set(0.0f,0.0f,0.0f);
   delta.rotVec.set(0.0f,0.0f,0.0f);
   delta.headVec.set(0.0f,0.0f,0.0f);
   delta.warpTicks = 0;
   delta.dt = 1.0f;
   delta.move = NullMove;
   mPredictionCount = sMaxPredictionTicks;
   mInstantInput.freeLookActive = false;
   mObjToWorld.setColumn(3,delta.pos);
   mRot = delta.rot;
   mHead = delta.head;
   mVelocity.set(0.0f, 0.0f, 0.0f);
   mDataBlock = 0;
   mHeadHThread = mHeadVThread = mRecoilThread = mImageStateThread = mWingsThread = 0;
   mArmAnimation.action = PlayerData::NullAnimation;
   mArmAnimation.thread = 0;
   mActionAnimation.action = PlayerData::NullAnimation;
   mActionAnimation.thread = 0;
   mActionAnimation.delayTicks = 0;
   mActionAnimation.forward = true;
   mActionAnimation.firstPerson = false;
   //mActionAnimation.time = 1.0f; //ActionAnimation::Scale;
   mActionAnimation.waitForEnd = false;
   mActionAnimation.holdAtEnd = false;
   mActionAnimation.animateOnServer = false;
   mActionAnimation.atEnd = false;
   mState = MoveState;
   mRunSurface = false;
   mJetting = false;
   mFalling = false;
   mSwimming = false;
   mInWater = false;
	mWantsToSlide = false;
   mLastTickSlideState = false;
   mPose = StandPose;
   mSlideContact = 0;
   mContactTimer = 0;
   mJumpDelay = 0;
   mJumpSurfaceLastContact = 0;
   mJumpSurfaceNormal.set(0.0f, 0.0f, 1.0f);
   mXJumpChargeInProgress = false;
   mXJumpCharge = 0.0f;
   mInstantXJumpReady = true;
   mControlObject = 0;
   dMemset( mSplashEmitter, 0, sizeof( mSplashEmitter ) );
   dMemset( mSlideEmitter, 0, sizeof( mSlideEmitter ) );
	dMemset( mSkidEmitter, 0, sizeof( mSkidEmitter ) );

   mUseHeadZCalc = true;

   allowAllPoses();

   mImpactSound = 0;
   mRecoverTicks = 0;
   mReversePending = 0;

   mLastPos.set( 0.0f, 0.0f, 0.0f );

   mMoveBubbleSound = 0;
   mWaterBreathSound = 0;   
   mSlideSound = 0;
   mSlideContactSound = 0;
	mSkidSound = 0;
   mXJumpChargeSound = 0;

   mConvex.init(this);
   mWorkingQueryBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mWorkingQueryBox.maxExtents.set(-1e9f, -1e9f, -1e9f);

   mWeaponBackFraction = 0.0f;

   mFirstPersonWeaponBob.state = FirstPersonWeaponBob::NoBob;
	mFirstPersonWeaponBob.pos.set(0, 0, 0);
	mFirstPersonWeaponBob.counter = 0;

   mInMissionArea = true;

   mBubbleEmitterTime = 10.0;
   mLastWaterPos.set( 0.0, 0.0, 0.0 );

   mMountPending = 0;

   mPhysicsRep = NULL;

   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      mShapeFPInstance[i] = 0;
      mShapeFPAmbientThread[i] = 0;
      mShapeFPVisThread[i] = 0;
      mShapeFPAnimThread[i] = 0;
      mShapeFPFlashThread[i] = 0;
      mShapeFPSpinThread[i] = 0;
   }

   mLastAbsoluteYaw = 0.0f;
   mLastAbsolutePitch = 0.0f;
}

Player::~Player()
{
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      delete mShapeFPInstance[i];
      mShapeFPInstance[i] = 0;
   }
}


//----------------------------------------------------------------------------

bool Player::onAdd()
{
   ActionAnimation serverAnim = mActionAnimation;
   if(!Parent::onAdd() || !mDataBlock)
      return false;

   mWorkingQueryBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mWorkingQueryBox.maxExtents.set(-1e9f, -1e9f, -1e9f);

   addToScene();

   // Make sure any state and animation passed from the server
   // in the initial update is set correctly.
   ActionState state = mState;
   mState = NullState;
   setState(state);
   setPose(StandPose);

   if (serverAnim.action != PlayerData::NullAnimation)
   {
      setActionThread(serverAnim.action,
                      serverAnim.forward,
                      serverAnim.holdAtEnd,
                      serverAnim.waitForEnd,
                      serverAnim.firstPerson,
                      true);
      if (serverAnim.atEnd)
      {
         mShapeInstance->clearTransition(mActionAnimation.thread);
         mShapeInstance->setPos(mActionAnimation.thread,
                                mActionAnimation.forward ? 1.0f : 0.0f);
         if (inDeathAnim())
            mDeath.lastPos = 1.0f;
      }

      // We have to leave them sitting for a while since mounts don't come through right
      // away (and sometimes not for a while).  Still going to let this time out because
      // I'm not sure if we're guaranteed another anim will come through and cancel.
      if (!isServerObject() && inSittingAnim())
         mMountPending = (S32) sMountPendingTickWait;
      else
         mMountPending = 0;
   }
   if (mArmAnimation.action != PlayerData::NullAnimation)
      setArmThread(mArmAnimation.action);

   //
   if (isServerObject())
   {
      scriptOnAdd();
   }
   else
   {
      U32 i;
      for( i=0; i<PlayerData::NUM_SPLASH_EMITTERS; i++ )
      {
         if ( mDataBlock->splashEmitterList[i] ) 
         {
            mSplashEmitter[i] = new ParticleEmitter;
            mSplashEmitter[i]->onNewDataBlock( mDataBlock->splashEmitterList[i], false );
            if( !mSplashEmitter[i]->registerObject() )
            {
               Con::warnf( ConsoleLogEntry::General, "Could not register splash emitter for class: %s", mDataBlock->getName() );
               mSplashEmitter[i].getPointer()->destroySelf();
               mSplashEmitter[i] = NULL;
            }
         }
      }
      mLastWaterPos = getPosition();

      // clear out all camera effects
      gCamFXMgr.clear();
   }

   if ( PHYSICSMGR )
   {
      PhysicsWorld *world = PHYSICSMGR->getWorld( isServerObject() ? "server" : "client" );

      mPhysicsRep = PHYSICSMGR->createPlayer();
      mPhysicsRep->init(   mDataBlock->physicsPlayerType,
                           mDataBlock->boxSize,
                           mDataBlock->runSurfaceCos,
                           mDataBlock->maxStepHeight,
                           this, 
                           world );
      mPhysicsRep->setTransform( getTransform() );
   }

   return true;
}

void Player::onRemove()
{
   setControlObject(0);
   scriptOnRemove();
   removeFromScene();
   
   if ( isGhost() )
   {
      SFX_DELETE( mMoveBubbleSound );
      SFX_DELETE( mWaterBreathSound );
      SFX_DELETE( mSlideSound );
      SFX_DELETE( mSlideContactSound );
		SFX_DELETE( mSkidSound );
		SFX_DELETE( mXJumpChargeSound );
   }

   U32 i;
   for( i=0; i<PlayerData::NUM_SPLASH_EMITTERS; i++ )
   {
      if( mSplashEmitter[i] )
      {
         mSplashEmitter[i]->deleteWhenEmpty();
         mSplashEmitter[i] = NULL;
      }
   }

   for( i=0; i<PlayerData::NumSlideEmitters; i++ )
   {
      if(mSlideEmitter[i])
      {
         mSlideEmitter[i]->deleteWhenEmpty();
         mSlideEmitter[i] = NULL;
      }
   }

   for( i=0; i<PlayerData::NumSkidEmitters; i++ )
   {
      if(mSkidEmitter[i])
      {
         mSkidEmitter[i]->deleteWhenEmpty();
         mSkidEmitter[i] = NULL;
      }
   }

   mWorkingQueryBox.minExtents.set(-1e9f, -1e9f, -1e9f);
   mWorkingQueryBox.maxExtents.set(-1e9f, -1e9f, -1e9f);

   SAFE_DELETE( mPhysicsRep );	

   Parent::onRemove();
}

void Player::onScaleChanged()
{
   const Point3F& scale = getScale();
   mScaledBox = mObjBox;
   mScaledBox.minExtents.convolve( scale );
   mScaledBox.maxExtents.convolve( scale );
}


//----------------------------------------------------------------------------

bool Player::onNewDataBlock( GameBaseData *dptr, bool reload )
{
   PlayerData* prevData = mDataBlock;
   mDataBlock = dynamic_cast<PlayerData*>(dptr);
   if ( !mDataBlock || !Parent::onNewDataBlock( dptr, reload ) )
      return false;

   // Player requires a shape instance.
   if ( mShapeInstance == NULL )
      return false;

   // Initialize arm thread, preserve arm sequence from last datablock.
   // Arm animation can be from last datablock, or sent from the server.
   U32 prevAction = mArmAnimation.action;
   mArmAnimation.action = PlayerData::NullAnimation;
   if (mDataBlock->lookAction) {
      mArmAnimation.thread = mShapeInstance->addThread();
      mShapeInstance->setTimeScale(mArmAnimation.thread,0);
      if (prevData) {
         if (prevAction != prevData->lookAction && prevAction != PlayerData::NullAnimation)
            setArmThread(prevData->actionList[prevAction].name);
         prevAction = PlayerData::NullAnimation;
      }
      if (mArmAnimation.action == PlayerData::NullAnimation) {
         mArmAnimation.action = (prevAction != PlayerData::NullAnimation)?
            prevAction: mDataBlock->lookAction;
         mShapeInstance->setSequence(mArmAnimation.thread,
           mDataBlock->actionList[mArmAnimation.action].sequence,0);
      }
   }
   else
      mArmAnimation.thread = 0;

   // Initialize head look thread
   TSShape const* shape = mShapeInstance->getShape();
   S32 headSeq = shape->findSequence("head");
   if (headSeq != -1) {
      mHeadVThread = mShapeInstance->addThread();
      mShapeInstance->setSequence(mHeadVThread,headSeq,0);
      mShapeInstance->setTimeScale(mHeadVThread,0);
   }
   else
      mHeadVThread = 0;

   headSeq = shape->findSequence("headside");
   if (headSeq != -1) {
      mHeadHThread = mShapeInstance->addThread();
      mShapeInstance->setSequence(mHeadHThread,headSeq,0);
      mShapeInstance->setTimeScale(mHeadHThread,0);
   }
   else
      mHeadHThread = 0;

   // Create Recoil thread if any recoil sequences are specified.
   // Note that the server player does not play this animation.
   mRecoilThread = 0;
   if (isGhost())
      for (U32 s = 0; s < PlayerData::NumRecoilSequences; s++)
         if (mDataBlock->recoilSequence[s] != -1) {
            mRecoilThread = mShapeInstance->addThread();
            mShapeInstance->setSequence(mRecoilThread, mDataBlock->recoilSequence[s], 0);
            mShapeInstance->setTimeScale(mRecoilThread, 0);
            break;
         }

   // Create wings thread.
   // Note that the server player does not play this animation.
   mWingsThread = 0;
   if(this->isGhost())
   {
      if(mDataBlock->wingsSequence != -1)
      {
         mWingsThread = mShapeInstance->addThread();
         mShapeInstance->setSequence(mWingsThread, mDataBlock->wingsSequence, 0);
      }
	}

   // Reset the image state driven animation thread.  This will be properly built
   // in onImageStateAnimation() when needed.
   mImageStateThread = 0;

   // Initialize the primary thread, the actual sequence is
   // set later depending on player actions.
   mActionAnimation.action = PlayerData::NullAnimation;
   mActionAnimation.thread = mShapeInstance->addThread();
   updateAnimationTree(!isGhost());

   // First person mounted image shapes.  Only on client.
   if ( isGhost() )
   {
      for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
      {
         if (bool(mDataBlock->mShapeFP[i]))
         {
            mShapeFPInstance[i] = new TSShapeInstance(mDataBlock->mShapeFP[i], isClientObject());

            mShapeFPInstance[i]->cloneMaterialList();

            // Ambient animation
            if (mShapeFPAmbientThread[i])
            {
               S32 seq = mShapeFPInstance[i]->getShape()->findSequence("ambient");
               if (seq != -1)
               {
                  mShapeFPAmbientThread[i] = mShapeFPInstance[i]->addThread();
                  mShapeFPInstance[i]->setTimeScale(mShapeFPAmbientThread[i], 1);
                  mShapeFPInstance[i]->setSequence(mShapeFPAmbientThread[i], seq, 0);
               }
            }

            // Standard state animation
            mShapeFPAnimThread[i] = mShapeFPInstance[i]->addThread();
            if (mShapeFPAnimThread[i])
            {
               mShapeFPInstance[i]->setTimeScale(mShapeFPAnimThread[i],0);
            }
         }
      }
   }

   if ( isGhost() )
   {
      // Create the sounds ahead of time.  This reduces runtime
      // costs and makes the system easier to understand.

      SFX_DELETE( mMoveBubbleSound );
      SFX_DELETE( mWaterBreathSound );
		SFX_DELETE( mSlideSound );
		SFX_DELETE( mSlideContactSound );
		SFX_DELETE( mSkidSound );
		SFX_DELETE( mXJumpChargeSound );

      if ( mDataBlock->sound[PlayerData::MoveBubbles] )
         mMoveBubbleSound = SFX->createSource( mDataBlock->sound[PlayerData::MoveBubbles] );

      if ( mDataBlock->sound[PlayerData::WaterBreath] )
         mWaterBreathSound = SFX->createSource( mDataBlock->sound[PlayerData::WaterBreath] );

      if ( mDataBlock->sound[PlayerData::Slide] )
         mSlideSound = SFX->createSource( mDataBlock->sound[PlayerData::Slide] );

      if ( mDataBlock->sound[PlayerData::SlideContact] )
         mSlideContactSound = SFX->createSource( mDataBlock->sound[PlayerData::SlideContact] );

      if ( mDataBlock->sound[PlayerData::Skid] )
         mSkidSound = SFX->createSource( mDataBlock->sound[PlayerData::Skid] );

      if ( mDataBlock->sound[PlayerData::XJumpCharge] )
         mXJumpChargeSound = SFX->createSource( mDataBlock->sound[PlayerData::XJumpCharge] );
   }

   if(this->isGhost())
   {
      for(U32 i=0; i<PlayerData::NumSlideEmitters; i++ )
      {
         if(mSlideEmitter[i] != NULL) 
				mSlideEmitter[i]->deleteWhenEmpty();

			mSlideEmitter[i] = NULL;

			if(!mDataBlock->slideEmitter[i])
				continue;

         mSlideEmitter[i] = new ParticleEmitter;
         mSlideEmitter[i]->onNewDataBlock( mDataBlock->slideEmitter[i], true );
         if( !mSlideEmitter[i]->registerObject() )
         {
            Con::warnf( ConsoleLogEntry::General, "Could not register slide particle emitter for class: %s", mDataBlock->getName() );
            delete mSlideEmitter[i];
            mSlideEmitter[i] = NULL;
         }
         else
         {
            if(i >= PlayerData::Relative1)
               mSlideEmitter[i]->moveParticlesWithObject(this);
         }
      }
	}

   if(this->isGhost())
   {
      for(U32 i=0; i<PlayerData::NumSkidEmitters; i++ )
      {
         if(mSkidEmitter[i] != NULL) 
				mSkidEmitter[i]->deleteWhenEmpty();

			mSkidEmitter[i] = NULL;

			if(!mDataBlock->skidEmitter[i])
				continue;

         mSkidEmitter[i] = new ParticleEmitter;
         mSkidEmitter[i]->onNewDataBlock( mDataBlock->skidEmitter[i], true );
         if( !mSkidEmitter[i]->registerObject() )
         {
            Con::warnf( ConsoleLogEntry::General, "Could not register skid particle emitter for class: %s", mDataBlock->getName() );
            delete mSkidEmitter[i];
            mSkidEmitter[i] = NULL;
         }
         else
         {
            if(i >= PlayerData::SkidRelative1)
               mSkidEmitter[i]->moveParticlesWithObject(this);
         }
      }
	}

   mObjBox.maxExtents.x = mDataBlock->boxSize.x * 0.5f;
   mObjBox.maxExtents.y = mDataBlock->boxSize.y * 0.5f;
   mObjBox.maxExtents.z = mDataBlock->boxSize.z;
   mObjBox.minExtents.x = -mObjBox.maxExtents.x;
   mObjBox.minExtents.y = -mObjBox.maxExtents.y;
   mObjBox.minExtents.z = 0.0f;

   // Setup the box for our convex object...
   mObjBox.getCenter(&mConvex.mCenter);
   mConvex.mSize.x = mObjBox.len_x() / 2.0f;
   mConvex.mSize.y = mObjBox.len_y() / 2.0f;
   mConvex.mSize.z = mObjBox.len_z() / 2.0f;

   // Initialize our scaled attributes as well
   onScaleChanged();
   resetWorldBox();

   scriptOnNewDataBlock();
   return true;
}

//----------------------------------------------------------------------------

void Player::reSkin()
{
   if ( isGhost() && mShapeInstance && mSkinNameHandle.isValidString() )
   {
      Vector<String> skins;
      String(mSkinNameHandle.getString()).split( ";", skins );

      for ( S32 i = 0; i < skins.size(); i++ )
      {
         String oldSkin( mAppliedSkinName.c_str() );
         String newSkin( skins[i] );

         // Check if the skin handle contains an explicit "old" base string. This
         // allows all models to support skinning, even if they don't follow the 
         // "base_xxx" material naming convention.
         S32 split = newSkin.find( '=' );    // "old=new" format skin?
         if ( split != String::NPos )
         {
            oldSkin = newSkin.substr( 0, split );
            newSkin = newSkin.erase( 0, split+1 );
         }

         // Apply skin to both 3rd person and 1st person shape instances
         mShapeInstance->reSkin( newSkin, oldSkin );
         for ( S32 j = 0; j < ShapeBase::MaxMountedImages; j++ )
         {
            if (mShapeFPInstance[j])
               mShapeFPInstance[j]->reSkin( newSkin, oldSkin );
         }

         mAppliedSkinName = newSkin;
      }
   }
}

//----------------------------------------------------------------------------

void Player::setControllingClient(GameConnection* client)
{
   Parent::setControllingClient(client);
   if (mControlObject)
      mControlObject->setControllingClient(client);
}

void Player::setControlObject(ShapeBase* obj)
{
   if (mControlObject == obj)
      return;

   if (mControlObject) {
      mControlObject->setControllingObject(0);
      mControlObject->setControllingClient(0);
   }
   if (obj == this || obj == 0)
      mControlObject = 0;
   else {
      if (ShapeBase* coo = obj->getControllingObject())
         coo->setControlObject(0);
      if (GameConnection* con = obj->getControllingClient())
         con->setControlObject(0);

      mControlObject = obj;
      mControlObject->setControllingObject(this);
      mControlObject->setControllingClient(getControllingClient());
   }
}

void Player::onCameraScopeQuery(NetConnection *connection, CameraScopeQuery *query)
{
   // First, we are certainly in scope, and whatever we're riding is too...
   if(mControlObject.isNull() || mControlObject == mMount.object)
      Parent::onCameraScopeQuery(connection, query);
   else
   {
      connection->objectInScope(this);
      if (isMounted())
         connection->objectInScope(mMount.object);
      mControlObject->onCameraScopeQuery(connection, query);
   }
}

ShapeBase* Player::getControlObject()
{
   return mControlObject;
}

bool Player::useInstantInput()
{
   if(this->isGhost() && mUseInstantInput && !mInstantInput.freeLookActive)
   {
      GameConnection* con = this->getControllingClient();
      if(con && con->getControlObject() == this)
         return true;
   }
   return false;
}

void Player::instantInput_init(bool useInstantInput)
{
   Parent::instantInput_init(useInstantInput);

   if(useInstantInput)
   {
      mInstantInput.head = mHead;
      mInstantInput.rot = mRot;
   }
}

void Player::instantInput_yaw(F32 yaw)
{
   //Con::printf("Player::instantInput_yaw(): %f", yaw);

   F32 y = yaw * (mPose == SprintPose ? mDataBlock->sprintYawScale : 1.0f);
   if (y > M_PI_F)
      y -= M_2PI_F;

   GameConnection* con = getControllingClient();
   if(!mInstantInput.freeLookActive)
      mInstantInput.rot.z += y;

   // constrain the range of mInstantInput.rot.z
   while (mInstantInput.rot.z < 0.0f)
      mInstantInput.rot.z += M_2PI_F;
   while (mInstantInput.rot.z > M_2PI_F)
      mInstantInput.rot.z -= M_2PI_F;

   this->setRenderPosition(this->getRenderPosition(), mInstantInput.rot);
}

void Player::instantInput_pitch(F32 pitch)
{
   //Con::printf("Player::instantInput_pitch(): %f", pitch);

   F32 p = pitch * (mPose == SprintPose ? mDataBlock->sprintPitchScale : 1.0f);
   if (p > M_PI_F) 
      p -= M_2PI_F;
   mInstantInput.head.x = mClampF(mInstantInput.head.x + p,mDataBlock->minLookAngle,
                     mDataBlock->maxLookAngle);
}

void Player::processTick(const Move* move)
{
   PROFILE_SCOPE(Player_ProcessTick);

   bool prevMoveMotion = mMoveMotion;
   Pose prevPose = getPose();

   // If we're not being controlled by a client, let the
   // AI sub-module get a chance at producing a move.
   Move aiMove;
   if (!move && isServerObject() && getAIMove(&aiMove))
      move = &aiMove;

   // Manage the control object and filter moves for the player
   Move pMove,cMove;
   if (mControlObject) {
      if (!move)
         mControlObject->processTick(0);
      else {
         pMove = NullMove;
         cMove = *move;
         //if (isMounted()) {
            // Filter Jump trigger if mounted
            //pMove.trigger[2] = move->trigger[2];
            //cMove.trigger[2] = false;
         //}
         if (move->freeLook) {
            // Filter yaw/picth/roll when freelooking.
            pMove.yaw = move->yaw;
            pMove.pitch = move->pitch;
            pMove.roll = move->roll;
            pMove.freeLook = true;
            cMove.freeLook = false;
            cMove.yaw = cMove.pitch = cMove.roll = 0.0f;
         }
         mControlObject->processTick((mDamageState == Enabled)? &cMove: &NullMove);
         move = &pMove;
      }
   }

   Parent::processTick(move);
   // Warp to catch up to server
   if (delta.warpTicks > 0) {
      delta.warpTicks--;

      // Set new pos
      getTransform().getColumn(3, &delta.pos);
      delta.pos += delta.warpOffset;
      delta.rot += delta.rotOffset;

      // Wrap yaw to +/-PI
      if (delta.rot.z < - M_PI_F)
         delta.rot.z += M_2PI_F;
      else if (delta.rot.z > M_PI_F)
         delta.rot.z -= M_2PI_F;

      setPosition(delta.pos,delta.rot);
      updateDeathOffsets();
      updateLookAnimation();

      // Backstepping
      delta.posVec = -delta.warpOffset;
      delta.rotVec = -delta.rotOffset;
   }
   else {
      // If there is no move, the player is either an
      // unattached player on the server, or a player's
      // client ghost.
      if (!move) {
         if (isGhost()) {
            // If we haven't run out of prediction time,
            // predict using the last known move.
            if (mPredictionCount-- <= 0)
               return;

            //Con::printf("Player: Using delta move");
            move = &delta.move;
         }
         else
            move = &NullMove;
      }
      if (!isGhost())
         updateAnimation(TickSec);

      PROFILE_START(Player_PhysicsSection);
      if ( isServerObject() || didRenderLastRender() || getControllingClient() )
      {
         if ( !mPhysicsRep )
         {
            if ( isMounted() )
            {
               // If we're mounted then do not perform any collision checks
               // and clear our previous working list.
               mConvex.clearWorkingList();
            }
            else
            {
               updateWorkingCollisionSet();
            }
         }

         updateState();
         updateMove(move);
         updateLookAnimation();
         updateDeathOffsets();
         updatePos();
      }
      PROFILE_END();

      if (!isGhost())
      {
         // Animations are advanced based on frame rate on the
         // client and must be ticked on the server.
         updateActionThread();
         updateAnimationTree(true);

         // Check for sprinting motion changes
         Pose currentPose = getPose();
         // Player has just switched into Sprint pose and is moving
         if (currentPose == SprintPose && prevPose != SprintPose && mMoveMotion)
         {
            mDataBlock->onStartSprintMotion_callback( this );
         }
         // Player has just switched out of Sprint pose and is moving, or was just moving
         else if (currentPose != SprintPose && prevPose == SprintPose && (mMoveMotion || prevMoveMotion))
         {
            mDataBlock->onStopSprintMotion_callback( this );
         }
         // Player is in Sprint pose and has modified their motion
         else if (currentPose == SprintPose && prevMoveMotion != mMoveMotion)
         {
            if (mMoveMotion)
            {
               mDataBlock->onStartSprintMotion_callback( this );
            }
            else
            {
               mDataBlock->onStopSprintMotion_callback( this );
            }
         }
      }
   }

   if(this->isServerObject())
   {
      // Check if sliding has changed.
      if(this->isSliding() != mLastTickSlideState)
      {
         if(this->isSliding())
            mDataBlock->onStartSliding_callback(this);
         else
            mDataBlock->onStopSliding_callback(this);
      }
   }

   mLastTickSlideState = this->isSliding();
}

void Player::interpolateTick(F32 dt)
{
   if (mControlObject)
      mControlObject->interpolateTick(dt);

   // Client side interpolation
   Parent::interpolateTick(dt);

   Point3F pos = delta.pos + delta.posVec * dt;
   Point3F rot = delta.rot + delta.rotVec * dt;

   setRenderPosition(pos,rot,dt);

/*
   // apply camera effects - is this the best place? - bramage
   GameConnection* connection = GameConnection::getConnectionToServer();
   if( connection->isFirstPerson() )
   {
      ShapeBase *obj = dynamic_cast<ShapeBase*>(connection->getControlObject());
      if( obj == this )
      {
         MatrixF curTrans = getRenderTransform();
         curTrans.mul( gCamFXMgr.getTrans() );
         Parent::setRenderTransform( curTrans );
      }
   }
*/

   updateLookAnimation(dt);
   delta.dt = dt;
}

void Player::advanceTime(F32 dt)
{
   // Client side animations
   Parent::advanceTime(dt);
   updateActionThread();
   updateAnimation(dt);
   updateSplash();
   updateFroth(dt);
   updateWaterSounds(dt);
   updateSlideParticles(dt);
   updateSlideSound(dt);
   updateSkidParticles(dt);
   updateSkidSound(dt);
   updateXJumpChargeSound(dt);
	updateFirstPersonWeaponBob(dt);

   mLastPos = getPosition();

   if (mImpactSound)
      playImpactSound();

   // update camera effects.  Definitely need to find better place for this - bramage
   if( isControlObject() )
   {
      if( mDamageState == Disabled || mDamageState == Destroyed )
      {
         // clear out all camera effects being applied to player if dead
         gCamFXMgr.clear();
      }
   }
}

bool Player::getAIMove(Move* move)
{
   return false;
}

void Player::setState(ActionState state, U32 recoverTicks)
{
   if (state != mState) {
      // Skip initialization if there is no manager, the state
      // will get reset when the object is added to a manager.
      if (isProperlyAdded()) {
         switch (state) {
            case RecoverState: {
               if (mDataBlock->landSequenceTime > 0.0f)
               {
                  // Use the land sequence as the basis for the recovery
                  setActionThread(PlayerData::LandAnim, true, false, true, true);
                  F32 timeScale = mShapeInstance->getDuration(mActionAnimation.thread) / mDataBlock->landSequenceTime;
                  mShapeInstance->setTimeScale(mActionAnimation.thread,timeScale);
                  mRecoverDelay =  mDataBlock->landSequenceTime;
               }
               else
               {
                  // Legacy recover system
                  mRecoverTicks = recoverTicks;
                  mReversePending = U32(F32(mRecoverTicks) / sLandReverseScale);
                  setActionThread(PlayerData::LandAnim, true, false, true, true);
               }
               break;
            }
            
            default:
               break;
         }
      }

      mState = state;
   }
}

void Player::updateState()
{
   switch (mState)
   {
      case RecoverState:
         if (mDataBlock->landSequenceTime > 0.0f)
         {
            // Count down the land time
            mRecoverDelay -= TickSec;
            if (mRecoverDelay <= 0.0f)
            {
               setState(MoveState);
            }
         }
         else
         {
            // Legacy recover system
            if (mRecoverTicks-- <= 0)
            {
               if (mReversePending && mActionAnimation.action != PlayerData::NullAnimation)
               {
                  // this serves and counter, and direction state
                  mRecoverTicks = mReversePending;
                  mActionAnimation.forward = false;

                  S32 seq = mDataBlock->actionList[mActionAnimation.action].sequence;
                  S32 imageBasedSeq = convertActionToImagePrefix(mActionAnimation.action);
                  if (imageBasedSeq != -1)
                     seq = imageBasedSeq;

                  F32 pos = mShapeInstance->getPos(mActionAnimation.thread);

                  mShapeInstance->setTimeScale(mActionAnimation.thread, -sLandReverseScale);
                  mShapeInstance->transitionToSequence(mActionAnimation.thread,
                                                       seq, pos, sAnimationTransitionTime, true);
                  mReversePending = 0;
               }
               else
               {
                  setState(MoveState);
               }
            }        // Stand back up slowly only if not moving much-
            else if (!mReversePending && mVelocity.lenSquared() > sSlowStandThreshSquared)
            {
               mActionAnimation.waitForEnd = false;
               setState(MoveState);
            }
         }
         break;
      
      default:
         break;
   }
}

const char* Player::getStateName()
{
   if (mDamageState != Enabled)
      return "Dead";
   if (isMounted())
      return "Mounted";
   if (mState == RecoverState)
      return "Recover";
   return "Move";
}

void Player::getDamageLocation(const Point3F& in_rPos, const char *&out_rpVert, const char *&out_rpQuad)
{
   // TODO: This will be WRONG when player is prone or swimming!

   Point3F newPoint;
   mWorldToObj.mulP(in_rPos, &newPoint);

   Point3F boxSize = mObjBox.getExtents();
   F32 zHeight = boxSize.z;
   F32 zTorso  = mDataBlock->boxTorsoPercentage;
   F32 zHead   = mDataBlock->boxHeadPercentage;

   zTorso *= zHeight;
   zHead  *= zHeight;

   if (newPoint.z <= zTorso)
      out_rpVert = "legs";
   else if (newPoint.z <= zHead)
      out_rpVert = "torso";
   else
      out_rpVert = "head";

   if(dStrcmp(out_rpVert, "head") != 0)
   {
      if (newPoint.y >= 0.0f)
      {
         if (newPoint.x <= 0.0f)
            out_rpQuad = "front_left";
         else
            out_rpQuad = "front_right";
      }
      else
      {
         if (newPoint.x <= 0.0f)
            out_rpQuad = "back_left";
         else
            out_rpQuad = "back_right";
      }
   }
   else
   {
      F32 backToFront = boxSize.x;
      F32 leftToRight = boxSize.y;

      F32 backPoint  = backToFront * mDataBlock->boxHeadBackPercentage;
      F32 frontPoint = backToFront * mDataBlock->boxHeadFrontPercentage;
      F32 leftPoint  = leftToRight * mDataBlock->boxHeadLeftPercentage;
      F32 rightPoint = leftToRight * mDataBlock->boxHeadRightPercentage;

      S32 index = 0;
      if (newPoint.y < backPoint)
         index += 0;
      else if (newPoint.y >= frontPoint)
         index += 3;
      else
         index += 6;

      if (newPoint.x < leftPoint)
         index += 0;
      else if (newPoint.x >= rightPoint)
         index += 1;
      else
         index += 2;

      switch (index)
      {
         case 0: out_rpQuad = "left_back";      break;
         case 1: out_rpQuad = "middle_back";    break;
         case 2: out_rpQuad = "right_back";     break;
         case 3: out_rpQuad = "left_middle";    break;
         case 4: out_rpQuad = "middle_middle";  break;
         case 5: out_rpQuad = "right_middle";   break;
         case 6: out_rpQuad = "left_front";     break;
         case 7: out_rpQuad = "middle_front";   break;
         case 8: out_rpQuad = "right_front";    break;

         default:
            AssertFatal(0, "Bad non-tant index");
      };
   }
}

const char* Player::getPoseName() const
{
   return EngineMarshallData< PlayerPose >(getPose());
}

void Player::setPose( Pose pose )
{
   // Already the set pose, return.
   if ( pose == mPose ) 
      return;

   Pose oldPose = mPose;

   mPose = pose;

   // Not added yet, just assign the pose and return.
   if ( !isProperlyAdded() )   
      return;
        
   Point3F boxSize(1,1,1);

   // Resize the player boxes
   switch (pose) 
   {
      case StandPose:
      case SprintPose:
         boxSize = mDataBlock->boxSize;
         break;
      case CrouchPose:
         boxSize = mDataBlock->crouchBoxSize;         
         break;
      case PronePose:
         boxSize = mDataBlock->proneBoxSize;         
         break;
      case SwimPose:
         boxSize = mDataBlock->swimBoxSize;
         break;
   }

   // Object and World Boxes...
   mObjBox.maxExtents.x = boxSize.x * 0.5f;
   mObjBox.maxExtents.y = boxSize.y * 0.5f;
   mObjBox.maxExtents.z = boxSize.z;
   mObjBox.minExtents.x = -mObjBox.maxExtents.x;
   mObjBox.minExtents.y = -mObjBox.maxExtents.y;
   mObjBox.minExtents.z = 0.0f;

   resetWorldBox();

   // Setup the box for our convex object...
   mObjBox.getCenter(&mConvex.mCenter);
   mConvex.mSize.x = mObjBox.len_x() / 2.0f;
   mConvex.mSize.y = mObjBox.len_y() / 2.0f;
   mConvex.mSize.z = mObjBox.len_z() / 2.0f;

   // Initialize our scaled attributes as well...
   onScaleChanged();

   // Resize the PhysicsPlayer rep. should we have one
   if ( mPhysicsRep )
      mPhysicsRep->setSpacials( getPosition(), boxSize );

   if ( isServerObject() )
      mDataBlock->onPoseChange_callback( this, EngineMarshallData< PlayerPose >(oldPose), EngineMarshallData< PlayerPose >(mPose));
}

void Player::allowAllPoses()
{
   mAllowJumping = true;
   mAllowJetJumping = true;
   mAllowSprinting = true;
   mAllowCrouching = true;
   mAllowProne = true;
   mAllowSwimming = true;
	mAllowSliding = true;
   mAllowChargedXJump = true;
   mAllowInstantXJump = true;
}

void Player::updateMove(const Move* move)
{
   delta.move = *move;

   // Is waterCoverage high enough to be 'swimming'?
   {
      bool swimming = mWaterCoverage > 0.65f && canSwim();      

      if ( swimming != mSwimming )
      {
         if ( !isGhost() )
         {
            if ( swimming )
               mDataBlock->onStartSwim_callback( this );
            else
               mDataBlock->onStopSwim_callback( this );
         }

         mSwimming = swimming;
      }
   }

	// Does the player want to slide?
   bool wantsToSlide = move->trigger[sSlideTrigger];
   if(mWantsToSlide != wantsToSlide)
   {
      mWantsToSlide = wantsToSlide;
      this->setMaskBits(MoveMask);
   }
 
   // Trigger images
   if (mDamageState == Enabled) 
   {
		this->updateImageTrigger(move, 0, move->trigger[sImageTrigger0], false);

      // If you have a secondary mounted image then
      // send the second trigger to it.  Else give it
      // to the first image as an alt fire.
      if(this->getMountedImage(1))
			this->updateImageTrigger(move, 1, move->trigger[sImageTrigger1], false);
      else
			this->updateImageTrigger(move, 0, move->trigger[sImageTrigger1], true);
   }

   // Update current orientation
   if (mDamageState == Enabled) {
      F32 prevZRot = mRot.z;
      delta.headVec = mHead;

      bool doStandardMove = true;
      GameConnection* con = getControllingClient();

#ifdef TORQUE_EXTENDED_MOVE
      // Work with an absolute rotation from the ExtendedMove class?
      if(con && con->getControlSchemeAbsoluteRotation())
      {
         doStandardMove = false;
         const ExtendedMove* emove = dynamic_cast<const ExtendedMove*>(move);
         U32 emoveIndex = smExtendedMoveHeadPosRotIndex;
         if(emoveIndex >= ExtendedMove::MaxPositionsRotations)
            emoveIndex = 0;

         if(emove->EulerBasedRotation[emoveIndex])
         {
            // Head pitch
            mHead.x += (emove->rotX[emoveIndex] - mLastAbsolutePitch);

            // Do we also include the relative yaw value?
            if(con->getControlSchemeAddPitchToAbsRot())
            {
               F32 x = move->pitch;
               if (x > M_PI_F)
                  x -= M_2PI_F;

               mHead.x += x;
            }

            // Constrain the range of mHead.x
            while (mHead.x < -M_PI_F) 
               mHead.x += M_2PI_F;
            while (mHead.x > M_PI_F) 
               mHead.x -= M_2PI_F;

            // Rotate (heading) head or body?
            if (move->freeLook && ((isMounted() && getMountNode() == 0) || (con && !con->isFirstPerson())))
            {
               // Rotate head
               mHead.z += (emove->rotZ[emoveIndex] - mLastAbsoluteYaw);

               // Do we also include the relative yaw value?
               if(con->getControlSchemeAddYawToAbsRot())
               {
                  F32 z = move->yaw;
                  if (z > M_PI_F)
                     z -= M_2PI_F;

                  mHead.z += z;
               }

               // Constrain the range of mHead.z
               while (mHead.z < 0.0f)
                  mHead.z += M_2PI_F;
               while (mHead.z > M_2PI_F)
                  mHead.z -= M_2PI_F;
            }
            else
            {
               // Rotate body
               mRot.z += (emove->rotZ[emoveIndex] - mLastAbsoluteYaw);

               // Do we also include the relative yaw value?
               if(con->getControlSchemeAddYawToAbsRot())
               {
                  F32 z = move->yaw;
                  if (z > M_PI_F)
                     z -= M_2PI_F;

                  mRot.z += z;
               }

               // Constrain the range of mRot.z
               while (mRot.z < 0.0f)
                  mRot.z += M_2PI_F;
               while (mRot.z > M_2PI_F)
                  mRot.z -= M_2PI_F;
            }
            mLastAbsoluteYaw = emove->rotZ[emoveIndex];
            mLastAbsolutePitch = emove->rotX[emoveIndex];

            // Head bank
            mHead.y = emove->rotY[emoveIndex];

            // Constrain the range of mHead.y
            while (mHead.y > M_PI_F) 
               mHead.y -= M_2PI_F;
         }
      }
#endif

      if(doStandardMove)
      {
         F32 p = move->pitch * (mPose == SprintPose ? mDataBlock->sprintPitchScale : 1.0f);
         if (p > M_PI_F) 
            p -= M_2PI_F;
         mHead.x = mClampF(mHead.x + p,mDataBlock->minLookAngle,
                           mDataBlock->maxLookAngle);

         F32 y = move->yaw * (mPose == SprintPose ? mDataBlock->sprintYawScale : 1.0f);
         if (y > M_PI_F)
            y -= M_2PI_F;

         if (move->freeLook && ((isMounted() && getMountNode() == 0) || (con && !con->isFirstPerson())))
         {
            mInstantInput.freeLookActive = true;
            mHead.z = mClampF(mHead.z + y,
                              -mDataBlock->maxFreelookAngle,
                              mDataBlock->maxFreelookAngle);
         }
         else
         {
            mRot.z += y;
            // Rotate the head back to the front, center horizontal
            // as well if we're controlling another object.
            mHead.z *= 0.5f;
            if (mControlObject)
               mHead.x *= 0.5f;

            if(mInstantInput.freeLookActive)
            {
               mInstantInput.rot = mRot;
               mInstantInput.freeLookActive = false;
            }
         }

         // constrain the range of mRot.z
         while (mRot.z < 0.0f)
            mRot.z += M_2PI_F;
         while (mRot.z > M_2PI_F)
            mRot.z -= M_2PI_F;
      }

      delta.rot = mRot;
      delta.rotVec.x = delta.rotVec.y = 0.0f;
      delta.rotVec.z = prevZRot - mRot.z;
      if (delta.rotVec.z > M_PI_F)
         delta.rotVec.z -= M_2PI_F;
      else if (delta.rotVec.z < -M_PI_F)
         delta.rotVec.z += M_2PI_F;

      delta.head = mHead;
      delta.headVec -= mHead;
      for(U32 i=0; i<3; ++i)
      {
         if (delta.headVec[i] > M_PI_F)
            delta.headVec[i] -= M_2PI_F;
         else if (delta.headVec[i] < -M_PI_F)
            delta.headVec[i] += M_2PI_F;
      }
   }
   MatrixF zRot;
   zRot.set(EulerF(0.0f, 0.0f, mRot.z));

   // Sync instant input
   if(this->useInstantInput())
   {
      mInstantInput.rot.z = mRot.z;
      mInstantInput.head.x = mHead.x;
   }

   // Desired move direction & speed
   VectorF moveVec;
   F32 moveSpeed;
   if ((mState == MoveState || (mState == RecoverState && mDataBlock->recoverRunForceScale > 0.0f)) && mDamageState == Enabled)
   {
      zRot.getColumn(0,&moveVec);
      moveVec *= (move->x * (mPose == SprintPose ? mDataBlock->sprintStrafeScale : 1.0f));
      VectorF tv;
      zRot.getColumn(1,&tv);
      moveVec += tv * move->y;

      // Clamp water movement
      if (move->y > 0.0f)
      {
         if ( mSwimming )
            moveSpeed = getMax(mDataBlock->maxUnderwaterForwardSpeed * move->y,
                               mDataBlock->maxUnderwaterSideSpeed * mFabs(move->x));
			else if(this->isSliding())
            moveSpeed = getMax(mDataBlock->maxSlideForwardSpeed * move->y,
                               mDataBlock->maxSlideSideSpeed * mFabs(move->x));
         else if ( mPose == PronePose )
            moveSpeed = getMax(mDataBlock->maxProneForwardSpeed * move->y,
                               mDataBlock->maxProneSideSpeed * mFabs(move->x));
         else if ( mPose == CrouchPose )
            moveSpeed = getMax(mDataBlock->maxCrouchForwardSpeed * move->y,
                               mDataBlock->maxCrouchSideSpeed * mFabs(move->x));
         else if ( mPose == SprintPose )
            moveSpeed = getMax(mDataBlock->maxSprintForwardSpeed * move->y,
                               mDataBlock->maxSprintSideSpeed * mFabs(move->x));

         else // StandPose
            moveSpeed = getMax(mDataBlock->maxForwardSpeed * move->y,
                               mDataBlock->maxSideSpeed * mFabs(move->x));
      }
      else
      {
         if ( mSwimming )
            moveSpeed = getMax(mDataBlock->maxUnderwaterBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxUnderwaterSideSpeed * mFabs(move->x));
			else if(this->isSliding())
            moveSpeed = getMax(mDataBlock->maxSlideBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxSlideSideSpeed * mFabs(move->x));
         else if ( mPose == PronePose )
            moveSpeed = getMax(mDataBlock->maxProneBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxProneSideSpeed * mFabs(move->x));
         else if ( mPose == CrouchPose )
            moveSpeed = getMax(mDataBlock->maxCrouchBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxCrouchSideSpeed * mFabs(move->x));         
         else if ( mPose == SprintPose )
            moveSpeed = getMax(mDataBlock->maxSprintBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxSprintSideSpeed * mFabs(move->x));         
         else // StandPose
            moveSpeed = getMax(mDataBlock->maxBackwardSpeed * mFabs(move->y),
                               mDataBlock->maxSideSpeed * mFabs(move->x));
      }

      // Cancel any script driven animations if we are going to move.
      if (moveVec.x + moveVec.y + moveVec.z != 0.0f &&
          (mActionAnimation.action >= PlayerData::NumTableActionAnims
               || mActionAnimation.action == PlayerData::LandAnim))
         mActionAnimation.action = PlayerData::NullAnimation;
   }
   else
   {
      moveVec.set(0.0f, 0.0f, 0.0f);
      moveSpeed = 0.0f;
   }

   // Acceleration due to gravity
   VectorF acc(0.0f, 0.0f, mGravity * mDataBlock->gravityMod * mGravityMod * TickSec);

   // Determine ground contact normal. Only look for contacts if
   // we can move and aren't mounted.
   mRunSurface = false;
   VectorF contactNormal(0,0,0);
   bool jumpSurface = false;
   if ( !isMounted() )
      findContact( &mRunSurface, &jumpSurface, &contactNormal );
   if ( jumpSurface )
      mJumpSurfaceNormal = contactNormal;

   // The slide-contact check is a simple ray-cast.
   {
	   RayInfo rInfo;
	   Point3F start = this->getPosition();
	   Point3F end = start + Point3F(0, 0, -9999);
	   this->disableCollision();
	   if(getContainer()->castRay(start, end, sCollisionMoveMask, &rInfo))
			mSlideContact = (rInfo.point - start).len();
	   else
		   mSlideContact = 9999;
	   this->enableCollision();
   }
   
   // If we don't have a runSurface but we do have a contactNormal,
   // then we are standing on something that is too steep.
   // Deflect the force of gravity by the normal so we slide.
   // We could also try aligning it to the runSurface instead,
   // but this seems to work well.
   if ( !mRunSurface && !contactNormal.isZero() )  
      acc = ( acc - 2 * contactNormal * mDot( acc, contactNormal ) );   

   // Remove acc into contact surface (should only be gravity)
   // Clear out floating point acc errors, this will allow
   // the player to "rest" on the ground.
   // However, no need to do that if we're using a physics library.
   // It will take care of itself.
   if(mRunSurface && !mPhysicsRep)
   {
      F32 vd = -mDot(acc,contactNormal);
      if (vd > 0.0f) {
         VectorF dv = contactNormal * (vd + 0.002f);
         acc += dv;
         if (acc.len() < 0.0001f)
            acc.set(0.0f, 0.0f, 0.0f);
      }
   }

   // Acceleration on run surface
   if(mRunSurface && !mSwimming && !this->isSliding())
   {
      mContactTimer = 0;

      // Force a 0 move if there is no energy, and only drain
      // move energy if we're moving.
      VectorF pv;
      if (mPose == SprintPose && mEnergy[0] >= mDataBlock->minSprintEnergy) {
         if (moveSpeed)
            mEnergy[0] -= mDataBlock->sprintEnergyDrain;
         pv = moveVec;
      }
      else if (mEnergy[0] >= mDataBlock->minRunEnergy) {
         if (moveSpeed)
            mEnergy[0] -= mDataBlock->runEnergyDrain;
         pv = moveVec;
      }
      else
         pv.set(0.0f, 0.0f, 0.0f);

      // Adjust the player's requested dir. to be parallel
      // to the contact surface.
      F32 pvl = pv.len();
      if(mJetting)
      {
         pvl = moveVec.len();
         if (pvl)
         {
            VectorF nn;
            mCross(pv,VectorF(0.0f, 0.0f, 0.0f),&nn);
            nn *= 1 / pvl;
            VectorF cv(0.0f, 0.0f, 0.0f);
            cv -= nn * mDot(nn,cv);
            pv -= cv * mDot(pv,cv);
            pvl = pv.len();
         }
      }
      else if (!mPhysicsRep)
      {
         // We only do this if we're not using a physics library.  The
         // library will take care of itself.
         if (pvl)
         {
            VectorF nn;
            mCross(pv,VectorF(0.0f, 0.0f, 1.0f),&nn);
            nn *= 1.0f / pvl;
            VectorF cv = contactNormal;
            cv -= nn * mDot(nn,cv);
            pv -= cv * mDot(pv,cv);
            pvl = pv.len();
         }
      }

      // Convert to acceleration
      if ( pvl )
         pv *= moveSpeed / pvl;
      VectorF runAcc = pv - (mVelocity + acc);
      F32 runSpeed = runAcc.len();

      // Clamp acceleration, player also accelerates faster when
      // in his hard landing recover state.
      F32 maxAcc;
      if (mPose == SprintPose)
      {
         maxAcc = (mDataBlock->sprintForce / getMass()) * TickSec;
      }
      else
      {
         maxAcc = (mDataBlock->runForce / getMass()) * TickSec;
      }
      if (mState == RecoverState)
         maxAcc *= mDataBlock->recoverRunForceScale;
      if (runSpeed > maxAcc)
         runAcc *= maxAcc / runSpeed;

      if(this->isSkidding())
      {
         F32 f = mDataBlock->skidSpeed / mVelocity.len();
         runAcc *= f * mDataBlock->skidFactor;
      }

      acc += runAcc;

      // If we are running on the ground, then we're not jumping
      if (mDataBlock->isJumpAction(mActionAnimation.action))
         mActionAnimation.action = PlayerData::NullAnimation;
   }
   else if(this->isSliding())
   {
      mContactTimer++;

      VectorF pv = moveVec;
      F32 pvl = pv.len();

      if(pvl)
         pv *= moveSpeed / pvl;

      Point3F acVec = pv;
      F32 acSpeed = acVec.len();

      F32 maxAcc;
      maxAcc = (mDataBlock->slideForce / this->getMass()) * TickSec;
      F32 oldEnergy = this->getEnergyLevel(mDataBlock->slideEnergySlot);
      this->setEnergyLevel(oldEnergy-mDataBlock->slideEnergyDrain, mDataBlock->slideEnergySlot);

      if(acSpeed > maxAcc)
         acVec *= maxAcc / acSpeed;

      acc += acVec;
	}
   else if(!mSwimming && mDataBlock->glideForce > 0)
   {
      mContactTimer++;

      VectorF pv = moveVec;
      F32 pvl = pv.len();

      if(pvl)
         pv *= moveSpeed / pvl;

      Point3F acVec = pv;
      F32 acSpeed = acVec.len();

      F32 maxAcc;
      maxAcc = (mDataBlock->glideForce / this->getMass()) * TickSec;

      if(acSpeed > maxAcc)
         acVec *= maxAcc / acSpeed;

      acc += acVec;
	}
   else if (!mSwimming && mDataBlock->airControl > 0.0f)
   {
      VectorF pv;
      pv = moveVec;
      F32 pvl = pv.len();

      if (pvl)
         pv *= moveSpeed / pvl;

      VectorF runAcc = pv - (mVelocity + acc);
      runAcc.z = 0;
      runAcc.x = runAcc.x * mDataBlock->airControl;
      runAcc.y = runAcc.y * mDataBlock->airControl;
      F32 runSpeed = runAcc.len();
      // We don't test for sprinting when performing air control
      F32 maxAcc = (mDataBlock->runForce / getMass()) * TickSec * 0.3f;

      if (runSpeed > maxAcc)
         runAcc *= maxAcc / runSpeed;

      acc += runAcc;

      // There are no special air control animations 
      // so... increment this unless you really want to 
      // play the run anims in the air.
      mContactTimer++;
   }
   else if (mSwimming)
   {
      // Remove acc into contact surface (should only be gravity)
      // Clear out floating point acc errors, this will allow
      // the player to "rest" on the ground.
      F32 vd = -mDot(acc,contactNormal);
      if (vd > 0.0f) {
         VectorF dv = contactNormal * (vd + 0.002f);
         acc += dv;
         if (acc.len() < 0.0001f)
            acc.set(0.0f, 0.0f, 0.0f);
      }

      // get the head pitch and add it to the moveVec
      // This more accurate swim vector calc comes from Matt Fairfax
      MatrixF xRot, zRot;
      xRot.set(EulerF(mHead.x, 0, 0));
      zRot.set(EulerF(0, 0, mRot.z));
      MatrixF rot;
      rot.mul(zRot, xRot);
      rot.getColumn(0,&moveVec);

      moveVec *= move->x;
      VectorF tv;
      rot.getColumn(1,&tv);
      moveVec += tv * move->y;
      rot.getColumn(2,&tv);
      moveVec += tv * move->z;

      // Force a 0 move if there is no energy, and only drain
      // move energy if we're moving.
      VectorF swimVec;
      if (mEnergy[0] >= mDataBlock->minRunEnergy) {
         if (moveSpeed)         
            mEnergy[0] -= mDataBlock->runEnergyDrain;
         swimVec = moveVec;
      }
      else
         swimVec.set(0.0f, 0.0f, 0.0f);

      // If we are swimming but close enough to the shore/ground
      // we can still have a surface-normal. In this case align the
      // velocity to the normal to make getting out of water easier.
      
      moveVec.normalize();
      F32 isSwimUp = mDot( moveVec, contactNormal );

      if ( !contactNormal.isZero() && isSwimUp < 0.1f )
      {
         F32 pvl = swimVec.len();

         if ( true && pvl )
         {
            VectorF nn;
            mCross(swimVec,VectorF(0.0f, 0.0f, 1.0f),&nn);
            nn *= 1.0f / pvl;
            VectorF cv = contactNormal;
            cv -= nn * mDot(nn,cv);
            swimVec -= cv * mDot(swimVec,cv);
         }
      }

      F32 swimVecLen = swimVec.len();

      // Convert to acceleration.
      if ( swimVecLen )
         swimVec *= moveSpeed / swimVecLen;
      VectorF swimAcc = swimVec - (mVelocity + acc);
      F32 swimSpeed = swimAcc.len();

      // Clamp acceleration.
      F32 maxAcc = (mDataBlock->swimForce / getMass()) * TickSec;
      if ( swimSpeed > maxAcc )
         swimAcc *= maxAcc / swimSpeed;      

      acc += swimAcc;

      mContactTimer++;
   }
   else
      mContactTimer++;   

   // Acceleration from Jumping
   if (move->trigger[sJumpTrigger] && canJump())// !isMounted() && 
   {
      // Scale the jump impulse base on maxJumpSpeed
      F32 zSpeedScale = mVelocity.z;
      if (zSpeedScale <= mDataBlock->maxJumpSpeed)
      {
         zSpeedScale = (zSpeedScale <= mDataBlock->minJumpSpeed)? 1:
            1 - (zSpeedScale - mDataBlock->minJumpSpeed) /
            (mDataBlock->maxJumpSpeed - mDataBlock->minJumpSpeed);

         // Desired jump direction
         VectorF pv = moveVec;
         F32 len = pv.len();
         if (len > 0)
            pv *= 1 / len;

         // We want to scale the jump size by the player size, somewhat
         // in reduced ratio so a smaller player can jump higher in
         // proportion to his size, than a larger player.
         F32 scaleZ = (getScale().z * 0.25) + 0.75;

         // Calculate our jump impulse
         F32 impulse = mDataBlock->jumpForce / getMass();

         if (mDataBlock->jumpTowardsNormal)
         {
            // If we are facing into the surface jump up, otherwise
            // jump away from surface.
            F32 dot = mDot(pv,mJumpSurfaceNormal);
            if (dot <= 0)
               acc.z += mJumpSurfaceNormal.z * scaleZ * impulse * zSpeedScale;
            else
            {
               acc.x += pv.x * impulse * dot;
               acc.y += pv.y * impulse * dot;
               acc.z += mJumpSurfaceNormal.z * scaleZ * impulse * zSpeedScale;
            }
         }
         else
            acc.z += scaleZ * impulse * zSpeedScale;

         mJumpDelay = mDataBlock->jumpDelay;
         mEnergy[0] -= mDataBlock->jumpEnergyDrain;

         // If we don't have a StandJumpAnim, just play the JumpAnim...
         if(!this->isSliding())
         {
            S32 seq = (mVelocity.len() < 0.5) ? PlayerData::StandJumpAnim: PlayerData::JumpAnim; 
            if ( mDataBlock->actionList[seq].sequence == -1 )
               seq = PlayerData::JumpAnim;         
            setActionThread( seq, true, false, true );
			}

         mJumpSurfaceLastContact = JumpSkipContactsMax;

         if(!isGhost())
            mDataBlock->onJump_callback(this);
      }
   }
   else
   {
      if (jumpSurface) 
      {
         if (mJumpDelay > 0)
            mJumpDelay--;
         mJumpSurfaceLastContact = 0;
      }
      else
         mJumpSurfaceLastContact++;
   }

   if (move->trigger[sJumpJetTrigger] && !isMounted() && canJetJump())
   {
      mJetting = true;

      // Scale the jump impulse base on maxJumpSpeed
      F32 zSpeedScale = mVelocity.z;

      if (zSpeedScale <= mDataBlock->jetMaxJumpSpeed)
      {
         zSpeedScale = (zSpeedScale <= mDataBlock->jetMinJumpSpeed)? 1:
         1 - (zSpeedScale - mDataBlock->jetMinJumpSpeed) / (mDataBlock->jetMaxJumpSpeed - mDataBlock->jetMinJumpSpeed);

         // Desired jump direction
         VectorF pv = moveVec;
         F32 len = pv.len();

         if (len > 0.0f)
            pv *= 1 / len;

         // If we are facing into the surface jump up, otherwise
         // jump away from surface.
         F32 dot = mDot(pv,mJumpSurfaceNormal);
         F32 impulse = mDataBlock->jetJumpForce / getMass();

         if (dot <= 0)
            acc.z += mJumpSurfaceNormal.z * impulse * zSpeedScale;
         else
         {
            acc.x += pv.x * impulse * dot;
            acc.y += pv.y * impulse * dot;
            acc.z += mJumpSurfaceNormal.z * impulse * zSpeedScale;
         }

         mEnergy[0] -= mDataBlock->jetJumpEnergyDrain;
      }
   }
   else
   {
      mJetting = false;
   }

   // X-Jump
   bool performXJump = false;
   if(this->canChargeXJump() && move->sendCount == 0)
   {
      if(move->trigger[sChargedXJumpTrigger])
      {
         if(!mXJumpChargeInProgress)
         {
            mXJumpChargeInProgress = true;
            if(this->isServerObject())
               mDataBlock->onXJumpChargeStart_callback(this);
         }
         mXJumpCharge += mDataBlock->xJumpChargeRate;
      }
      else if(mXJumpChargeInProgress)
      {
         mXJumpChargeInProgress = false;
         if(mXJumpCharge > 0)
            performXJump = true;
      }
      F32 availableEnergy = this->getEnergyLevel(mDataBlock->xJumpEnergySlot);
      mXJumpCharge = mClampF(mXJumpCharge, 0, availableEnergy);
   }
   if(this->canInstantXJump() && mXJumpCharge == 0 && move->trigger[sInstantXJumpTrigger] && mInstantXJumpReady && move->sendCount == 0)
   {
      mXJumpCharge = mDataBlock->xJumpInstantEnergy;
      performXJump = true;
   }
   if(!move->trigger[sInstantXJumpTrigger] && !mInstantXJumpReady)
      mInstantXJumpReady = true;
   if(performXJump)
      this->performXJump(&acc);

   // Add in force from physical zones...
   acc += (mAppliedForce / getMass()) * TickSec;

   // Adjust velocity with all the move & gravity acceleration
   // TG: I forgot why doesn't the TickSec multiply happen here...
   mVelocity += acc;

   // apply horizontal air resistance

   F32 hvel = mSqrt(mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y);

   if(hvel > mDataBlock->horizResistSpeed)
   {
      F32 speedCap = hvel;
      if(speedCap > mDataBlock->horizMaxSpeed)
         speedCap = mDataBlock->horizMaxSpeed;
      speedCap -= mDataBlock->horizResistFactor * TickSec * (speedCap - mDataBlock->horizResistSpeed);
      F32 scale = speedCap / hvel;
      mVelocity.x *= scale;
      mVelocity.y *= scale;
   }
   if(mVelocity.z > mDataBlock->upResistSpeed)
   {
      if(mVelocity.z > mDataBlock->upMaxSpeed)
         mVelocity.z = mDataBlock->upMaxSpeed;
      mVelocity.z -= mDataBlock->upResistFactor * TickSec * (mVelocity.z - mDataBlock->upResistSpeed);
   }

   // Container buoyancy & drag
/* Commented out until the buoyancy calculation can be reworked so that a container and
** player with the same density will result in neutral buoyancy.
   if (mBuoyancy != 0)
   {     
      // Applying buoyancy when standing still causing some jitters-
      if (mBuoyancy > 1.0 || !mVelocity.isZero() || !runSurface)
      {
         // A little hackery to prevent oscillation
         // based on http://reinot.blogspot.com/2005/11/oh-yes-they-float-georgie-they-all.html

         F32 buoyancyForce = mBuoyancy * mGravity * mGravityMod * TickSec;
         F32 currHeight = getPosition().z;
         const F32 C = 2.0f;
         const F32 M = 0.1f;
         
         if ( currHeight + mVelocity.z * TickSec * C > mLiquidHeight )
            buoyancyForce *= M;
                  
         mVelocity.z -= buoyancyForce;
      }
   }
*/

   // Apply drag
   if ( mSwimming )
      mVelocity -= mVelocity * mDrag * TickSec * ( mVelocity.len() / mDataBlock->maxUnderwaterForwardSpeed );
   else
      mVelocity -= mVelocity * mDrag * TickSec;

   // Clamp very small velocity to zero
   if ( mVelocity.isZero() )
      mVelocity = Point3F::Zero;

   // If we are not touching anything and have sufficient -z vel,
   // we are falling.
   if (mRunSurface)
      mFalling = false;
   else
   {
      VectorF vel;
      mWorldToObj.mulV(mVelocity,&vel);
      mFalling = vel.z < mDataBlock->fallingSpeedThreshold;
   }
   
   // Vehicle Dismount   
   if ( !isGhost() && move->trigger[sVehicleDismountTrigger] && canJump())
      mDataBlock->doDismount_callback( this );

   // Enter/Leave Liquid
   if ( !mInWater && mWaterCoverage > 0.0f ) 
   {      
      mInWater = true;

      if ( !isGhost() )
         mDataBlock->onEnterLiquid_callback( this, mWaterCoverage, mLiquidType.c_str() );
   }
   else if ( mInWater && mWaterCoverage <= 0.0f ) 
   {      
      mInWater = false;

      if ( !isGhost() )
         mDataBlock->onLeaveLiquid_callback( this, mLiquidType.c_str() );
      else
      {
         // exit-water splash sound happens for client only
         if ( getSpeed() >= mDataBlock->exitSplashSoundVel && !isMounted() )         
            SFX->playOnce( mDataBlock->sound[PlayerData::ExitWater], &getTransform() );                     
      }
   }

   // Update the PlayerPose
   Pose desiredPose = mPose;

   if ( mSwimming )
      desiredPose = SwimPose; 
   else if ( mRunSurface && move->trigger[sCrouchTrigger] && canCrouch() )     
      desiredPose = CrouchPose;
   else if ( mRunSurface && move->trigger[sProneTrigger] && canProne() )
      desiredPose = PronePose;
   else if ( move->trigger[sSprintTrigger] && canSprint() )
      desiredPose = SprintPose;
   else if ( canStand() )
      desiredPose = StandPose;

   setPose( desiredPose );
}

void Player::performXJump(Point3F* acc)
{
   F32 availableEnergy = this->getEnergyLevel(mDataBlock->xJumpEnergySlot);
   if(mXJumpCharge > availableEnergy)
   {
      mXJumpCharge = 0.0;
      return;
   }

   bool canJump = false;
   U32 typeMask = ShapeBaseObjectType | StaticObjectType;
   Point3F vec(0,0,0);
   Point3F start = this->getWorldBox().getCenter();

   this->disableCollision();
   for(F32 x = -0.3; x <= 0.3; x += 0.1)
   {
      for(F32 y = -0.3; y <= 0.3; y += 0.1)
      {
         for(F32 z = -0.3; z <= 0.3; z += 0.1)
         {
            Point3F v(x,y,z);
            v.normalize();
            v *= 5;
            Point3F end = start + v;
            RayInfo rInfo;
            if(this->getContainer()->castRay(start, end, typeMask, &rInfo))
            {
               canJump = true;
               vec += v;
            }
         }
      }
   }
   this->enableCollision();

   if(!canJump)
   {
      mXJumpCharge = 0.0;
      return;
   }

   F32 speedfactor = this->getVelocity().len() / mDataBlock->xJumpSpeedFactorD;
   if(speedfactor < 1)
      speedfactor = 1;

   F32 power = mDataBlock->xJumpPowerBaseConst + mDataBlock->xJumpPowerMultConst * mXJumpCharge * speedfactor;

   Point3F dirVec = vec;
   dirVec.normalize();
   dirVec *= -power;

   acc->x = dirVec.x / getMass();
   acc->y = dirVec.y / getMass();
   acc->z = dirVec.z / getMass();

   //this->applyImpulse(this->getPosition(), dirVec);
   this->setEnergyLevel(availableEnergy-mXJumpCharge, mDataBlock->xJumpEnergySlot);

   // If we don't have a StandJumpAnim, just play the JumpAnim...
   if(!this->isSliding())
   {
      S32 seq = (mVelocity.len() < 0.5) ? PlayerData::StandJumpAnim: PlayerData::JumpAnim; 
      if ( mDataBlock->actionList[seq].sequence == -1 )
         seq = PlayerData::JumpAnim;         
      setActionThread( seq, true, false, true );
	}

   mJumpSurfaceLastContact = JumpSkipContactsMax;
   mInstantXJumpReady = false;
   mXJumpChargeInProgress = false;

   if(this->isGhost())
   {
      MatrixF footMat = this->getTransform();
		SFX->playOnce(mDataBlock->sound[PlayerData::XJump], &footMat);
   }
   else
      mDataBlock->onXJump_callback(this, dirVec);

   mXJumpCharge = 0.0;   
}

//----------------------------------------------------------------------------

bool Player::checkDismountPosition(const MatrixF& oldMat, const MatrixF& mat)
{
   AssertFatal(getContainer() != NULL, "Error, must have a container!");
   AssertFatal(getObjectMount() != NULL, "Error, must be mounted!");

   Point3F pos;
   Point3F oldPos;
   mat.getColumn(3, &pos);
   oldMat.getColumn(3, &oldPos);
   RayInfo info;
   disableCollision();
   getObjectMount()->disableCollision();
   if (getContainer()->castRay(oldPos, pos, sCollisionMoveMask, &info))
   {
      enableCollision();
      getObjectMount()->enableCollision();
      return false;
   }

   Box3F wBox = mObjBox;
   wBox.minExtents += pos;
   wBox.maxExtents += pos;

   EarlyOutPolyList polyList;
   polyList.mNormal.set(0.0f, 0.0f, 0.0f);
   polyList.mPlaneList.clear();
   polyList.mPlaneList.setSize(6);
   polyList.mPlaneList[0].set(wBox.minExtents,VectorF(-1.0f, 0.0f, 0.0f));
   polyList.mPlaneList[1].set(wBox.maxExtents,VectorF(0.0f, 1.0f, 0.0f));
   polyList.mPlaneList[2].set(wBox.maxExtents,VectorF(1.0f, 0.0f, 0.0f));
   polyList.mPlaneList[3].set(wBox.minExtents,VectorF(0.0f, -1.0f, 0.0f));
   polyList.mPlaneList[4].set(wBox.minExtents,VectorF(0.0f, 0.0f, -1.0f));
   polyList.mPlaneList[5].set(wBox.maxExtents,VectorF(0.0f, 0.0f, 1.0f));

   if (getContainer()->buildPolyList(PLC_Collision, wBox, sCollisionMoveMask, &polyList))
   {
      enableCollision();
      getObjectMount()->enableCollision();
      return false;
   }

   enableCollision();
   getObjectMount()->enableCollision();
   return true;
}

//----------------------------------------------------------------------------

bool Player::canSlide()
{
   F32 energyAvailable = this->getEnergyLevel(mDataBlock->slideEnergySlot);
   return (mAllowSliding 
		  && mState == MoveState
		  && mDamageState == Enabled
		  && !isMounted() 
		  && energyAvailable >= mDataBlock->minSlideEnergy 
		  && mDataBlock->slideForce != 0.0f);
}

bool Player::isSliding()
{
   return (mWantsToSlide && this->canSlide());
}

//----------------------------------------------------------------------------

bool Player::isSkidding()
{
   return (mRunSurface && mVelocity.len() > mDataBlock->skidSpeed && !this->isSliding());
}

//----------------------------------------------------------------------------

bool Player::canJump()
{
   return mAllowJumping && mState == MoveState && mDamageState == Enabled && !isMounted() && !mJumpDelay && mEnergy[0] >= mDataBlock->minJumpEnergy && mJumpSurfaceLastContact < JumpSkipContactsMax && !mSwimming && (mPose != SprintPose || mDataBlock->sprintCanJump);
}

bool Player::canJetJump()
{
   return mAllowJetJumping && mState == MoveState && mDamageState == Enabled && !isMounted() && mEnergy[0] >= mDataBlock->jetMinJumpEnergy && mDataBlock->jetJumpForce != 0.0f;
}

bool Player::canChargeXJump()
{
   return mAllowChargedXJump && mDamageState == Enabled && !isMounted();
}

bool Player::canInstantXJump()
{
   return mAllowInstantXJump && mDamageState == Enabled && !isMounted() && mEnergy[mDataBlock->xJumpEnergySlot] >= mDataBlock->xJumpInstantEnergy;
}

bool Player::canSwim()
{  
   // Not used!
   //return mState == MoveState && mDamageState == Enabled && !isMounted() && mEnergy[0] >= mDataBlock->minSwimEnergy[0] && mWaterCoverage >= 0.8f;
   return mAllowSwimming;
}

bool Player::canCrouch()
{
   if (!mAllowCrouching)
      return false;

   if ( mState != MoveState || 
        mDamageState != Enabled || 
        isMounted() || 
        mSwimming ||
        mFalling )
      return false;

   // Can't crouch if no crouch animation!
   if ( mDataBlock->actionList[PlayerData::CrouchRootAnim].sequence == -1 )
      return false;       

	// We are already in this pose, so don't test it again...
	if ( mPose == CrouchPose )
		return true;

   // Do standard Torque physics test here!
   if ( !mPhysicsRep )
   {
      F32 radius;

      if ( mPose == PronePose )
         radius = mDataBlock->proneBoxSize.z;
      else
         return true;

      // use our X and Y dimentions on our boxsize as the radii for our search, and the difference between a standing position
      // and the position we currently are in.
      Point3F extent( mDataBlock->crouchBoxSize.x / 2, mDataBlock->crouchBoxSize.y / 2, mDataBlock->crouchBoxSize.z - radius );

      Point3F position = getPosition();
      position.z += radius;

      // Use these radii to create a box that represents the difference between a standing position and the position
      // we want to move into.
      Box3F B(position - extent, position + extent, true);

      EarlyOutPolyList polyList;
      polyList.mPlaneList.clear();
      polyList.mNormal.set( 0,0,0 );
      polyList.mPlaneList.setSize( 6 );
      polyList.mPlaneList[0].set( B.minExtents, VectorF( -1,0,0 ) );
      polyList.mPlaneList[1].set( B.maxExtents, VectorF( 0,1,0 ) );
      polyList.mPlaneList[2].set( B.maxExtents, VectorF( 1,0,0 ) );
      polyList.mPlaneList[3].set( B.minExtents, VectorF( 0,-1,0 ) );
      polyList.mPlaneList[4].set( B.minExtents, VectorF( 0,0,-1 ) );
      polyList.mPlaneList[5].set( B.maxExtents, VectorF( 0,0,1 ) );

      // If an object exists in this space, we must stay prone. Otherwise we are free to crouch.
      return !getContainer()->buildPolyList( PLC_Collision, B, StaticShapeObjectType, &polyList );
   }

   return mPhysicsRep->testSpacials( getPosition(), mDataBlock->crouchBoxSize );
}

bool Player::canStand()
{   
   if ( mState != MoveState || 
        mDamageState != Enabled || 
        isMounted() || 
        mSwimming )
      return false;

   // We are already in this pose, so don't test it again...
	if ( mPose == StandPose )
		return true;

   // Do standard Torque physics test here!
   if ( !mPhysicsRep )
   {
      F32 radius;

      if (mPose == CrouchPose)
         radius = mDataBlock->crouchBoxSize.z;
      else if (mPose == PronePose)
         radius = mDataBlock->proneBoxSize.z;
      else
         return true;

      // use our X and Y dimentions on our boxsize as the radii for our search, and the difference between a standing position
      // and the position we currently are in.
      Point3F extent( mDataBlock->boxSize.x / 2, mDataBlock->boxSize.y / 2, mDataBlock->boxSize.z - radius );

      Point3F position = getPosition();
      position.z += radius;

      // Use these radii to create a box that represents the difference between a standing position and the position
      // we want to move into.
      Box3F B(position - extent, position + extent, true);

      EarlyOutPolyList polyList;
      polyList.mPlaneList.clear();
      polyList.mNormal.set(0,0,0);
      polyList.mPlaneList.setSize(6);
      polyList.mPlaneList[0].set(B.minExtents, VectorF(-1,0,0));
      polyList.mPlaneList[1].set(B.maxExtents, VectorF(0,1,0));
      polyList.mPlaneList[2].set(B.maxExtents, VectorF(1,0,0));
      polyList.mPlaneList[3].set(B.minExtents, VectorF(0,-1,0));
      polyList.mPlaneList[4].set(B.minExtents, VectorF(0,0,-1));
      polyList.mPlaneList[5].set(B.maxExtents, VectorF(0,0,1));

      // If an object exists in this space, we must stay crouched/prone. Otherwise we are free to stand.
      return !getContainer()->buildPolyList(PLC_Collision, B, StaticShapeObjectType, &polyList);
   }

   return mPhysicsRep->testSpacials( getPosition(), mDataBlock->boxSize );
}

bool Player::canProne()
{
   if (!mAllowProne)
      return false;

   if ( mState != MoveState || 
        mDamageState != Enabled || 
        isMounted() || 
        mSwimming ||
        mFalling )
      return false;

   // Can't go prone if no prone animation!
   if ( mDataBlock->actionList[PlayerData::ProneRootAnim].sequence == -1 )
      return false;

   // Do standard Torque physics test here!
   if ( !mPhysicsRep )
      return true;

	// We are already in this pose, so don't test it again...
	if ( mPose == PronePose )
		return true;

   return mPhysicsRep->testSpacials( getPosition(), mDataBlock->proneBoxSize );
}

bool Player::canSprint()
{
   return mAllowSprinting && mState == MoveState && mDamageState == Enabled && !isMounted() && mEnergy[0] >= mDataBlock->minSprintEnergy && !mSwimming;
}

//----------------------------------------------------------------------------

bool Player::updateImageTargetFollow(MountedImage& image)
{
	if(!this->checkImageTargetingAim(image))
		return false;

	// Use the eye as the current position.
	MatrixF eye;
	this->getEyeTransform(&eye);
	Point3F location = eye.getPosition();
	Point3F rotation = this->getRotation();

	Point3F targetPos;
	if(image.currTarget->getTypeMask() & PlayerObjectType)
	{
		Player* p = (Player*)image.currTarget;
		MatrixF mat; p->getEyeTransform(&mat);
		targetPos = mat.getPosition();
	}
	else
	{
		targetPos = image.currTarget->getBoxCenter();
	}

	// predict pos on next tick...
	location  += this->getVelocity() * TickSec;
	targetPos += image.currTarget->getVelocity() * TickSec;

	F32 xDiff = targetPos.x - location.x;
	F32 yDiff = targetPos.y - location.y;
	if(!mIsZero(xDiff) || !mIsZero(yDiff))
	{
		// First do Yaw
		// use the cur yaw between -Pi and Pi
		F32 curYaw = rotation.z;
		while (curYaw > M_2PI)
			curYaw -= M_2PI;
		while (curYaw < -M_2PI)
			curYaw += M_2PI;

		// find the yaw offset
		F32 newYaw = mAtan2( xDiff, yDiff );
		F32 yawDiff = newYaw - curYaw;

		// make it between 0 and 2PI
		if( yawDiff < 0.0f )
			yawDiff += M_2PI;
		else if( yawDiff >= M_2PI )
			yawDiff -= M_2PI;

		// now make sure we take the short way around the circle
		if( yawDiff > M_PI )
			yawDiff -= M_2PI;
		else if( yawDiff < -M_PI )
			yawDiff += M_2PI;

		// This should be adjusted to run from the
		// eye point to the object's center position. Though this
		// works well enough for now.
		F32 vertDist = targetPos.z - location.z;
		F32 horzDist = mSqrt(xDiff * xDiff + yDiff * yDiff);
		F32 newPitch = mAtan2( horzDist, vertDist ) - ( M_PI / 2.0f );
		Point3F headRotation = getHeadRotation();
		F32 pitchDiff = newPitch - headRotation.x;

		MoveManager::mYaw = yawDiff;
		MoveManager::mPitch = pitchDiff;
	}

	return true;
}

//----------------------------------------------------------------------------

void Player::updateDamageLevel()
{
   Parent::updateDamageLevel();

   if (!isGhost())
      setDamageState((mDamage >= mDataBlock->maxDamage)? Disabled: Enabled);
   if (mDamageThread)
      mShapeInstance->setPos(mDamageThread, mDamage / mDataBlock->destroyedLevel);
}

void Player::updateDamageState()
{
   // Become a corpse when we're disabled (dead).
   if (mDamageState == Enabled) {
      mTypeMask &= ~CorpseObjectType;
      mTypeMask |= PlayerObjectType;
   }
   else {
      mTypeMask &= ~PlayerObjectType;
      mTypeMask |= CorpseObjectType;
   }

   Parent::updateDamageState();
}


//----------------------------------------------------------------------------

void Player::updateLookAnimation(F32 dt)
{
   // Calculate our interpolated head position.
   Point3F renderHead = delta.head + delta.headVec * dt;

   // Adjust look pos.  This assumes that the animations match
   // the min and max look angles provided in the datablock.
   if (mArmAnimation.thread) 
   {
      if(mControlObject)
      {
         mShapeInstance->setPos(mArmAnimation.thread,0.5f);
      }
      else
      {
         F32 d = mDataBlock->maxLookAngle - mDataBlock->minLookAngle;
         F32 tp = (renderHead.x - mDataBlock->minLookAngle) / d;
         mShapeInstance->setPos(mArmAnimation.thread,mClampF(tp,0,1));
      }
   }
   
   if (mHeadVThread) 
   {
      F32 d = mDataBlock->maxLookAngle - mDataBlock->minLookAngle;
      F32 tp = (renderHead.x - mDataBlock->minLookAngle) / d;
      mShapeInstance->setPos(mHeadVThread,mClampF(tp,0,1));
   }
   
   if (mHeadHThread) 
   {
      F32 d = 2 * mDataBlock->maxFreelookAngle;
      F32 tp = (renderHead.z + mDataBlock->maxFreelookAngle) / d;
      mShapeInstance->setPos(mHeadHThread,mClampF(tp,0,1));
   }
}


//----------------------------------------------------------------------------
// Methods to get delta (as amount to affect velocity by)

bool Player::inDeathAnim()
{
   if (mActionAnimation.thread && mActionAnimation.action >= 0)
      if (mActionAnimation.action < mDataBlock->actionCount)
         return mDataBlock->actionList[mActionAnimation.action].death;

   return false;
}

// Get change from mLastDeathPos - return current pos.  Assumes we're in death anim.
F32 Player::deathDelta(Point3F & delta)
{
   // Get ground delta from the last time we offset this.
   MatrixF  mat;
   F32 pos = mShapeInstance->getPos(mActionAnimation.thread);
   mShapeInstance->deltaGround1(mActionAnimation.thread, mDeath.lastPos, pos, mat);
   mat.getColumn(3, & delta);
   return pos;
}

// Called before updatePos() to prepare it's needed change to velocity, which
// must roll over.  Should be updated on tick, this is where we remember last
// position of animation that was used to roll into velocity.
void Player::updateDeathOffsets()
{
   if (inDeathAnim())
      // Get ground delta from the last time we offset this.
      mDeath.lastPos = deathDelta(mDeath.posAdd);
   else
      mDeath.clear();
}


//----------------------------------------------------------------------------

static const U32 sPlayerConformMask =  StaticShapeObjectType | StaticObjectType | TerrainObjectType;

static void accel(F32& from, F32 to, F32 rate)
{
   if (from < to)
      from = getMin(from += rate, to);
   else
      from = getMax(from -= rate, to);
}

// if (dt == -1)
//    normal tick, so we advance.
// else
//    interpolate with dt as % of tick, don't advance
//
MatrixF * Player::Death::fallToGround(F32 dt, const Point3F& loc, F32 curZ, F32 boxRad)
{
   static const F32 sConformCheckDown = 4.0f;
   RayInfo     coll;
   bool        conformToStairs = false;
   Point3F     pos(loc.x, loc.y, loc.z + 0.1f);
   Point3F     below(pos.x, pos.y, loc.z - sConformCheckDown);
   MatrixF  *  retVal = NULL;

   PROFILE_SCOPE(ConformToGround);

   if (gClientContainer.castRay(pos, below, sPlayerConformMask, &coll))
   {
      F32      adjust, height = (loc.z - coll.point.z), sink = curSink;
      VectorF  desNormal = coll.normal;
      VectorF  normal = curNormal;

      // dt >= 0 means we're interpolating and don't accel the numbers
      if (dt >= 0.0f)
         adjust = dt * TickSec;
      else
         adjust = TickSec;

      // Try to get them to conform to stairs by doing several LOS calls.  We do this if
      // normal is within about 5 deg. of vertical.
      if (desNormal.z > 0.995f)
      {
         Point3F  corners[3], downpts[3];
         S32      c;

         for (c = 0; c < 3; c++) {    // Build 3 corners to cast down from-
            corners[c].set(loc.x - boxRad, loc.y - boxRad, loc.z + 1.0f);
            if (c)      // add (0,boxWidth) and (boxWidth,0)
               corners[c][c - 1] += (boxRad * 2.0f);
            downpts[c].set(corners[c].x, corners[c].y, loc.z - sConformCheckDown);
         }

         // Do the three casts-
         for (c = 0; c < 3; c++)
            if (gClientContainer.castRay(corners[c], downpts[c], sPlayerConformMask, &coll))
               downpts[c] = coll.point;
            else
               break;

         // Do the math if everything hit below-
         if (c == 3) {
            mCross(downpts[1] - downpts[0], downpts[2] - downpts[1], &desNormal);
            AssertFatal(desNormal.z > 0, "Abnormality in Player::Death::fallToGround()");
            downpts[2] = downpts[2] - downpts[1];
            downpts[1] = downpts[1] - downpts[0];
            desNormal.normalize();
            conformToStairs = true;
         }
      }

      // Move normal in direction we want-
      F32   * cur = normal, * des = desNormal;
      for (S32 i = 0; i < 3; i++)
         accel(*cur++, *des++, adjust * 0.25f);

      if (mFabs(height) < 2.2f && !normal.isZero() && desNormal.z > 0.01f)
      {
         VectorF  upY(0.0f, 1.0f, 0.0f), ahead;
         VectorF  sideVec;
         MatrixF  mat(true);

         normal.normalize();
         mat.set(EulerF (0.0f, 0.0f, curZ));
         mat.mulV(upY, & ahead);
	      mCross(ahead, normal, &sideVec);
         sideVec.normalize();
         mCross(normal, sideVec, &ahead);

         static MatrixF resMat(true);
         resMat.setColumn(0, sideVec);
         resMat.setColumn(1, ahead);
         resMat.setColumn(2, normal);

         // Adjust Z down to account for box offset on slope.  Figure out how
         // much we want to sink, and gradually accel to this amount.  Don't do if
         // we're conforming to stairs though
         F32   xy = mSqrt(desNormal.x * desNormal.x + desNormal.y * desNormal.y);
         F32   desiredSink = (boxRad * xy / desNormal.z);
         if (conformToStairs)
            desiredSink *= 0.5f;

         accel(sink, desiredSink, adjust * 0.15f);

         Point3F  position(pos);
         position.z -= sink;
         resMat.setColumn(3, position);

         if (dt < 0.0f)
         {  // we're moving, so update normal and sink amount
            curNormal = normal;
            curSink = sink;
         }

         retVal = &resMat;
      }
   }
   return retVal;
}


//-------------------------------------------------------------------------------------

// This is called ::onAdd() to see if we're in a sitting animation.  These then
// can use a longer tick delay for the mount to get across.
bool Player::inSittingAnim()
{
   U32   action = mActionAnimation.action;
   if (mActionAnimation.thread && action < mDataBlock->actionCount) {
      const char * name = mDataBlock->actionList[action].name;
      if (!dStricmp(name, "Sitting") || !dStricmp(name, "Scoutroot"))
         return true;
   }
   return false;
}


//----------------------------------------------------------------------------

const String& Player::getArmThread() const
{
   if (mArmAnimation.thread && mArmAnimation.thread->hasSequence())
   {
      return mArmAnimation.thread->getSequenceName();
   }

   return String::EmptyString;
}

bool Player::setArmThread(const char* sequence, bool updateClients)
{
   // The arm sequence must be in the action list.
   for (U32 i = 1; i < mDataBlock->actionCount; i++)
      if (!dStricmp(mDataBlock->actionList[i].name,sequence))
         return setArmThread(i, updateClients);
   return false;
}

bool Player::setArmThread(U32 action, bool updateClients)
{
   PlayerData::ActionAnimation &anim = mDataBlock->actionList[action];
   if (anim.sequence != -1 &&
      anim.sequence != mShapeInstance->getSequence(mArmAnimation.thread))
   {
      mShapeInstance->setSequence(mArmAnimation.thread,anim.sequence,0);
      mArmAnimation.action = action;

		if(isGhost())
			this->advanceTime(0);

		if(isServerObject() && updateClients)
			setMaskBits(ActionMask);

      return true;
   }
   return false;
}


//----------------------------------------------------------------------------

bool Player::setActionThread(const char* sequence,bool hold,bool wait,bool fsp)
{
   for (U32 i = 1; i < mDataBlock->actionCount; i++)
   {
      PlayerData::ActionAnimation &anim = mDataBlock->actionList[i];
      if (!dStricmp(anim.name,sequence))
      {
         setActionThread(i,true,hold,wait,fsp);
         setMaskBits(ActionMask);
         return true;
      }
   }
   return false;
}

void Player::setActionThread(U32 action,bool forward,bool hold,bool wait,bool fsp, bool forceSet)
{
   if (!mDataBlock || !mDataBlock->actionCount || (mActionAnimation.action == action && mActionAnimation.forward == forward && !forceSet))
      return;

   if (action >= PlayerData::NumActionAnims)
   {
      Con::errorf("Player::setActionThread(%d): Player action out of range", action);
      return;
   }

   PlayerData::ActionAnimation &anim = mDataBlock->actionList[action];
   if (anim.sequence != -1)
   {
      U32 lastAction = mActionAnimation.action;

      mActionAnimation.action          = action;
      mActionAnimation.forward         = forward;
      mActionAnimation.firstPerson     = fsp;
      mActionAnimation.holdAtEnd       = hold;
      mActionAnimation.waitForEnd      = hold? true: wait;
      mActionAnimation.animateOnServer = fsp;
      mActionAnimation.atEnd           = false;
      mActionAnimation.delayTicks      = (S32)sNewAnimationTickTime;
      mActionAnimation.atEnd           = false;

      if (sUseAnimationTransitions && (action != PlayerData::LandAnim || !(mDataBlock->landSequenceTime > 0.0f && !mDataBlock->transitionToLand)) && (isGhost()/* || mActionAnimation.animateOnServer*/))
      {
         // The transition code needs the timeScale to be set in the
         // right direction to know which way to go.
         F32   transTime = sAnimationTransitionTime;
         if (mDataBlock && mDataBlock->isJumpAction(action))
            transTime = 0.15f;

         F32 timeScale = mActionAnimation.forward ? 1.0f : -1.0f;
         if (mDataBlock && mDataBlock->isJumpAction(action))
            timeScale *= 1.5f;

         mShapeInstance->setTimeScale(mActionAnimation.thread,timeScale);

         S32 seq = anim.sequence;
         S32 imageBasedSeq = convertActionToImagePrefix(mActionAnimation.action);
         if (imageBasedSeq != -1)
            seq = imageBasedSeq;

         // If we're transitioning into the same sequence (an action may use the
         // same sequence as a previous action) then we want to start at the same
         // position.
         F32 pos = mActionAnimation.forward ? 0.0f : 1.0f;
         PlayerData::ActionAnimation &lastAnim = mDataBlock->actionList[lastAction];
         if (lastAnim.sequence == anim.sequence)
         {
            pos = mShapeInstance->getPos(mActionAnimation.thread);
         }

         mShapeInstance->transitionToSequence(mActionAnimation.thread,seq,
            pos, transTime, true);
      }
      else
      {
         S32 seq = anim.sequence;
         S32 imageBasedSeq = convertActionToImagePrefix(mActionAnimation.action);
         if (imageBasedSeq != -1)
            seq = imageBasedSeq;

         mShapeInstance->setSequence(mActionAnimation.thread,seq,
            mActionAnimation.forward ? 0.0f : 1.0f);
      }
   }

	// Initialize first person weapon bobbing.
	if(mFirstPersonWeaponBob.state == FirstPersonWeaponBob::NoBob)
	{
      switch(action)
      {
         case PlayerData::RunForwardAnim:
         case PlayerData::SprintForwardAnim:
         case PlayerData::CrouchForwardAnim:
         case PlayerData::ProneForwardAnim:
         case PlayerData::BackBackwardAnim:
         case PlayerData::SprintBackwardAnim:
         case PlayerData::CrouchBackwardAnim:
         case PlayerData::ProneBackwardAnim:
            mFirstPersonWeaponBob.state = FirstPersonWeaponBob::BobSideways;
            mFirstPersonWeaponBob.counter = M_PI; // bob to left first
            break;

         case PlayerData::SideLeftAnim:
         case PlayerData::SprintLeftAnim:
         case PlayerData::CrouchLeftAnim:
            mFirstPersonWeaponBob.state = FirstPersonWeaponBob::BobSideways;
            if(forward) // sidestep left
               mFirstPersonWeaponBob.counter = M_PI; // bob to left first
            else // sidestep right
               mFirstPersonWeaponBob.counter = 0; // bob to right first
            break;

         case PlayerData::SideRightAnim:
         case PlayerData::SprintRightAnim:
         case PlayerData::CrouchRightAnim:
            mFirstPersonWeaponBob.state = FirstPersonWeaponBob::BobSideways;
            if(forward) // sidestep right
               mFirstPersonWeaponBob.counter = 0; // bob to right first
            else // sidestep right
               mFirstPersonWeaponBob.counter = M_PI; // bob to left first
            break;
      }
   }

   switch(action)
   {
      case PlayerData::RootAnim:
      case PlayerData::SprintRootAnim:
      case PlayerData::CrouchRootAnim:
      case PlayerData::ProneRootAnim:
      case PlayerData::SwimRootAnim:
      case PlayerData::SwimForwardAnim:
      case PlayerData::SwimBackwardAnim:
      case PlayerData::SwimLeftAnim:
      case PlayerData::SwimRightAnim:
      case PlayerData::FallAnim:
      case PlayerData::SlideAnim:
      case PlayerData::JumpAnim:
      case PlayerData::StandJumpAnim:
      case PlayerData::LandAnim:
      case PlayerData::JetAnim:
         mFirstPersonWeaponBob.state = FirstPersonWeaponBob::NoBob;
         mFirstPersonWeaponBob.counter = 0;
         break;
   }
}

void Player::updateActionThread()
{
   PROFILE_START(UpdateActionThread);

   // Select an action animation sequence, this assumes that
   // this function is called once per tick.
   if(mActionAnimation.action != PlayerData::NullAnimation)
      if (mActionAnimation.forward)
         mActionAnimation.atEnd = mShapeInstance->getPos(mActionAnimation.thread) == 1;
      else
         mActionAnimation.atEnd = mShapeInstance->getPos(mActionAnimation.thread) == 0;

   // Only need to deal with triggers on the client
   if( isGhost() )
   {
      bool triggeredLeft = false;
      bool triggeredRight = false;
      
      F32 offset = 0.0f;
      if( mShapeInstance->getTriggerState( 1 ) )
      {
         triggeredLeft = true;
         offset = -mDataBlock->decalOffset * getScale().x;
      }
      else if(mShapeInstance->getTriggerState( 2 ) )
      {
         triggeredRight = true;
         offset = mDataBlock->decalOffset * getScale().x;
      }

      if(this->isSliding() && mDataBlock->slideDecal)
      {
         Point3F rot, pos;
         RayInfo rInfo;
         MatrixF mat = getRenderTransform();
         mat.getColumn( 1, &rot );
         mat.mulP( Point3F( offset, 0.0f, 0.0f), &pos );

         if( gClientContainer.castRay( Point3F( pos.x, pos.y, pos.z + 0.01f ),
               Point3F( pos.x, pos.y, pos.z - 2.0f ),
               STATIC_COLLISION_TYPEMASK | VehicleObjectType, &rInfo ) )
         {
            Point3F normal;
            Point3F tangent;
            mObjToWorld.getColumn( 0, &tangent );
            mObjToWorld.getColumn( 2, &normal );
            DecalInstance* dinst = gDecalManager->addDecal( rInfo.point, normal, tangent, mDataBlock->slideDecal, getScale().y );
            if(dinst)
               dinst->mPalette = this->getPalette();
         }
      }
      else if(this->isSkidding() && mDataBlock->skidDecal)
      {
         Point3F rot, pos;
         RayInfo rInfo;
         MatrixF mat = getRenderTransform();
         mat.getColumn( 1, &rot );
         mat.mulP( Point3F( offset, 0.0f, 0.0f), &pos );

         if( gClientContainer.castRay( Point3F( pos.x, pos.y, pos.z + 0.01f ),
               Point3F( pos.x, pos.y, pos.z - 2.0f ),
               STATIC_COLLISION_TYPEMASK | VehicleObjectType, &rInfo ) )
         {
            Point3F normal;
            Point3F tangent;
            mObjToWorld.getColumn( 0, &tangent );
            mObjToWorld.getColumn( 2, &normal );
            DecalInstance* dinst = gDecalManager->addDecal( rInfo.point, normal, tangent, mDataBlock->skidDecal, getScale().y );
            if(dinst)
               dinst->mPalette = this->getPalette();
         }
      }
      else if( triggeredLeft || triggeredRight )
      {
         Point3F rot, pos;
         RayInfo rInfo;
         MatrixF mat = getRenderTransform();
         mat.getColumn( 1, &rot );
         mat.mulP( Point3F( offset, 0.0f, 0.0f), &pos );

         if( gClientContainer.castRay( Point3F( pos.x, pos.y, pos.z + 0.01f ),
               Point3F( pos.x, pos.y, pos.z - 2.0f ),
               STATIC_COLLISION_TYPEMASK | VehicleObjectType, &rInfo ) )
         {
            Material* material = ( rInfo.material ? dynamic_cast< Material* >( rInfo.material->getMaterial() ) : 0 );

            // Put footprints on surface, if appropriate for material.

            if( material && material->mShowFootprints
                && mDataBlock->decalData )
            {
               Point3F normal;
               Point3F tangent;
               mObjToWorld.getColumn( 0, &tangent );
               mObjToWorld.getColumn( 2, &normal );
               DecalInstance* dinst = gDecalManager->addDecal( rInfo.point, normal, tangent, mDataBlock->decalData, getScale().y );
               if(dinst)
                  dinst->mPalette = this->getPalette();
            }
            
            // Emit footpuffs.

            if( rInfo.t <= 0.5 && mWaterCoverage == 0.0
                && material && material->mShowDust )
            {
               // New emitter every time for visibility reasons
               ParticleEmitter * emitter = new ParticleEmitter;
               emitter->setPalette(this->getPalette());
               emitter->onNewDataBlock( mDataBlock->footPuffEmitter, false );

               ColorF colorList[ ParticleData::PDC_NUM_KEYS];

               for( U32 x = 0; x < getMin( Material::NUM_EFFECT_COLOR_STAGES, ParticleData::PDC_NUM_KEYS ); ++ x )
                  colorList[ x ].set( material->mEffectColor[ x ].red,
                                      material->mEffectColor[ x ].green,
                                      material->mEffectColor[ x ].blue,
                                      material->mEffectColor[ x ].alpha );
               for( U32 x = Material::NUM_EFFECT_COLOR_STAGES; x < ParticleData::PDC_NUM_KEYS; ++ x )
                  colorList[ x ].set( 1.0, 1.0, 1.0, 0.0 );

               emitter->setColors( colorList );
               if( !emitter->registerObject() )
               {
                  Con::warnf( ConsoleLogEntry::General, "Could not register emitter for particle of class: %s", mDataBlock->getName() );
                  delete emitter;
                  emitter = NULL;
               }
               else
               {
                  emitter->emitParticles( pos, Point3F( 0.0, 0.0, 1.0 ), mDataBlock->footPuffRadius,
                     Point3F( 0, 0, 0 ), mDataBlock->footPuffNumParts );
                  emitter->deleteWhenEmpty();
               }
            }

            // Play footstep sound.
            
            playFootstepSound( triggeredLeft, material, rInfo.object );
         }
      }
   }

   // Mount pending variable puts a hold on the delayTicks below so players don't
   // inadvertently stand up because their mount has not come over yet.
   if (mMountPending)
      mMountPending = (isMounted() ? 0 : (mMountPending - 1));

   if (mActionAnimation.action == PlayerData::NullAnimation ||
       ((!mActionAnimation.waitForEnd || mActionAnimation.atEnd)) &&
       !mActionAnimation.holdAtEnd && (mActionAnimation.delayTicks -= !mMountPending) <= 0)
   {
      //The scripting language will get a call back when a script animation has finished...
      //  example: When the chat menu animations are done playing...
      if ( isServerObject() && mActionAnimation.action >= PlayerData::NumTableActionAnims )
         mDataBlock->animationDone_callback( this );
      pickActionAnimation();
   }

   if ( (mActionAnimation.action != PlayerData::LandAnim) &&
        (mActionAnimation.action != PlayerData::NullAnimation) )
   {
      // Update action animation time scale to match ground velocity
      PlayerData::ActionAnimation &anim =
         mDataBlock->actionList[mActionAnimation.action];
      F32 scale = 1;
      if (anim.velocityScale && anim.speed) {
         VectorF vel;
         mWorldToObj.mulV(mVelocity,&vel);
         scale = mFabs(mDot(vel, anim.dir) / anim.speed);

         if (scale > mDataBlock->maxTimeScale)
            scale = mDataBlock->maxTimeScale;
      }

      mShapeInstance->setTimeScale(mActionAnimation.thread,
                                   mActionAnimation.forward? scale: -scale);
   }
   PROFILE_END();
}

void Player::pickBestMoveAction(U32 startAnim, U32 endAnim, U32 * action, bool * forward) const
{
   *action = startAnim;
   *forward = false;

   VectorF vel;
   mWorldToObj.mulV(mVelocity,&vel);

   if (vel.lenSquared() > 0.01f)
   {
      // Bias the velocity towards picking the forward/backward anims over
      // the sideways ones to prevent oscillation between anims.
      vel *= VectorF(0.5f, 1.0f, 0.5f);

      // Pick animation that is the best fit for our current (local) velocity.
      // Assumes that the root (stationary) animation is at startAnim.
      F32 curMax = -0.1f;
      for (U32 i = startAnim+1; i <= endAnim; i++)
      {
         const PlayerData::ActionAnimation &anim = mDataBlock->actionList[i];
         if (anim.sequence != -1 && anim.speed) 
         {
            F32 d = mDot(vel, anim.dir);
            if (d > curMax)
            {
               curMax = d;
               *action = i;
               *forward = true;
            }
            else
            {
               // Check if reversing this animation would fit (bias against this
               // so that when moving right, the real right anim is still chosen,
               // but if not present, the reversed left anim will be used instead)
               d *= -0.75f;
               if (d > curMax)
               {
                  curMax = d;
                  *action = i;
                  *forward = false;
               }
            }
         }
      }
   }
}

void Player::pickActionAnimation()
{
   // Only select animations in our normal move state.
   if (mState != MoveState || mDamageState != Enabled)
      return;

   if (isMounted() || mMountPending)
   {
      // Go into root position unless something was set explicitly
      // from a script.
      if (mActionAnimation.action != PlayerData::RootAnim &&
          mActionAnimation.action < PlayerData::NumTableActionAnims)
         setActionThread(PlayerData::RootAnim,true,false,false);
      return;
   }

   bool forward = true;
   U32 action = PlayerData::RootAnim;
   bool fsp = true;
   
   // Jetting overrides the fall animation condition
   if (mJetting)
   {
      // Play the jetting animation
      action = PlayerData::JetAnim;
   }
	else if(this->isSliding() || this->isSkidding())
	{
		action = PlayerData::SlideAnim;
	}
   else if (mFalling)
   {
      // Not in contact with any surface and falling
      action = PlayerData::FallAnim;
   }
   else if ( mSwimming )
   {
      pickBestMoveAction(PlayerData::SwimRootAnim, PlayerData::SwimRightAnim, &action, &forward);
   }
   else if ( mPose == StandPose )
   {
      if (mContactTimer >= sContactTickTime)
      {
         // Nothing under our feet
         action = PlayerData::RootAnim;
      }
      else
      {
         // Our feet are on something
         pickBestMoveAction(PlayerData::RootAnim, PlayerData::SideRightAnim, &action, &forward);
      }
   }
   else if ( mPose == CrouchPose )
   {
      pickBestMoveAction(PlayerData::CrouchRootAnim, PlayerData::CrouchRightAnim, &action, &forward);
   }
   else if ( mPose == PronePose )
   {
      pickBestMoveAction(PlayerData::ProneRootAnim, PlayerData::ProneBackwardAnim, &action, &forward);
   }
   else if ( mPose == SprintPose )
   {
      pickBestMoveAction(PlayerData::SprintRootAnim, PlayerData::SprintRightAnim, &action, &forward);
   }

   setActionThread(action,forward,false,false,fsp);
}

void Player::onImage(U32 imageSlot, bool unmount)
{
   // Update 3rd person sequences based on images used.  Start be getting a
   // list of all possible image prefix sequences.
   String prefixPaths[ShapeBase::MaxMountedImages];
   buildImagePrefixPaths(prefixPaths);

   // Clear out any previous image state animation
   if (mImageStateThread)
   {
      mShapeInstance->destroyThread(mImageStateThread);
      mImageStateThread = 0;
   }

   // Attempt to update the action thread
   U32 action = mActionAnimation.action;
   if (action != PlayerData::NullAnimation)
   {
      String actionSeq = mDataBlock->actionList[action].name;
      if (actionSeq.isNotEmpty())
      {
         S32 seqIndex = mDataBlock->actionList[action].sequence;
         S32 prefixIndex = findPrefixSequence(prefixPaths, actionSeq);
         if (prefixIndex != -1)
         {
            seqIndex = prefixIndex;
         }

         // Only change the sequence if it isn't already playing.
         if (seqIndex != mShapeInstance->getSequence(mActionAnimation.thread))
         {
            F32 pos = mShapeInstance->getPos(mActionAnimation.thread);
            mShapeInstance->setSequence(mActionAnimation.thread, seqIndex, pos);
         }
      }
   }

   // Attempt to update the arm thread
   U32 armAction = getArmAction();
   if (armAction != PlayerData::NullAnimation)
   {
      String armSeq = mDataBlock->actionList[armAction].name;
      if (armSeq.isNotEmpty())
      {
         S32 seqIndex = mDataBlock->actionList[armAction].sequence;
         S32 prefixIndex = findPrefixSequence(prefixPaths, armSeq);
         if (prefixIndex != -1)
         {
            seqIndex = prefixIndex;
         }

         // Only change the sequence if it isn't already playing.
         if (seqIndex != mShapeInstance->getSequence(mArmAnimation.thread))
         {
            F32 pos = mShapeInstance->getPos(mArmAnimation.thread);
            mShapeInstance->setSequence(mArmAnimation.thread, seqIndex, pos);
         }
      }
   }

   // Attempt to update the head threads
   if (mHeadVThread)
   {
      TSShape const* shape = mShapeInstance->getShape();
      S32 seqIndex = shape->findSequence("head");
      S32 prefixIndex = findPrefixSequence(prefixPaths, "head");
      if (prefixIndex != -1)
      {
         seqIndex = prefixIndex;
      }

      // Only change the sequence if it isn't already playing.
      if (seqIndex != mShapeInstance->getSequence(mHeadVThread))
      {
         F32 pos = mShapeInstance->getPos(mHeadVThread);
         mShapeInstance->setSequence(mHeadVThread, seqIndex, pos);
      }
   }

   if (mHeadHThread)
   {
      TSShape const* shape = mShapeInstance->getShape();
      S32 seqIndex = shape->findSequence("headside");
      S32 prefixIndex = findPrefixSequence(prefixPaths, "headside");
      if (prefixIndex != -1)
      {
         seqIndex = prefixIndex;
      }

      // Only change the sequence if it isn't already playing.
      if (seqIndex != mShapeInstance->getSequence(mHeadHThread))
      {
         F32 pos = mShapeInstance->getPos(mHeadHThread);
         mShapeInstance->setSequence(mHeadHThread, seqIndex, pos);
      }
   }
}

void Player::buildImagePrefixPaths(String* prefixPaths)
{
   // We begin obtaining the anim prefix for each image.
   String prefix[ShapeBase::MaxMountedImages];
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      MountedImage& image = mMountedImageList[i];
      if (image.dataBlock && image.dataBlock->imageAnimPrefix && image.dataBlock->imageAnimPrefix[0])
      {
         prefix[i] = String(image.dataBlock->imageAnimPrefix);
      }
   }

   // Build out the full prefix names we will be searching for.
   S32 counter = ShapeBase::MaxMountedImages-1;
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      // Only build out the prefix path for images that have a defined prefix.
      if (prefix[i].isNotEmpty())
      {
         bool start = true;
         for (U32 j=0; j<=i; ++j)
         {
            if (prefix[j].isNotEmpty())
            {
               if (!start)
               {
                  prefixPaths[counter] += "_";
               }
               else
               {
                  start = false;
               }
               prefixPaths[counter] += prefix[j];
            }
         }
      }

      -- counter;
   }
}
S32 Player::findPrefixSequence(String* prefixPaths, const String& baseSeq)
{
   // Go through the prefix list.  If we find a match then return the sequence
   // index.
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      if (prefixPaths[i].isNotEmpty())
      {
         String seq = prefixPaths[i] + "_" + baseSeq;
         S32 seqIndex = mShapeInstance->getShape()->findSequence(seq);
         if (seqIndex != -1)
         {
            return seqIndex;
         }
      }
   }

   return -1;
}

S32 Player::convertActionToImagePrefix(U32 action)
{
   String prefixPaths[ShapeBase::MaxMountedImages];
   buildImagePrefixPaths(prefixPaths);

   if (action != PlayerData::NullAnimation)
   {
      String actionSeq;
      S32 seq = -1;

      // We'll first attempt to find the action sequence by name
      // as defined within the action list.
      actionSeq = mDataBlock->actionList[action].name;
      if (actionSeq.isNotEmpty())
      {
         seq = findPrefixSequence(prefixPaths, actionSeq);
      }

      if (seq == -1)
      {
         // Couldn't find a valid sequence.  If this is a sprint action
         // then we also need to search through the standard movement
         // sequences.
         if (action >= PlayerData::SprintRootAnim && action <= PlayerData::SprintRightAnim)
         {
            U32 standardAction = action - PlayerData::SprintRootAnim;
            actionSeq = mDataBlock->actionList[standardAction].name;
            if (actionSeq.isNotEmpty())
            {
               seq = findPrefixSequence(prefixPaths, actionSeq);
            }
         }
      }

      return seq;
   }

   return -1;
}

void Player::onImageRecoil( U32, ShapeBaseImageData::StateData::RecoilState state )
{
   if ( mRecoilThread )
   {
      if ( state != ShapeBaseImageData::StateData::NoRecoil )
      {
         S32 stateIndex = state - ShapeBaseImageData::StateData::LightRecoil;
         if ( mDataBlock->recoilSequence[stateIndex] != -1 )
         {
            mShapeInstance->setSequence( mRecoilThread, mDataBlock->recoilSequence[stateIndex], 0 );
            mShapeInstance->setTimeScale( mRecoilThread, 1 );
         }
      }
   }
}

void Player::onImageStateAnimation(U32 imageSlot, const char* seqName, bool direction, bool scaleToState, F32 stateTimeOutValue)
{
   if (mDataBlock->allowImageStateAnimation && isGhost())
   {
      MountedImage& image = mMountedImageList[imageSlot];

      // Just as with onImageAnimThreadChange we're going to apply various prefixes to determine the final sequence to use.
      // Here is the order:
      // imageBasePrefix_scriptPrefix_baseAnimName
      // imageBasePrefix_baseAnimName
      // scriptPrefix_baseAnimName
      // baseAnimName

      // Collect the prefixes
      const char* imageBasePrefix = "";
      bool hasImageBasePrefix = image.dataBlock && image.dataBlock->imageAnimPrefix && image.dataBlock->imageAnimPrefix[0];
      if (hasImageBasePrefix)
         imageBasePrefix = image.dataBlock->imageAnimPrefix;
      const char* scriptPrefix = getImageScriptAnimPrefix(imageSlot).getString();
      bool hasScriptPrefix = scriptPrefix && scriptPrefix[0];

      S32 seqIndex = mShapeInstance->getShape()->findSequence(seqName);

      // Find the final sequence based on the prefix combinations
      if (hasImageBasePrefix || hasScriptPrefix)
      {
         bool found = false;
         String baseSeqName(seqName);

         if (!found && hasImageBasePrefix && hasScriptPrefix)
         {
            String seqName = String(imageBasePrefix) + String("_") + String(scriptPrefix) + String("_") + baseSeqName;
            S32 index = mShapeInstance->getShape()->findSequence(seqName);
            if (index != -1)
            {
               seqIndex = index;
               found = true;
            }
         }

         if (!found && hasImageBasePrefix)
         {
            String seqName = String(imageBasePrefix) + String("_") + baseSeqName;
            S32 index = mShapeInstance->getShape()->findSequence(seqName);
            if (index != -1)
            {
               seqIndex = index;
               found = true;
            }
         }

         if (!found && hasScriptPrefix)
         {
            String seqName = String(scriptPrefix) + String("_") + baseSeqName;
            S32 index = mShapeInstance->getShape()->findSequence(seqName);
            if (index != -1)
            {
               seqIndex = index;
               found = true;
            }
         }
      }

      if (seqIndex != -1)
      {
         if (!mImageStateThread)
         {
            mImageStateThread = mShapeInstance->addThread();
         }

         mShapeInstance->setSequence( mImageStateThread, seqIndex, 0 );

         F32 timeScale = (scaleToState && stateTimeOutValue) ?
            mShapeInstance->getDuration(mImageStateThread) / stateTimeOutValue : 1.0f;

         mShapeInstance->setTimeScale( mImageStateThread, direction ? timeScale : -timeScale );
      }
   }
}

const char* Player::getImageAnimPrefix(U32 imageSlot, S32 imageShapeIndex)
{
   if (!mDataBlock)
      return "";

   switch (imageShapeIndex)
   {
      case ShapeBaseImageData::StandardImageShape:
      {
         return mDataBlock->imageAnimPrefix;
      }

      case ShapeBaseImageData::FirstPersonImageShape:
      {
         return mDataBlock->imageAnimPrefixFP;
      }

      default:
      {
         return "";
      }
   }
}

void Player::onImageAnimThreadChange(U32 imageSlot, S32 imageShapeIndex, ShapeBaseImageData::StateData* lastState, const char* anim, F32 pos, F32 timeScale, bool reset)
{
   if (!mShapeFPInstance[imageSlot] || !mShapeFPAnimThread[imageSlot])
      return;

   MountedImage& image = mMountedImageList[imageSlot];
   ShapeBaseImageData::StateData& stateData = *image.state;

   if (reset)
   {
      // Reset cyclic sequences back to the first frame to turn it off
      // (the first key frame should be it's off state).
      if (mShapeFPAnimThread[imageSlot]->getSequence()->isCyclic() && (stateData.sequenceNeverTransition || !(stateData.sequenceTransitionIn || (lastState && lastState->sequenceTransitionOut))) )
      {
         mShapeFPInstance[imageSlot]->setPos(mShapeFPAnimThread[imageSlot],0);
         mShapeFPInstance[imageSlot]->setTimeScale(mShapeFPAnimThread[imageSlot],0);
      }

      return;
   }

   // Just as with ShapeBase::udpateAnimThread we're going to apply various prefixes to determine the final sequence to use.
   // Here is the order:
   // imageBasePrefix_scriptPrefix_baseAnimName
   // imageBasePrefix_baseAnimName
   // scriptPrefix_baseAnimName
   // baseAnimName

   // Collect the prefixes
   const char* imageBasePrefix = "";
   bool hasImageBasePrefix = image.dataBlock && image.dataBlock->imageAnimPrefixFP && image.dataBlock->imageAnimPrefixFP[0];
   if (hasImageBasePrefix)
      imageBasePrefix = image.dataBlock->imageAnimPrefixFP;
   const char* scriptPrefix = getImageScriptAnimPrefix(imageSlot).getString();
   bool hasScriptPrefix = scriptPrefix && scriptPrefix[0];

   S32 seqIndex = mShapeFPInstance[imageSlot]->getShape()->findSequence(anim);

   // Find the final sequence based on the prefix combinations
   if (hasImageBasePrefix || hasScriptPrefix)
   {
      bool found = false;
      String baseSeqName(anim);

      if (!found && hasImageBasePrefix && hasScriptPrefix)
      {
         String seqName = String(imageBasePrefix) + String("_") + String(scriptPrefix) + String("_") + baseSeqName;
         S32 index = mShapeFPInstance[imageSlot]->getShape()->findSequence(seqName);
         if (index != -1)
         {
            seqIndex = index;
            found = true;
         }
      }

      if (!found && hasImageBasePrefix)
      {
         String seqName = String(imageBasePrefix) + String("_") + baseSeqName;
         S32 index = mShapeFPInstance[imageSlot]->getShape()->findSequence(seqName);
         if (index != -1)
         {
            seqIndex = index;
            found = true;
         }
      }

      if (!found && hasScriptPrefix)
      {
         String seqName = String(scriptPrefix) + String("_") + baseSeqName;
         S32 index = mShapeFPInstance[imageSlot]->getShape()->findSequence(seqName);
         if (index != -1)
         {
            seqIndex = index;
            found = true;
         }
      }
   }

   if (seqIndex != -1)
   {
      if (!lastState)
      {
         // No lastState indicates that we are just switching animation sequences, not states.  Transition into this new sequence, but only
         // if it is different than what we're currently playing.
         S32 prevSeq = -1;
         if (mShapeFPAnimThread[imageSlot]->hasSequence())
         {
            prevSeq = mShapeFPInstance[imageSlot]->getSequence(mShapeFPAnimThread[imageSlot]);
         }
         if (seqIndex != prevSeq)
         {
            mShapeFPInstance[imageSlot]->transitionToSequence(mShapeFPAnimThread[imageSlot], seqIndex, pos, image.dataBlock->scriptAnimTransitionTime, true);
         }
      }
      else if (!stateData.sequenceNeverTransition && stateData.sequenceTransitionTime && (stateData.sequenceTransitionIn || (lastState && lastState->sequenceTransitionOut)) )
      {
         mShapeFPInstance[imageSlot]->transitionToSequence(mShapeFPAnimThread[imageSlot], seqIndex, pos, stateData.sequenceTransitionTime, true);
      }
      else
      {
         mShapeFPInstance[imageSlot]->setSequence(mShapeFPAnimThread[imageSlot], seqIndex, pos);
      }
      mShapeFPInstance[imageSlot]->setTimeScale(mShapeFPAnimThread[imageSlot], timeScale);
   }
}

void Player::onImageAnimThreadUpdate(U32 imageSlot, S32 imageShapeIndex, F32 dt)
{
   if (!mShapeFPInstance[imageSlot])
      return;

   if (mShapeFPAmbientThread[imageSlot] && mShapeFPAmbientThread[imageSlot]->hasSequence())
   {
      mShapeFPInstance[imageSlot]->advanceTime(dt,mShapeFPAmbientThread[imageSlot]);
   }

   if (mShapeFPAnimThread[imageSlot] && mShapeFPAnimThread[imageSlot]->hasSequence())
   {
      mShapeFPInstance[imageSlot]->advanceTime(dt,mShapeFPAnimThread[imageSlot]);
   }
}

void Player::onUnmount( ShapeBase *obj, S32 node )
{
   // Reset back to root position during dismount.
   setActionThread(PlayerData::RootAnim,true,false,false);

   // Re-orient the player straight up
   Point3F pos,vec;
   getTransform().getColumn(1,&vec);
   getTransform().getColumn(3,&pos);
   Point3F rot(0.0f,0.0f,-mAtan2(-vec.x,vec.y));
   setPosition(pos,rot);

   // Parent function will call script
   Parent::onUnmount( obj, node );
}

void Player::unmount()
{
   // Reset back to root position during dismount.  This copies what is
   // done on the server and corrects the fact that the RootAnim change
   // is not sent across to the client using the standard ActionMask.
   setActionThread(PlayerData::RootAnim,true,false,false);

   Parent::unmount();
}


//----------------------------------------------------------------------------

void Player::updateAnimation(F32 dt)
{
   // If dead then remove any image animations
   if ((mDamageState == Disabled || mDamageState == Destroyed) && mImageStateThread)
   {
      // Remove the image state animation
      mShapeInstance->destroyThread(mImageStateThread);
      mImageStateThread = 0;
   }

   if ((isGhost() || mActionAnimation.animateOnServer) && mActionAnimation.thread)
      mShapeInstance->advanceTime(dt,mActionAnimation.thread);
   if (mRecoilThread)
      mShapeInstance->advanceTime(dt,mRecoilThread);
   if (mImageStateThread)
      mShapeInstance->advanceTime(dt,mImageStateThread);

	// Wings animation
   if(mShapeInstance && mWingsThread)
   {
      bool open = (!mRunSurface || this->isSliding());
      mShapeInstance->advanceTime(open ? dt : -dt, mWingsThread);
   }

   // If we are the client's player on this machine, then we need
   // to make sure the transforms are up to date as they are used
   // to setup the camera.
   if (isGhost())
   {
      if (getControllingClient())
      {
         updateAnimationTree(isFirstPerson());
         mShapeInstance->animate();
      }
      else
      {
         updateAnimationTree(false);
      }
   }
}

void Player::updateAnimationTree(bool firstPerson)
{
   S32 mode = 0;
   if (firstPerson)
      if (mActionAnimation.firstPerson)
         mode = 0;
//            TSShapeInstance::MaskNodeRotation;
//            TSShapeInstance::MaskNodePosX |
//            TSShapeInstance::MaskNodePosY;
      else
         mode = TSShapeInstance::MaskNodeAllButBlend;

   for (U32 i = 0; i < PlayerData::NumSpineNodes; i++)
      if (mDataBlock->spineNode[i] != -1)
         mShapeInstance->setNodeAnimationState(mDataBlock->spineNode[i],mode);
}


//----------------------------------------------------------------------------

bool Player::step(Point3F *pos,F32 *maxStep,F32 time)
{
   const Point3F& scale = getScale();
   Box3F box;
   VectorF offset = mVelocity * time;
   box.minExtents = mObjBox.minExtents + offset + *pos;
   box.maxExtents = mObjBox.maxExtents + offset + *pos;
   box.maxExtents.z += mDataBlock->maxStepHeight * scale.z + sMinFaceDistance;

   SphereF sphere;
   sphere.center = (box.minExtents + box.maxExtents) * 0.5f;
   VectorF bv = box.maxExtents - sphere.center;
   sphere.radius = bv.len();

   ClippedPolyList polyList;
   polyList.mPlaneList.clear();
   polyList.mNormal.set(0.0f, 0.0f, 0.0f);
   polyList.mPlaneList.setSize(6);
   polyList.mPlaneList[0].set(box.minExtents,VectorF(-1.0f, 0.0f, 0.0f));
   polyList.mPlaneList[1].set(box.maxExtents,VectorF(0.0f, 1.0f, 0.0f));
   polyList.mPlaneList[2].set(box.maxExtents,VectorF(1.0f, 0.0f, 0.0f));
   polyList.mPlaneList[3].set(box.minExtents,VectorF(0.0f, -1.0f, 0.0f));
   polyList.mPlaneList[4].set(box.minExtents,VectorF(0.0f, 0.0f, -1.0f));
   polyList.mPlaneList[5].set(box.maxExtents,VectorF(0.0f, 0.0f, 1.0f));

   CollisionWorkingList& rList = mConvex.getWorkingList();
   CollisionWorkingList* pList = rList.wLink.mNext;
   while (pList != &rList) {
      Convex* pConvex = pList->mConvex;
      
      // Alright, here's the deal... a polysoup mesh really needs to be 
      // designed with stepping in mind.  If there are too many smallish polygons
      // the stepping system here gets confused and allows you to run up walls 
      // or on the edges/seams of meshes.

      TSStatic *st = dynamic_cast<TSStatic *> (pConvex->getObject());
      bool skip = false;
      if (st && !st->allowPlayerStep())
         skip = true;

      if ((pConvex->getObject()->getTypeMask() & StaticObjectType) != 0 && !skip)
      {
         Box3F convexBox = pConvex->getBoundingBox();
         if (box.isOverlapped(convexBox))
            pConvex->getPolyList(&polyList);
      }
      pList = pList->wLink.mNext;
   }

   // Find max step height
   F32 stepHeight = pos->z - sMinFaceDistance;
   U32* vp = polyList.mIndexList.begin();
   U32* ep = polyList.mIndexList.end();
   for (; vp != ep; vp++) {
      F32 h = polyList.mVertexList[*vp].point.z + sMinFaceDistance;
      if (h > stepHeight)
         stepHeight = h;
   }

   F32 step = stepHeight - pos->z;
   if (stepHeight > pos->z && step < *maxStep) {
      // Go ahead and step
      pos->z = stepHeight;
      *maxStep -= step;
      return true;
   }

   return false;
}


//----------------------------------------------------------------------------
inline Point3F createInterpPos(const Point3F& s, const Point3F& e, const F32 t, const F32 d)
{
   Point3F ret;
   ret.interpolate(s, e, t/d);
   return ret;
}

Point3F Player::_move( const F32 travelTime, Collision *outCol )
{
   // Try and move to new pos
   F32 totalMotion  = 0.0f;
   
   // TODO: not used?
   //F32 initialSpeed = mVelocity.len();

   Point3F start;
   Point3F initialPosition;
   getTransform().getColumn(3,&start);
   initialPosition = start;

   static CollisionList collisionList;
   static CollisionList physZoneCollisionList;

   collisionList.clear();
   physZoneCollisionList.clear();

   MatrixF collisionMatrix(true);
   collisionMatrix.setColumn(3, start);

   VectorF firstNormal(0.0f, 0.0f, 0.0f);
   F32 maxStep = mDataBlock->maxStepHeight;
   F32 time = travelTime;
   U32 count = 0;

   const Point3F& scale = getScale();

   static Polyhedron sBoxPolyhedron;
   static ExtrudedPolyList sExtrudedPolyList;
   static ExtrudedPolyList sPhysZonePolyList;

   for (; count < sMoveRetryCount; count++) {
      F32 speed = mVelocity.len();
      if (!speed && !mDeath.haveVelocity())
         break;

      Point3F end = start + mVelocity * time;
      if (mDeath.haveVelocity()) {
         // Add in death movement-
         VectorF  deathVel = mDeath.getPosAdd();
         VectorF  resVel;
         getTransform().mulV(deathVel, & resVel);
         end += resVel;
      }
      else
      {
         // simulate stepping...
         bool justJumped = mJumpSurfaceLastContact == JumpSkipContactsMax;
         if(!this->isSliding() && mRunSurface && !justJumped && end.z != start.z)
         {
            RayInfo rInfo;
            if(getContainer()->castRay(end, Point3F(end.x, end.y, end.z - maxStep), sCollisionMoveMask, &rInfo))
               end = rInfo.point;
         }
      }
      Point3F distance = end - start;

      if (mFabs(distance.x) < mObjBox.len_x() &&
          mFabs(distance.y) < mObjBox.len_y() &&
          mFabs(distance.z) < mObjBox.len_z())
      {
         // We can potentially early out of this.  If there are no polys in the clipped polylist at our
         //  end position, then we can bail, and just set start = end;
         Box3F wBox = mScaledBox;
         wBox.minExtents += end;
         wBox.maxExtents += end;

         static EarlyOutPolyList eaPolyList;
         eaPolyList.clear();
         eaPolyList.mNormal.set(0.0f, 0.0f, 0.0f);
         eaPolyList.mPlaneList.clear();
         eaPolyList.mPlaneList.setSize(6);
         eaPolyList.mPlaneList[0].set(wBox.minExtents,VectorF(-1.0f, 0.0f, 0.0f));
         eaPolyList.mPlaneList[1].set(wBox.maxExtents,VectorF(0.0f, 1.0f, 0.0f));
         eaPolyList.mPlaneList[2].set(wBox.maxExtents,VectorF(1.0f, 0.0f, 0.0f));
         eaPolyList.mPlaneList[3].set(wBox.minExtents,VectorF(0.0f, -1.0f, 0.0f));
         eaPolyList.mPlaneList[4].set(wBox.minExtents,VectorF(0.0f, 0.0f, -1.0f));
         eaPolyList.mPlaneList[5].set(wBox.maxExtents,VectorF(0.0f, 0.0f, 1.0f));

         // Build list from convex states here...
         CollisionWorkingList& rList = mConvex.getWorkingList();
         CollisionWorkingList* pList = rList.wLink.mNext;
         while (pList != &rList) {
            Convex* pConvex = pList->mConvex;
            if (pConvex->getObject()->getTypeMask() & sCollisionMoveMask) {
               Box3F convexBox = pConvex->getBoundingBox();
               if (wBox.isOverlapped(convexBox))
               {
                  // No need to separate out the physical zones here, we want those
                  //  to cause a fallthrough as well...
                  pConvex->getPolyList(&eaPolyList);
               }
            }
            pList = pList->wLink.mNext;
         }

         if (eaPolyList.isEmpty())
         {
            totalMotion += (end - start).len();
            start = end;
            break;
         }
      }

      collisionMatrix.setColumn(3, start);
      sBoxPolyhedron.buildBox(collisionMatrix, mScaledBox, true);

      // Setup the bounding box for the extrudedPolyList
      Box3F plistBox = mScaledBox;
      collisionMatrix.mul(plistBox);
      Point3F oldMin = plistBox.minExtents;
      Point3F oldMax = plistBox.maxExtents;
      plistBox.minExtents.setMin(oldMin + (mVelocity * time) - Point3F(0.1f, 0.1f, 0.1f));
      plistBox.maxExtents.setMax(oldMax + (mVelocity * time) + Point3F(0.1f, 0.1f, 0.1f));

      // Build extruded polyList...
      VectorF vector = end - start;
      sExtrudedPolyList.extrude(sBoxPolyhedron,vector);
      sExtrudedPolyList.setVelocity(mVelocity);
      sExtrudedPolyList.setCollisionList(&collisionList);

      sPhysZonePolyList.extrude(sBoxPolyhedron,vector);
      sPhysZonePolyList.setVelocity(mVelocity);
      sPhysZonePolyList.setCollisionList(&physZoneCollisionList);

      // Build list from convex states here...
      CollisionWorkingList& rList = mConvex.getWorkingList();
      CollisionWorkingList* pList = rList.wLink.mNext;
      while (pList != &rList) {
         Convex* pConvex = pList->mConvex;
         if (pConvex->getObject()->getTypeMask() & sCollisionMoveMask) {
            Box3F convexBox = pConvex->getBoundingBox();
            if (plistBox.isOverlapped(convexBox))
            {
               if (pConvex->getObject()->getTypeMask() & PhysicalZoneObjectType)
                  pConvex->getPolyList(&sPhysZonePolyList);
               else
                  pConvex->getPolyList(&sExtrudedPolyList);
            }
         }
         pList = pList->wLink.mNext;
      }

      // Take into account any physical zones...
      for (U32 j = 0; j < physZoneCollisionList.getCount(); j++) 
      {
         AssertFatal(dynamic_cast<PhysicalZone*>(physZoneCollisionList[j].object), "Bad phys zone!");
         const PhysicalZone* pZone = (PhysicalZone*)physZoneCollisionList[j].object;
         if (pZone->isActive())
            mVelocity *= pZone->getVelocityMod();
      }

      if (collisionList.getCount() != 0 && collisionList.getTime() < 1.0f) 
      {
         // Set to collision point
         F32 velLen = mVelocity.len();

         F32 dt = time * getMin(collisionList.getTime(), 1.0f);
         start += mVelocity * dt;
         time -= dt;

         totalMotion += velLen * dt;

         bool wasFalling = mFalling;
         mFalling = false;

         // Back off...
         if ( velLen > 0.f ) {
            F32 newT = getMin(0.01f / velLen, dt);
            start -= mVelocity * newT;
            totalMotion -= velLen * newT;
         }

         // Try stepping if there is a vertical surface
         if (collisionList.getMaxHeight() < start.z + mDataBlock->maxStepHeight * scale.z) 
         {
            bool stepped = false;
            for (U32 c = 0; c < collisionList.getCount(); c++) 
            {
               const Collision& cp = collisionList[c];
               // if (mFabs(mDot(cp.normal,VectorF(0,0,1))) < sVerticalStepDot)
               //    Dot with (0,0,1) just extracts Z component [lh]-
               if (mFabs(cp.normal.z) < sVerticalStepDot)
               {
                  stepped = step(&start,&maxStep,time);
                  break;
               }
            }
            if (stepped)
            {
               continue;
            }
         }

         // Pick the surface most parallel to the face that was hit.
         const Collision *collision = &collisionList[0];
         const Collision *cp = collision + 1;
         const Collision *ep = collision + collisionList.getCount();
         for (; cp != ep; cp++)
         {
            if (cp->faceDot > collision->faceDot)
               collision = cp;
         }

         F32 bd = _doCollisionImpact( collision, wasFalling );

         // Copy this collision out so
         // we can use it to do impacts
         // and query collision.
         *outCol = *collision;

         // Subtract out velocity
         VectorF dv = collision->normal * (bd + sNormalElasticity);
         mVelocity += dv;
         if (count == 0)
         {
            firstNormal = collision->normal;
         }
         else
         {
            if (count == 1)
            {
               // Re-orient velocity along the crease.
               if (mDot(dv,firstNormal) < 0.0f &&
                   mDot(collision->normal,firstNormal) < 0.0f)
               {
                  VectorF nv;
                  mCross(collision->normal,firstNormal,&nv);
                  F32 nvl = nv.len();
                  if (nvl)
                  {
                     if (mDot(nv,mVelocity) < 0.0f)
                        nvl = -nvl;
                     nv *= mVelocity.len() / nvl;
                     mVelocity = nv;
                  }
               }
            }
         }
      }
      else
      {
         totalMotion += (end - start).len();
         start = end;
         break;
      }
   }

   if (count == sMoveRetryCount)
   {
      // Failed to move
      start = initialPosition;
      mVelocity.set(0.0f, 0.0f, 0.0f);
   }

   return start;
}

F32 Player::_doCollisionImpact( const Collision *collision, bool fallingCollision)
{
   F32 bd = -mDot( mVelocity, collision->normal);

   // shake camera on ground impact
   if( bd > mDataBlock->groundImpactMinSpeed && isControlObject() )
   {
      F32 ampScale = (bd - mDataBlock->groundImpactMinSpeed) / mDataBlock->minImpactSpeed;

      CameraShake *groundImpactShake = new CameraShake;
      groundImpactShake->setDuration( mDataBlock->groundImpactShakeDuration );
      groundImpactShake->setFrequency( mDataBlock->groundImpactShakeFreq );

      VectorF shakeAmp = mDataBlock->groundImpactShakeAmp * ampScale;
      groundImpactShake->setAmplitude( shakeAmp );
      groundImpactShake->setFalloff( mDataBlock->groundImpactShakeFalloff );
      groundImpactShake->init();
      gCamFXMgr.addFX( groundImpactShake );
   }

   if ( ((bd > mDataBlock->minImpactSpeed && fallingCollision) || bd > mDataBlock->minLateralImpactSpeed) 
      && !mMountPending )
   {
      if ( !isGhost() )
         onImpact( collision->object, collision->normal * bd );

      if (mDamageState == Enabled && mState != RecoverState) 
      {
         // Scale how long we're down for
         if (mDataBlock->landSequenceTime > 0.0f)
         {
            // Recover time is based on the land sequence
            setState(RecoverState);
         }
         else
         {
            // Legacy recover system
            F32   value = (bd - mDataBlock->minImpactSpeed);
            F32   range = (mDataBlock->minImpactSpeed * 0.9f);
            U32   recover = mDataBlock->recoverDelay;
            if (value < range)
               recover = 1 + S32(mFloor( F32(recover) * value / range) );
            //Con::printf("Used %d recover ticks", recover);
            //Con::printf("  minImpact = %g, this one = %g", mDataBlock->minImpactSpeed, bd);
            setState(RecoverState, recover);
         }
      }
   }

   if ( isServerObject() && 
      (bd > (mDataBlock->minImpactSpeed / 3.0f) || bd > (mDataBlock->minLateralImpactSpeed / 3.0f )) ) 
   {
      mImpactSound = PlayerData::ImpactNormal;
      setMaskBits(ImpactMask);
   }

   return bd;
}

void Player::_handleCollision( const Collision &collision )
{
   // Track collisions
   if (  !isGhost() && 
         collision.object && 
         collision.object != mContactInfo.contactObject )
      queueCollision( collision.object, mVelocity - collision.object->getVelocity() );
}

bool Player::updatePos(const F32 travelTime)
{
   PROFILE_SCOPE(Player_UpdatePos);
   getTransform().getColumn(3,&delta.posVec);

   // When mounted to another object, only Z rotation used.
   if (isMounted()) {
      mVelocity = mMount.object->getVelocity();
      setPosition(Point3F(0.0f, 0.0f, 0.0f), mRot);
      setMaskBits(MoveMask);
      return true;
   }

   Point3F newPos;

   Collision col;
   dMemset( &col, 0, sizeof( col ) );

   // DEBUG:
   //Point3F savedVelocity = mVelocity;

   if ( mPhysicsRep )
   {
      static CollisionList collisionList;
      collisionList.clear();

      newPos = mPhysicsRep->move( mVelocity * travelTime, collisionList );

      bool haveCollisions = false;
      bool wasFalling = mFalling;
      if (collisionList.getCount() > 0)
      {
         mFalling = false;
         haveCollisions = true;
      }

      if (haveCollisions)
      {
         // Pick the collision that most closely matches our direction
         VectorF velNormal = mVelocity;
         velNormal.normalizeSafe();
         const Collision *collision = &collisionList[0];
         F32 collisionDot = mDot(velNormal, collision->normal);
         const Collision *cp = collision + 1;
         const Collision *ep = collision + collisionList.getCount();
         for (; cp != ep; cp++)
         {
            F32 dp = mDot(velNormal, cp->normal);
            if (dp < collisionDot)
            {
               collisionDot = dp;
               collision = cp;
            }
         }

         _doCollisionImpact( collision, wasFalling );

         // Modify our velocity based on collisions
         for (U32 i=0; i<collisionList.getCount(); ++i)
         {
            F32 bd = -mDot( mVelocity, collisionList[i].normal );
            VectorF dv = collisionList[i].normal * (bd + sNormalElasticity);
            mVelocity += dv;
         }

         // Store the last collision for use later on.  The handle collision
         // code only expects a single collision object.
         if (collisionList.getCount() > 0)
            col = collisionList[collisionList.getCount() - 1];

         // We'll handle any player-to-player collision, and the last collision
         // with other obejct types.
         for (U32 i=0; i<collisionList.getCount(); ++i)
         {
            Collision& colCheck = collisionList[i];
            if (colCheck.object)
            {
               SceneObject* obj = static_cast<SceneObject*>(col.object);
               if (obj->getTypeMask() & PlayerObjectType)
               {
                  _handleCollision( colCheck );
               }
               else
               {
                  col = colCheck;
               }
            }
         }

         _handleCollision( col );
      }
   }
   else
   {
      if ( mVelocity.isZero() )
         newPos = delta.posVec;
      else
         newPos = _move( travelTime, &col );
   
      _handleCollision( col );
   }

   // DEBUG:
   //if ( isClientObject() )
   //   Con::printf( "(client) vel: %g %g %g", mVelocity.x, mVelocity.y, mVelocity.z );
   //else
   //   Con::printf( "(server) vel: %g %g %g", mVelocity.x, mVelocity.y, mVelocity.z );

   // Set new position
   // If on the client, calc delta for backstepping
   if (isClientObject())
   {
      delta.pos = newPos;
      delta.posVec = delta.posVec - delta.pos;
      delta.dt = 1.0f;
   }

   setPosition( newPos, mRot );
   setMaskBits( MoveMask );
   updateContainer();

   if (!isGhost())  
   {
      // Collisions are only queued on the server and can be
      // generated by either updateMove or updatePos
      notifyCollision();

      // Do mission area callbacks on the server as well
      checkMissionArea();
   }

   // Check the total distance moved.  If it is more than 1000th of the velocity, then
   //  we moved a fair amount...
   //if (totalMotion >= (0.001f * initialSpeed))
      return true;
   //else
      //return false;
}


//----------------------------------------------------------------------------
 
void Player::_findContact( SceneObject **contactObject, 
                           VectorF *contactNormal, 
                           Vector<SceneObject*> *outOverlapObjects )
{
   Point3F pos;
   getTransform().getColumn(3,&pos);

   Box3F wBox;
   Point3F exp(0,0,sTractionDistance);
   wBox.minExtents = pos + mScaledBox.minExtents - exp;
   wBox.maxExtents.x = pos.x + mScaledBox.maxExtents.x;
   wBox.maxExtents.y = pos.y + mScaledBox.maxExtents.y;
   wBox.maxExtents.z = pos.z + mScaledBox.minExtents.z + sTractionDistance;

   static ClippedPolyList polyList;
   polyList.clear();
   polyList.doConstruct();
   polyList.mNormal.set(0.0f, 0.0f, 0.0f);
   polyList.setInterestNormal(Point3F(0.0f, 0.0f, -1.0f));

   polyList.mPlaneList.setSize(6);
   polyList.mPlaneList[0].setYZ(wBox.minExtents, -1.0f);
   polyList.mPlaneList[1].setXZ(wBox.maxExtents, 1.0f);
   polyList.mPlaneList[2].setYZ(wBox.maxExtents, 1.0f);
   polyList.mPlaneList[3].setXZ(wBox.minExtents, -1.0f);
   polyList.mPlaneList[4].setXY(wBox.minExtents, -1.0f);
   polyList.mPlaneList[5].setXY(wBox.maxExtents, 1.0f);
   Box3F plistBox = wBox;

   // Expand build box as it will be used to collide with items.
   // PickupRadius will be at least the size of the box.
   F32 pd = (F32)mDataBlock->pickupDelta;
   wBox.minExtents.x -= pd; wBox.minExtents.y -= pd;
   wBox.maxExtents.x += pd; wBox.maxExtents.y += pd;
   wBox.maxExtents.z = pos.z + mScaledBox.maxExtents.z;

   // Build list from convex states here...
   CollisionWorkingList& rList = mConvex.getWorkingList();
   CollisionWorkingList* pList = rList.wLink.mNext;
   while (pList != &rList)
   {
      Convex* pConvex = pList->mConvex;

      U32 objectMask = pConvex->getObject()->getTypeMask();
      
      if (  ( objectMask & sCollisionMoveMask ) &&
            !( objectMask & PhysicalZoneObjectType ) )
      {
         Box3F convexBox = pConvex->getBoundingBox();
         if (plistBox.isOverlapped(convexBox))
            pConvex->getPolyList(&polyList);
      }
      else
         outOverlapObjects->push_back( pConvex->getObject() );

      pList = pList->wLink.mNext;
   }

   if (!polyList.isEmpty())
   {
      // Pick flattest surface
      F32 bestVd = -1.0f;
      ClippedPolyList::Poly* poly = polyList.mPolyList.begin();
      ClippedPolyList::Poly* end = polyList.mPolyList.end();
      for (; poly != end; poly++)
      {
         F32 vd = poly->plane.z;       // i.e.  mDot(Point3F(0,0,1), poly->plane);
         if (vd > bestVd)
         {
            bestVd = vd;
            *contactObject = poly->object;
            *contactNormal = poly->plane;
         }
      }
   }
}

void Player::findContact( bool *run, bool *jump, VectorF *contactNormal )
{
   SceneObject *contactObject = NULL;

   Vector<SceneObject*> overlapObjects;
   if ( mPhysicsRep )
      mPhysicsRep->findContact( &contactObject, contactNormal, &overlapObjects );
   else
      _findContact( &contactObject, contactNormal, &overlapObjects );

   // Check for triggers, corpses and items.
   const U32 filterMask = isGhost() ? sClientCollisionContactMask : sServerCollisionContactMask;
   for ( U32 i=0; i < overlapObjects.size(); i++ )
   {
      SceneObject *obj = overlapObjects[i];
      U32 objectMask = obj->getTypeMask();

      if ( !( objectMask & filterMask ) )
         continue;

      // Check: triggers, tactical zones, corpses and items...
      //
      if (objectMask & TriggerObjectType)
      {
         Trigger* pTrigger = static_cast<Trigger*>( obj );
         pTrigger->potentialEnterObject(this);
      }
      else if (objectMask & TacticalZoneObjectType)
      {
         TacticalZone* pZone = static_cast<TacticalZone*>( obj );
         pZone->potentialEnterObject(this);
      }
      else if (objectMask & CorpseObjectType)
      {
         // If we've overlapped the worldbounding boxes, then that's it...
         if ( getWorldBox().isOverlapped( obj->getWorldBox() ) )
         {
            ShapeBase* col = static_cast<ShapeBase*>( obj );
            queueCollision(col,getVelocity() - col->getVelocity());
         }
      }
      else if (objectMask & ItemObjectType)
      {
         // If we've overlapped the worldbounding boxes, then that's it...
         Item* item = static_cast<Item*>( obj );
         if (  getWorldBox().isOverlapped(item->getWorldBox()) &&
               item->getCollisionObject() != this && 
               !item->isHidden() )
            queueCollision(item,getVelocity() - item->getVelocity());
      }
   }

   F32 vd = (*contactNormal).z;
   *run = vd > mDataBlock->runSurfaceCos;
   *jump = vd > mDataBlock->jumpSurfaceCos;

   mContactInfo.clear();
  
   mContactInfo.contacted = contactObject != NULL;
   mContactInfo.contactObject = contactObject;

   if ( mContactInfo.contacted )
      mContactInfo.contactNormal = *contactNormal;

   mContactInfo.run = *run;
   mContactInfo.jump = *jump;
}

//----------------------------------------------------------------------------

void Player::checkMissionArea()
{
   // Checks to see if the player is in the Mission Area...
   Point3F pos;
   MissionArea * obj = MissionArea::getServerObject();

   if(!obj)
      return;

   const RectI &area = obj->getArea();
   getTransform().getColumn(3, &pos);

   if ((pos.x < area.point.x || pos.x > area.point.x + area.extent.x ||
       pos.y < area.point.y || pos.y > area.point.y + area.extent.y)) {
      if(mInMissionArea) {
         mInMissionArea = false;
         mDataBlock->onLeaveMissionArea_callback( this );
      }
   }
   else if(!mInMissionArea)
   {
      mInMissionArea = true;
      mDataBlock->onEnterMissionArea_callback( this );
   }
}


//----------------------------------------------------------------------------

bool Player::isDisplacable() const
{
   return true;
}

Point3F Player::getMomentum() const
{
   return mVelocity * getMass();
}

void Player::setMomentum(const Point3F& newMomentum)
{
   Point3F newVelocity = newMomentum / getMass();
   mVelocity = newVelocity;
}

#define  LH_HACK   1
// Hack for short-term soln to Training crash -
#if   LH_HACK
static U32  sBalance;

bool Player::displaceObject(const Point3F& displacement)
{
   F32 vellen = mVelocity.len();
   if (vellen < 0.001f || sBalance > 16) {
      mVelocity.set(0.0f, 0.0f, 0.0f);
      return false;
   }

   F32 dt = displacement.len() / vellen;

   sBalance++;

   bool result = updatePos(dt);

   sBalance--;

   getTransform().getColumn(3, &delta.pos);
   delta.posVec.set(0.0f, 0.0f, 0.0f);

   return result;
}

#else

bool Player::displaceObject(const Point3F& displacement)
{
   F32 vellen = mVelocity.len();
   if (vellen < 0.001f) {
      mVelocity.set(0.0f, 0.0f, 0.0f);
      return false;
   }

   F32 dt = displacement.len() / vellen;

   bool result = updatePos(dt);

   mObjToWorld.getColumn(3, &delta.pos);
   delta.posVec.set(0.0f, 0.0f, 0.0f);

   return result;
}

#endif

//----------------------------------------------------------------------------

void Player::setPosition(const Point3F& pos,const Point3F& rot)
{
   F32 rotZ = rot.z;

   if(this->useInstantInput())
      rotZ = mInstantInput.rot.z;

   MatrixF mat;
   if (isMounted()) {
      // Use transform from mounted object
      MatrixF nmat,zrot;
      mMount.object->getMountTransform( mMount.node, mMount.xfm, &nmat );
      zrot.set(EulerF(0.0f, 0.0f, rotZ));
      mat.mul(nmat,zrot);
   }
   else {
      mat.set(EulerF(0.0f, 0.0f, rotZ));
      mat.setColumn(3,pos);
   }
   Parent::setTransform(mat);
   mRot = rot;

   if ( mPhysicsRep )
      mPhysicsRep->setTransform( mat );
}


void Player::setRenderPosition(const Point3F& pos, const Point3F& rot, F32 dt)
{
   F32 rotZ = rot.z;

   if(this->useInstantInput())
      rotZ = mInstantInput.rot.z;

   MatrixF mat;
   if (isMounted()) {
      // Use transform from mounted object
      MatrixF nmat,zrot;
      mMount.object->getRenderMountTransform( dt, mMount.node, mMount.xfm, &nmat );
      zrot.set(EulerF(0.0f, 0.0f, rotZ));
      mat.mul(nmat,zrot);
   }
   else {
      EulerF   orient(0.0f, 0.0f, rotZ);

      mat.set(orient);
      mat.setColumn(3, pos);

      if (inDeathAnim()) {
         F32   boxRad = (mDataBlock->boxSize.x * 0.5f);
         if (MatrixF * fallMat = mDeath.fallToGround(dt, pos, rot.z, boxRad))
            mat = * fallMat;
      }
      else
         mDeath.initFall();
   }
   Parent::setRenderTransform(mat);
}

//----------------------------------------------------------------------------

void Player::setTransform(const MatrixF& mat)
{
   // This method should never be called on the client.

   // This currently converts all rotation in the mat into
   // rotations around the z axis.
   Point3F pos,vec;
   mat.getColumn(1,&vec);
   mat.getColumn(3,&pos);
   Point3F rot(0.0f, 0.0f, -mAtan2(-vec.x,vec.y));
   setPosition(pos,rot);
   setMaskBits(MoveMask | NoWarpMask);
}

void Player::getEyeTransform(MatrixF* mat)
{
   getEyeBaseTransform(mat, true);

   // The shape instance is animated in getEyeBaseTransform() so we're
   // good here when attempting to get the eye node position on the server.
   S32 imageIndex = -1;
   S32 shapeIndex = -1;
   MountedImage* image = NULL;
   ShapeBaseImageData* data = NULL;
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      image = &(mMountedImageList[i]);
      if (image->dataBlock) 
      {
         data = image->dataBlock;
         shapeIndex = getImageShapeIndex(*image);
         if ( data->useEyeNode && (data->animateOnServer || isGhost()) && isFirstPerson() && data->eyeMountNode[shapeIndex] != -1 && data->eyeNode[shapeIndex] != -1 )
         {
            imageIndex = i;
            break;
         }
      }
   }

   if (imageIndex >= 0)
   {
      // Get the image's eye node's position relative to the eye mount node
      MatrixF mountTransform = image->shapeInstance[shapeIndex]->mNodeTransforms[data->eyeMountNode[shapeIndex]];
      Point3F eyeMountNodePos = mountTransform.getPosition();
      mountTransform = image->shapeInstance[shapeIndex]->mNodeTransforms[data->eyeNode[shapeIndex]];
      Point3F eyeNodePos = mountTransform.getPosition() - eyeMountNodePos;

      // Now transform to the image's eye node (position only)
      MatrixF xfm(true);
      xfm.setPosition(eyeNodePos);
      mat->mul(xfm);
   }
}

void Player::getEyeBaseTransform(MatrixF* mat, bool includeBank)
{
   // Eye transform in world space.  We only use the eye position
   // from the animation and supply our own rotation.
   MatrixF pmat,xmat,zmat;

   if(!isGhost()) 
      mShapeInstance->animate();

   Point3F head = mHead;

   if(this->useInstantInput())
      head = mInstantInput.head;

   xmat.set(EulerF(head.x, 0.0f, 0.0f));

   if (mUseHeadZCalc)
      zmat.set(EulerF(0.0f, 0.0f, head.z));
   else
      zmat.identity();

   if(includeBank && mDataBlock->cameraCanBank)
   {
      // Take mHead.y into account to bank the camera
      MatrixF imat;
      imat.mul(zmat, xmat);
      MatrixF ymat;
      ymat.set(EulerF(0.0f, head.y, 0.0f));
      pmat.mul(imat, ymat);
   }
   else
   {
      pmat.mul(zmat,xmat);
   }

   F32 *dp = pmat;

   F32* sp;
   MatrixF eyeMat(true);
   if (mDataBlock->eyeNode != -1)
   {
      sp = mShapeInstance->mNodeTransforms[mDataBlock->eyeNode];
   }
   else
   {
      Point3F center;
      mObjBox.getCenter(&center);
      eyeMat.setPosition(center);
      sp = eyeMat;
   }

   const Point3F& scale = getScale();
   dp[3] = sp[3] * scale.x;
   dp[7] = sp[7] * scale.y;
   dp[11] = sp[11] * scale.z;
   mat->mul(getTransform(),pmat);
}

void Player::getRenderEyeTransform(MatrixF* mat)
{
   getRenderEyeBaseTransform(mat, true);

   // Use the first image that is set to use the eye node
   for (U32 i=0; i<ShapeBase::MaxMountedImages; ++i)
   {
      MountedImage& image = mMountedImageList[i];
      if (image.dataBlock) 
      {
         ShapeBaseImageData& data = *image.dataBlock;
         U32 shapeIndex = getImageShapeIndex(image);
         if ( data.useEyeNode && isFirstPerson() && data.eyeMountNode[shapeIndex] != -1 && data.eyeNode[shapeIndex] != -1 )
         {
            // Get the eye node's position relative to the eye mount node
            MatrixF mountTransform = image.shapeInstance[shapeIndex]->mNodeTransforms[data.eyeMountNode[shapeIndex]];
            Point3F eyeMountNodePos = mountTransform.getPosition();
            mountTransform = image.shapeInstance[shapeIndex]->mNodeTransforms[data.eyeNode[shapeIndex]];
            Point3F eyeNodePos = mountTransform.getPosition() - eyeMountNodePos;

            // Now transform to the image's eye node (position only)
            MatrixF xfm(true);
            xfm.setPosition(eyeNodePos);
            mat->mul(xfm);

            return;
         }
      }
   }
}

void Player::getRenderEyeBaseTransform(MatrixF* mat, bool includeBank)
{
   // Eye transform in world space.  We only use the eye position
   // from the animation and supply our own rotation.
   MatrixF pmat,xmat,zmat;

   F32 headX = delta.head.x + delta.headVec.x * delta.dt;
   F32 headY = delta.head.y + delta.headVec.y * delta.dt;
   F32 headZ = delta.head.z + delta.headVec.z * delta.dt;
   if(this->useInstantInput())
   {
      headX = mInstantInput.head.x;
      headY = mInstantInput.head.y;
      headZ = mInstantInput.head.z;
   }

   xmat.set(EulerF(headX, 0.0f, 0.0f));

   if (mUseHeadZCalc)
      zmat.set(EulerF(0.0f, 0.0f, headZ));
   else
      zmat.identity();

   if(includeBank && mDataBlock->cameraCanBank)
   {
      // Take mHead.y delta into account to bank the camera
      MatrixF imat;
      imat.mul(zmat, xmat);
      MatrixF ymat;
      ymat.set(EulerF(0.0f, headY, 0.0f));
      pmat.mul(imat, ymat);
   }
   else
   {
      pmat.mul(zmat,xmat);
   }

   F32 *dp = pmat;

   F32* sp;
   MatrixF eyeMat(true);
   if (mDataBlock->eyeNode != -1)
   {
      sp = mShapeInstance->mNodeTransforms[mDataBlock->eyeNode];
   }
   else
   {
      // Use the center of the Player's bounding box for the eye position.
      Point3F center;
      mObjBox.getCenter(&center);
      eyeMat.setPosition(center);
      sp = eyeMat;
   }

   // Only use position of eye node, and take Player's scale
   // into account.
   const Point3F& scale = getScale();
   dp[3] = sp[3] * scale.x;
   dp[7] = sp[7] * scale.y;
   dp[11] = sp[11] * scale.z;

   mat->mul(getRenderTransform(), pmat);
}

void Player::getMuzzleTransform(U32 imageSlot,MatrixF* mat)
{
   disableHeadZCalc();

   MatrixF nmat;
   Parent::getRetractionTransform(imageSlot,&nmat);
   MatrixF smat;
   Parent::getImageTransform(imageSlot,&smat);

   disableCollision();

   // See if we are pushed into a wall...
   if (getDamageState() == Enabled) {
      Point3F start, end;
      smat.getColumn(3, &start);
      nmat.getColumn(3, &end);

      RayInfo rinfo;
      if (getContainer()->castRay(start, end, sCollisionMoveMask, &rinfo)) {
         Point3F finalPoint;
         finalPoint.interpolate(start, end, rinfo.t);
         nmat.setColumn(3, finalPoint);
      }
      else
         Parent::getMuzzleTransform(imageSlot,&nmat);
   }
   else
      Parent::getMuzzleTransform(imageSlot,&nmat);

   enableCollision();

   enableHeadZCalc();
  
   *mat = nmat;
}


void Player::getRenderMuzzleTransform(U32 imageSlot,MatrixF* mat)
{
   disableHeadZCalc();

   MatrixF nmat;
   Parent::getRenderRetractionTransform(imageSlot,&nmat);
   MatrixF smat;
   Parent::getRenderImageTransform(imageSlot,&smat);

   disableCollision();

   // See if we are pushed into a wall...
   if (getDamageState() == Enabled)
   {
      Point3F start, end;
      smat.getColumn(3, &start);
      nmat.getColumn(3, &end);

      RayInfo rinfo;
      if (getContainer()->castRay(start, end, sCollisionMoveMask, &rinfo)) {
         Point3F finalPoint;
         finalPoint.interpolate(start, end, rinfo.t);
         nmat.setColumn(3, finalPoint);
      }
      else
      {
         Parent::getRenderMuzzleTransform(imageSlot,&nmat);
      }
   }
   else
   {
      Parent::getRenderMuzzleTransform(imageSlot,&nmat);
   }

   enableCollision();

   enableHeadZCalc();
  
   *mat = nmat;
}

void Player::getMuzzleVector(U32 imageSlot,VectorF* vec)
{
   MatrixF mat;
   getMuzzleTransform(imageSlot,&mat);

   GameConnection * gc = getControllingClient();
   if (gc && !gc->isAIControlled())
   {
      MountedImage& image = mMountedImageList[imageSlot];

      bool fp = gc->isFirstPerson();
      if ((fp && image.dataBlock->correctMuzzleVector) ||
         (!fp && image.dataBlock->correctMuzzleVectorTP))
      {
         disableHeadZCalc();
         if (getCorrectedAim(mat, vec))
         {
            enableHeadZCalc();
            return;
         }
         enableHeadZCalc();
      }
   }

   mat.getColumn(1,vec);
}

void Player::renderMountedImage( U32 imageSlot, TSRenderState &rstate, SceneRenderState *state )
{
   GFX->pushWorldMatrix();

   MatrixF world;

   MountedImage& image = mMountedImageList[imageSlot];
   ShapeBaseImageData& data = *image.dataBlock;
   U32 imageShapeIndex;
   if ( state->isShadowPass() )
   {
      // Force the standard image shapes for the shadow pass.
      imageShapeIndex = ShapeBaseImageData::StandardImageShape;
   }
   else
   {
      imageShapeIndex = getImageShapeIndex(image);
   }

   if ( !state->isShadowPass() && isFirstPerson() && (data.useEyeOffset || (data.useEyeNode && data.eyeMountNode[imageShapeIndex] != -1)) ) 
   {
      if (data.useEyeNode && data.eyeMountNode[imageShapeIndex] != -1)
      {
         MatrixF nmat;
         getRenderEyeBaseTransform(&nmat, mDataBlock->mountedImagesBank);
         MatrixF offsetMat = image.shapeInstance[imageShapeIndex]->mNodeTransforms[data.eyeMountNode[imageShapeIndex]];
         offsetMat.affineInverse();
         world.mul(nmat,offsetMat);
      }
      else
      {
         MatrixF nmat;
         getRenderEyeBaseTransform(&nmat, mDataBlock->mountedImagesBank);
         MatrixF offset = data.eyeOffset;
         offset.setPosition(offset.getPosition() + mFirstPersonWeaponBob.pos);
         world.mul(nmat, offset);
      }

      if ( imageSlot == 0 )
      {
         MatrixF nmat;
         MatrixF smat;

         getRenderRetractionTransform(0,&nmat);         
         getRenderImageTransform(0,&smat);

         // See if we are pushed into a wall...
         Point3F start, end;
         smat.getColumn(3, &start);
         nmat.getColumn(3, &end);

         Point3F displace = (start - end) * mWeaponBackFraction;

         world.setPosition( world.getPosition() + displace );
      }
   }
   else 
   {
      MatrixF nmat;
      getRenderMountTransform( 0.0f, data.mountPoint, MatrixF::Identity, &nmat);
      world.mul(nmat,data.mountTransform[imageShapeIndex]);
   }

   GFX->setWorldMatrix( world );

   image.shapeInstance[imageShapeIndex]->animate();
   image.shapeInstance[imageShapeIndex]->render( rstate );

   // Render the first person mount image shape?
   if (!state->isShadowPass() && imageShapeIndex == ShapeBaseImageData::FirstPersonImageShape && mShapeFPInstance[imageSlot])
   {
      mShapeFPInstance[imageSlot]->animate();
      mShapeFPInstance[imageSlot]->render( rstate );
   }

   GFX->popWorldMatrix();
}

// Bot aiming code calls this frequently and will work fine without the check
// for being pushed into a wall, which shows up on profile at ~ 3% (eight bots)
void Player::getMuzzlePointAI(U32 imageSlot, Point3F* point)
{
   MatrixF nmat;
   Parent::getMuzzleTransform(imageSlot, &nmat);

   // If we are in one of the standard player animations, adjust the
   // muzzle to point in the direction we are looking.
   if (mActionAnimation.action < PlayerData::NumTableActionAnims)
   {
      MatrixF xmat;
      xmat.set(EulerF(mHead.x, 0, 0));
      MatrixF result;
      result.mul(getTransform(), xmat);
      F32 *sp = nmat, *dp = result;
      dp[3] = sp[3]; dp[7] = sp[7]; dp[11] = sp[11];
      result.getColumn(3, point);
   }
   else
      nmat.getColumn(3, point);
}

void Player::getCameraParameters(F32 *min,F32* max,Point3F* off,MatrixF* rot)
{
   if (!mControlObject.isNull() && mControlObject == getObjectMount()) {
      mControlObject->getCameraParameters(min,max,off,rot);
      return;
   }
   const Point3F& scale = getScale();
   *min = mDataBlock->cameraMinDist * scale.y;
   *max = mDataBlock->cameraMaxDist * scale.y;
   off->set(0.0f, 0.0f, 0.0f);
   rot->identity();
}


//----------------------------------------------------------------------------

Point3F Player::getVelocity() const
{
   return mVelocity;
}

F32 Player::getSpeed() const
{
   return mVelocity.len();
}

void Player::setVelocity(const VectorF& vel)
{
	AssertFatal( !mIsNaN( vel ), "Player::setVelocity() - The velocity is NaN!" );

   mVelocity = vel;
   setMaskBits(MoveMask);
}

void Player::applyImpulse(const Point3F&,const VectorF& vec)
{
	AssertFatal( !mIsNaN( vec ), "Player::applyImpulse() - The vector is NaN!" );

   // Players ignore angular velocity
   VectorF vel;
   vel.x = vec.x / getMass();
   vel.y = vec.y / getMass();
   vel.z = vec.z / getMass();

   // Make sure the impulse isn't too big
   F32 len = vel.magnitudeSafe();
   if (len > sMaxImpulseVelocity)
   {
      Point3F excess = vel * ( 1.0f - (sMaxImpulseVelocity / len ) );
      vel -= excess;
   }

   setVelocity(mVelocity + vel);
}


//----------------------------------------------------------------------------

bool Player::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   if (getDamageState() != Enabled)
      return false;

   // Collide against bounding box. Need at least this for the editor.
   F32 st,et,fst = 0.0f,fet = 1.0f;
   F32 *bmin = &mObjBox.minExtents.x;
   F32 *bmax = &mObjBox.maxExtents.x;
   F32 const *si = &start.x;
   F32 const *ei = &end.x;

   for (S32 i = 0; i < 3; i++) {
      if (*si < *ei) {
         if (*si > *bmax || *ei < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si < *bmin)? (*bmin - *si) / di: 0.0f;
         et = (*ei > *bmax)? (*bmax - *si) / di: 1.0f;
      }
      else {
         if (*ei > *bmax || *si < *bmin)
            return false;
         F32 di = *ei - *si;
         st = (*si > *bmax)? (*bmax - *si) / di: 0.0f;
         et = (*ei < *bmin)? (*bmin - *si) / di: 1.0f;
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

static MatrixF IMat(1);

bool Player::buildPolyList(PolyListContext, AbstractPolyList* polyList, const Box3F&, const SphereF&)
{
   // Collision with the player is always against the player's object
   // space bounding box axis aligned in world space.
   Point3F pos;
   getTransform().getColumn(3,&pos);
   IMat.setColumn(3,pos);
   polyList->setTransform(&IMat, Point3F(1.0f,1.0f,1.0f));
   polyList->setObject(this);
   polyList->addBox(mObjBox);
   return true;
}


void Player::buildConvex(const Box3F& box, Convex* convex)
{
   if (mShapeInstance == NULL)
      return;

   // These should really come out of a pool
   mConvexList->collectGarbage();

   Box3F realBox = box;
   mWorldToObj.mul(realBox);
   realBox.minExtents.convolveInverse(mObjScale);
   realBox.maxExtents.convolveInverse(mObjScale);

   if (realBox.isOverlapped(getObjBox()) == false)
      return;

   Convex* cc = 0;
   CollisionWorkingList& wl = convex->getWorkingList();
   for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
      if (itr->mConvex->getType() == BoxConvexType &&
          itr->mConvex->getObject() == this) {
         cc = itr->mConvex;
         break;
      }
   }
   if (cc)
      return;

   // Create a new convex.
   BoxConvex* cp = new OrthoBoxConvex;
   mConvexList->registerObject(cp);
   convex->addToWorkingList(cp);
   cp->init(this);

   mObjBox.getCenter(&cp->mCenter);
   cp->mSize.x = mObjBox.len_x() / 2.0f;
   cp->mSize.y = mObjBox.len_y() / 2.0f;
   cp->mSize.z = mObjBox.len_z() / 2.0f;
}


//----------------------------------------------------------------------------

void Player::updateWorkingCollisionSet()
{
   // First, we need to adjust our velocity for possible acceleration.  It is assumed
   // that we will never accelerate more than 20 m/s for gravity, plus 10 m/s for
   // jetting, and an equivalent 10 m/s for jumping.  We also assume that the
   // working list is updated on a Tick basis, which means we only expand our
   // box by the possible movement in that tick.
   Point3F scaledVelocity = mVelocity * TickSec;
   F32 len    = scaledVelocity.len();
   F32 newLen = len + (10.0f * TickSec);

   // Check to see if it is actually necessary to construct the new working list,
   // or if we can use the cached version from the last query.  We use the x
   // component of the min member of the mWorkingQueryBox, which is lame, but
   // it works ok.
   bool updateSet = false;

   Box3F convexBox = mConvex.getBoundingBox(getTransform(), getScale());
   F32 l = (newLen * 1.1f) + 0.1f;  // from Convex::updateWorkingList
   const Point3F  lPoint( l, l, l );
   convexBox.minExtents -= lPoint;
   convexBox.maxExtents += lPoint;

   // Check containment
   if (mWorkingQueryBox.minExtents.x != -1e9f)
   {
      if (mWorkingQueryBox.isContained(convexBox) == false)
         // Needed region is outside the cached region.  Update it.
         updateSet = true;
   }
   else
   {
      // Must update
      updateSet = true;
   }
   // Actually perform the query, if necessary
   if (updateSet == true) {
      const Point3F  twolPoint( 2.0f * l, 2.0f * l, 2.0f * l );
      mWorkingQueryBox = convexBox;
      mWorkingQueryBox.minExtents -= twolPoint;
      mWorkingQueryBox.maxExtents += twolPoint;

      disableCollision();
      mConvex.updateWorkingList(mWorkingQueryBox,
         isGhost() ? sClientCollisionContactMask : sServerCollisionContactMask);
      enableCollision();
   }
}


//----------------------------------------------------------------------------

void Player::writePacketData(GameConnection *connection, BitStream *stream)
{
   Parent::writePacketData(connection, stream);

   stream->writeInt(mState,NumStateBits);
   if (stream->writeFlag(mState == RecoverState))
      stream->writeInt(mRecoverTicks,PlayerData::RecoverDelayBits);
   if (stream->writeFlag(mJumpDelay > 0))
      stream->writeInt(mJumpDelay,PlayerData::JumpDelayBits);

   Point3F pos;
   getTransform().getColumn(3,&pos);
   if (stream->writeFlag(!isMounted())) {
      // Will get position from mount
      stream->setCompressionPoint(pos);
      stream->write(pos.x);
      stream->write(pos.y);
      stream->write(pos.z);
      stream->write(mVelocity.x);
      stream->write(mVelocity.y);
      stream->write(mVelocity.z);
      stream->writeInt(mJumpSurfaceLastContact > 15 ? 15 : mJumpSurfaceLastContact, 4);

      if (stream->writeFlag(!mAllowSprinting || !mAllowCrouching || !mAllowProne || !mAllowJumping || !mAllowJetJumping || !mAllowSwimming || !mAllowSliding || !mAllowChargedXJump || !mAllowInstantXJump))
      {
         stream->writeFlag(mAllowJumping);
         stream->writeFlag(mAllowJetJumping);
         stream->writeFlag(mAllowSprinting);
         stream->writeFlag(mAllowCrouching);
         stream->writeFlag(mAllowProne);
         stream->writeFlag(mAllowSwimming);
			stream->writeFlag(mAllowSliding);
         stream->writeFlag(mAllowChargedXJump);
         stream->writeFlag(mAllowInstantXJump);
      }
   }
   stream->write(mHead.x);
   if(stream->writeFlag(mDataBlock->cameraCanBank))
   {
      // Include mHead.y to allow for camera banking
      stream->write(mHead.y);
   }
   stream->write(mHead.z);
   stream->write(mRot.z);

   if (mControlObject) {
      S32 gIndex = connection->getGhostIndex(mControlObject);
      if (stream->writeFlag(gIndex != -1)) {
         stream->writeInt(gIndex,NetConnection::GhostIdBitSize);
         mControlObject->writePacketData(connection, stream);
      }
   }
   else
      stream->writeFlag(false);
}


void Player::readPacketData(GameConnection *connection, BitStream *stream)
{
   Parent::readPacketData(connection, stream);

   mState = (ActionState)stream->readInt(NumStateBits);
   if (stream->readFlag())
      mRecoverTicks = stream->readInt(PlayerData::RecoverDelayBits);
   if (stream->readFlag())
      mJumpDelay = stream->readInt(PlayerData::JumpDelayBits);
   else
      mJumpDelay = 0;

   Point3F pos,rot;
   if (stream->readFlag()) {
      // Only written if we are not mounted
      stream->read(&pos.x);
      stream->read(&pos.y);
      stream->read(&pos.z);
      stream->read(&mVelocity.x);
      stream->read(&mVelocity.y);
      stream->read(&mVelocity.z);
      stream->setCompressionPoint(pos);
      delta.pos = pos;
      mJumpSurfaceLastContact = stream->readInt(4);

      if (stream->readFlag())
      {
         mAllowJumping = stream->readFlag();
         mAllowJetJumping = stream->readFlag();
         mAllowSprinting = stream->readFlag();
         mAllowCrouching = stream->readFlag();
         mAllowProne = stream->readFlag();
         mAllowSwimming = stream->readFlag();
			mAllowSliding = stream->readFlag();
         mAllowChargedXJump = stream->readFlag();
         mAllowInstantXJump = stream->readFlag();
      }
      else
      {
         mAllowJumping = true;
         mAllowJetJumping = true;
         mAllowSprinting = true;
         mAllowCrouching = true;
         mAllowProne = true;
         mAllowSwimming = true;
			mAllowSliding = true;
         mAllowChargedXJump = true;
         mAllowInstantXJump = true;
      }
   }
   else
      pos = delta.pos;
   stream->read(&mHead.x);
   if(stream->readFlag())
   {
      // Include mHead.y to allow for camera banking
      stream->read(&mHead.y);
   }
   stream->read(&mHead.z);
   stream->read(&rot.z);
   rot.x = rot.y = 0;
   setPosition(pos,rot);
   delta.head = mHead;
   delta.rot = rot;

   if (stream->readFlag()) {
      S32 gIndex = stream->readInt(NetConnection::GhostIdBitSize);
      ShapeBase* obj = static_cast<ShapeBase*>(connection->resolveGhost(gIndex));
      setControlObject(obj);
      obj->readPacketData(connection, stream);
   }
   else
      setControlObject(0);
}

U32 Player::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   if (stream->writeFlag((mask & ImpactMask) && !(mask & InitialUpdateMask)))
      stream->writeInt(mImpactSound, PlayerData::ImpactBits);

   if (stream->writeFlag(mask & ActionMask &&
         mActionAnimation.action != PlayerData::NullAnimation &&
         mActionAnimation.action >= PlayerData::NumTableActionAnims)) {
      stream->writeInt(mActionAnimation.action,PlayerData::ActionAnimBits);
      stream->writeFlag(mActionAnimation.holdAtEnd);
      stream->writeFlag(mActionAnimation.atEnd);
      stream->writeFlag(mActionAnimation.firstPerson);
      if (!mActionAnimation.atEnd) {
         // If somewhere in middle on initial update, must send position-
         F32   where = mShapeInstance->getPos(mActionAnimation.thread);
         if (stream->writeFlag((mask & InitialUpdateMask) != 0 && where > 0))
            stream->writeSignedFloat(where, 6);
      }
   }

   if (stream->writeFlag(mask & ActionMask &&
         mArmAnimation.action != PlayerData::NullAnimation &&
         (!(mask & InitialUpdateMask) ||
         mArmAnimation.action != mDataBlock->lookAction))) {
      stream->writeInt(mArmAnimation.action,PlayerData::ActionAnimBits);
   }

   // The rest of the data is part of the control object packet update.
   // If we're controlled by this client, we don't need to send it.
   // we only need to send it if this is the initial update - in that case,
   // the client won't know this is the control object yet.
   if(stream->writeFlag(getControllingClient() == con && !(mask & InitialUpdateMask)))
      return(retMask);

   if (stream->writeFlag(mask & MoveMask))
   {
      stream->writeFlag(mWantsToSlide);
      stream->writeFlag(mRunSurface);
      stream->writeFlag(mFalling);

      stream->writeInt(mState,NumStateBits);
      if (stream->writeFlag(mState == RecoverState))
         stream->writeInt(mRecoverTicks,PlayerData::RecoverDelayBits);

      Point3F pos;
      getTransform().getColumn(3,&pos);
      stream->writeCompressedPoint(pos);
      F32 len = mVelocity.len();
      if(stream->writeFlag(len > 0.02f))
      {
         Point3F outVel = mVelocity;
         outVel *= 1.0f/len;
         stream->writeNormalVector(outVel, 10);
         len *= 32.0f;  // 5 bits of fraction
         if(len > 8191)
            len = 8191;
         stream->writeInt((S32)len, 13);
      }
      stream->writeFloat(mRot.z / M_2PI_F, 7);
      stream->writeSignedFloat(mHead.x / mDataBlock->maxLookAngle, 6);
      stream->writeSignedFloat(mHead.z / mDataBlock->maxFreelookAngle, 6);
      delta.move.pack(stream);
      stream->writeFlag(!(mask & NoWarpMask));
   }
   // Ghost need energy to predict reliably
   for(U32 i = 0; i < ShapeBaseData::MaxEnergySlots; i++)
      stream->writeFloat(getEnergyLevel(i) / mDataBlock->maxEnergy[i], EnergyLevelBits);
   return retMask;
}

void Player::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con,stream);

   if (stream->readFlag())
      mImpactSound = stream->readInt(PlayerData::ImpactBits);

   // Server specified action animation
   if (stream->readFlag()) {
      U32 action = stream->readInt(PlayerData::ActionAnimBits);
      bool hold = stream->readFlag();
      bool atEnd = stream->readFlag();
      bool fsp = stream->readFlag();

      F32   animPos = -1.0f;
      if (!atEnd && stream->readFlag())
         animPos = stream->readSignedFloat(6);

      if (isProperlyAdded()) {
         setActionThread(action,true,hold,true,fsp);
         bool  inDeath = inDeathAnim();
         if (atEnd)
         {
            mShapeInstance->clearTransition(mActionAnimation.thread);
            mShapeInstance->setPos(mActionAnimation.thread,
                                   mActionAnimation.forward? 1: 0);
            if (inDeath)
               mDeath.lastPos = 1.0f;
         }
         else if (animPos > 0) {
            mShapeInstance->setPos(mActionAnimation.thread, animPos);
            if (inDeath)
               mDeath.lastPos = animPos;
         }

         // mMountPending suppresses tickDelay countdown so players will sit until
         // their mount, or another animation, comes through (or 13 seconds elapses).
         mMountPending = (S32) (inSittingAnim() ? sMountPendingTickWait : 0);
      }
      else {
         mActionAnimation.action = action;
         mActionAnimation.holdAtEnd = hold;
         mActionAnimation.atEnd = atEnd;
         mActionAnimation.firstPerson = fsp;
      }
   }

   // Server specified arm animation
   if (stream->readFlag()) {
      U32 action = stream->readInt(PlayerData::ActionAnimBits);
      if (isProperlyAdded())
         setArmThread(action);
      else
         mArmAnimation.action = action;
   }

   // Done if controlled by client ( and not initial update )
   if(stream->readFlag())
      return;

   // MoveMask
   if (stream->readFlag()) {
      mPredictionCount = sMaxPredictionTicks;
      mWantsToSlide = stream->readFlag();
      mRunSurface = stream->readFlag();
      mFalling = stream->readFlag();

      ActionState actionState = (ActionState)stream->readInt(NumStateBits);
      if (stream->readFlag()) {
         mRecoverTicks = stream->readInt(PlayerData::RecoverDelayBits);
         setState(actionState, mRecoverTicks);
      }
      else
         setState(actionState);

      Point3F pos,rot;
      stream->readCompressedPoint(&pos);
      F32 speed = mVelocity.len();
      if(stream->readFlag())
      {
         stream->readNormalVector(&mVelocity, 10);
         mVelocity *= stream->readInt(13) / 32.0f;
      }
      else
      {
         mVelocity.set(0.0f, 0.0f, 0.0f);
      }
      
      rot.y = rot.x = 0.0f;
      rot.z = stream->readFloat(7) * M_2PI_F;
      mHead.x = stream->readSignedFloat(6) * mDataBlock->maxLookAngle;
      mHead.z = stream->readSignedFloat(6) * mDataBlock->maxFreelookAngle;
      delta.move.unpack(stream);

      delta.head = mHead;
      delta.headVec.set(0.0f, 0.0f, 0.0f);

      if (stream->readFlag() && isProperlyAdded())
      {
         // Determine number of ticks to warp based on the average
         // of the client and server velocities.
         delta.warpOffset = pos - delta.pos;
         F32 as = (speed + mVelocity.len()) * 0.5f * TickSec;
         F32 dt = (as > 0.00001f) ? delta.warpOffset.len() / as: sMaxWarpTicks;
         delta.warpTicks = (S32)((dt > sMinWarpTicks) ? getMax(mFloor(dt + 0.5f), 1.0f) : 0.0f);

         if (delta.warpTicks)
         {
            // Setup the warp to start on the next tick.
            if (delta.warpTicks > sMaxWarpTicks)
               delta.warpTicks = sMaxWarpTicks;
            delta.warpOffset /= (F32)delta.warpTicks;

            delta.rotOffset = rot - delta.rot;

            // Ignore small rotation differences
            if (mFabs(delta.rotOffset.z) < 0.001f)
               delta.rotOffset.z = 0;

            // Wrap rotation to +/-PI
            if(delta.rotOffset.z < - M_PI_F)
               delta.rotOffset.z += M_2PI_F;
            else if(delta.rotOffset.z > M_PI_F)
               delta.rotOffset.z -= M_2PI_F;

            delta.rotOffset /= (F32)delta.warpTicks;
         }
         else
         {
            // Going to skip the warp, server and client are real close.
            // Adjust the frame interpolation to move smoothly to the
            // new position within the current tick.
            Point3F cp = delta.pos + delta.posVec * delta.dt;
            if (delta.dt == 0) 
            {
               delta.posVec.set(0.0f, 0.0f, 0.0f);
               delta.rotVec.set(0.0f, 0.0f, 0.0f);
            }
            else
            {
               F32 dti = 1.0f / delta.dt;
               delta.posVec = (cp - pos) * dti;
               delta.rotVec.z = mRot.z - rot.z;

               if(delta.rotVec.z > M_PI_F)
                  delta.rotVec.z -= M_2PI_F;
               else if(delta.rotVec.z < -M_PI_F)
                  delta.rotVec.z += M_2PI_F;

               delta.rotVec.z *= dti;
            }
            delta.pos = pos;
            delta.rot = rot;
            setPosition(pos,rot);
         }
      }
      else 
      {
         // Set the player to the server position
         delta.pos = pos;
         delta.rot = rot;
         delta.posVec.set(0.0f, 0.0f, 0.0f);
         delta.rotVec.set(0.0f, 0.0f, 0.0f);
         delta.warpTicks = 0;
         delta.dt = 0.0f;
         setPosition(pos,rot);
      }
   }
   for(U32 i = 0; i < ShapeBaseData::MaxEnergySlots; i++)
   {
      F32 energy = stream->readFloat(EnergyLevelBits) * mDataBlock->maxEnergy[i];
      setEnergyLevel(energy, i);
   }
}


//----------------------------------------------------------------------------

DefineEngineMethod( Player, getPitch, F32, (),,
   "@brief Get the value of the player's current pitch.\n\n"
   "@return The current pitch value\n" )
{
   return object->getHeadRotation().x;
}

DefineEngineMethod( Player, getPose, const char*, (),,
   "@brief Get the name of the player's current pose.\n\n"

   "The pose is one of the following:\n\n<ul>"
   "<li>Stand - Standard movement pose.</li>"
   "<li>Sprint - Sprinting pose.</li>"
   "<li>Crouch - Crouch pose.</li>"
   "<li>Prone - Prone pose.</li>"
   "<li>Swim - Swimming pose.</li></ul>\n"

   "@return The current pose; one of: \"Stand\", \"Sprint\", \"Crouch\", \"Prone\", \"Swim\"\n" )
{
   return object->getPoseName();
}

DefineEngineMethod( Player, allowAllPoses, void, (),,
   "@brief Allow all poses a chance to occur.\n\n"
   "This method resets any poses that have manually been blocked from occuring.  "
   "This includes the regular pose states such as sprinting, crouch, being prone "
   "and swimming.  It also includes being able to jump and jet jump.  While this "
   "is allowing these poses to occur it doesn't mean that they all can due to other "
   "conditions.  We're just not manually blocking them from being allowed.\n"
	"@see allowSliding()\n"
   "@see allowJumping()\n"
   "@see allowJetJumping()\n"
   "@see allowChargedXJump()\n"
   "@see allowInstantXJump()\n"
   "@see allowSprinting()\n"
   "@see allowCrouching()\n"
   "@see allowProne()\n"
   "@see allowSwimming()\n" )
{
   object->allowAllPoses();
}

DefineEngineMethod( Player, allowSliding, void, (bool state),,
   "@brief Set if the Player is allowed to slide.\n\n"
   "The default is to allow sliding unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow sliding "
   "at any time.\n"
   "@param state Set to true to allow sliding, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowSliding(state);
}

DefineEngineMethod( Player, allowJumping, void, (bool state),,
   "@brief Set if the Player is allowed to jump.\n\n"
   "The default is to allow jumping unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow jumping "
   "at any time.\n"
   "@param state Set to true to allow jumping, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowJumping(state);
}

DefineEngineMethod( Player, allowJetJumping, void, (bool state),,
   "@brief Set if the Player is allowed to jet jump.\n\n"
   "The default is to allow jet jumping unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow jet jumping "
   "at any time.\n"
   "@param state Set to true to allow jet jumping, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowJetJumping(state);
}

DefineEngineMethod( Player, allowChargedXJump, void, (bool state),,
   "@brief Set if the Player is allowed to use charged x-jumps.\n\n"
   "The default is to allow charged x-jumps.\n"
   "This method is mainly used to disallow charged x-jumps "
   "at any time.\n"
   "@param state Set to true to allow charged x-jumping, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowChargedXJump(state);
}

DefineEngineMethod( Player, allowInstantXJump, void, (bool state),,
   "@brief Set if the Player is allowed to use instant x-jumps.\n\n"
   "The default is to allow instant x-jumps.\n"
   "This method is mainly used to disallow instant x-jumps "
   "at any time.\n"
   "@param state Set to true to allow instant x-jumping, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowInstantXJump(state);
}

DefineEngineMethod( Player, allowSprinting, void, (bool state),,
   "@brief Set if the Player is allowed to sprint.\n\n"
   "The default is to allow sprinting unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow sprinting "
   "at any time.\n"
   "@param state Set to true to allow sprinting, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowSprinting(state);
}

DefineEngineMethod( Player, allowCrouching, void, (bool state),,
   "@brief Set if the Player is allowed to crouch.\n\n"
   "The default is to allow crouching unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow crouching "
   "at any time.\n"
   "@param state Set to true to allow crouching, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowCrouching(state);
}

DefineEngineMethod( Player, allowProne, void, (bool state),,
   "@brief Set if the Player is allowed to go prone.\n\n"
   "The default is to allow being prone unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow going prone "
   "at any time.\n"
   "@param state Set to true to allow being prone, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowProne(state);
}

DefineEngineMethod( Player, allowSwimming, void, (bool state),,
   "@brief Set if the Player is allowed to swim.\n\n"
   "The default is to allow swimming unless there are other environmental concerns "
   "that prevent it.  This method is mainly used to explicitly disallow swimming "
   "at any time.\n"
   "@param state Set to true to allow swimming, false to disable it.\n"
   "@see allowAllPoses()\n" )
{
   object->allowSwimming(state);
}

//----------------------------------------------------------------------------

DefineEngineMethod( Player, getState, const char*, (),,
   "@brief Get the name of the player's current state.\n\n"

   "The state is one of the following:\n\n<ul>"
   "<li>Dead - The Player is dead.</li>"
   "<li>Mounted - The Player is mounted to an object such as a vehicle.</li>"
   "<li>Move - The Player is free to move.  The usual state.</li>"
   "<li>Recover - The Player is recovering from a fall.  See PlayerData::recoverDelay.</li></ul>\n"

   "@return The current state; one of: \"Dead\", \"Mounted\", \"Move\", \"Recover\"\n" )
{
   return object->getStateName();
}

DefineEngineMethod( Player, getDamageLocation, const char*, ( Point3F pos ),,
   "@brief Get the named damage location and modifier for a given world position.\n\n"

   "the Player object can simulate different hit locations based on a pre-defined set "
   "of PlayerData defined percentages.  These hit percentages divide up the Player's "
   "bounding box into different regions.  The diagram below demonstrates how the various "
   "PlayerData properties split up the bounding volume:\n\n"

   "<img src=\"images/player_damageloc.png\">\n\n"

   "While you may pass in any world position and getDamageLocation() will provide a best-fit "
   "location, you should be aware that this can produce some interesting results.  For example, "
   "any position that is above PlayerData::boxHeadPercentage will be considered a 'head' hit, even "
   "if the world position is high in the sky.  Therefore it may be wise to keep the passed in point "
   "to somewhere on the surface of, or within, the Player's bounding volume.\n\n"

   "@note This method will not return an accurate location when the player is "
   "prone or swimming.\n\n"

   "@param pos A world position for which to retrieve a body region on this player.\n"

   "@return a string containing two words (space separated strings), where the "
   "first is a location and the second is a modifier.\n\n"

   "Posible locations:<ul>"
   "<li>head</li>"
   "<li>torso</li>"
   "<li>legs</li></ul>\n"

   "Head modifiers:<ul>"
   "<li>left_back</li>"
   "<li>middle_back</li>"
   "<li>right_back</li>"
   "<li>left_middle</li>"
   "<li>middle_middle</li>"
   "<li>right_middle</li>"
   "<li>left_front</li>"
   "<li>middle_front</li>"
   "<li>right_front</li></ul>\n"

   "Legs/Torso modifiers:<ul>"
   "<li>front_left</li>"
   "<li>front_right</li>"
   "<li>back_left</li>"
   "<li>back_right</li></ul>\n"

   "@see PlayerData::boxHeadPercentage\n"
   "@see PlayerData::boxHeadFrontPercentage\n"
   "@see PlayerData::boxHeadBackPercentage\n"
   "@see PlayerData::boxHeadLeftPercentage\n"
   "@see PlayerData::boxHeadRightPercentage\n"
   "@see PlayerData::boxTorsoPercentage\n"
   )
{
   const char *buffer1;
   const char *buffer2;

   object->getDamageLocation(pos, buffer1, buffer2);

   static const U32 bufSize = 128;
   char *buff = Con::getReturnBuffer(bufSize);
   dSprintf(buff, bufSize, "%s %s", buffer1, buffer2);
   return buff;
}

DefineEngineMethod( Player, setArmThread, bool, ( const char* name ),,
   "@brief Set the sequence that controls the player's arms (dynamically adjusted "
   "to match look direction).\n\n"
   "@param name Name of the sequence to play on the player's arms.\n"
   "@return true if successful, false if failed.\n"
   "@note By default the 'look' sequence is used, if available.\n")
{
   return object->setArmThread( name );
}

DefineEngineMethod( Player, setActionThread, bool, ( const char* name, bool hold, bool fsp ), ( false, true ),
   "@brief Set the main action sequence to play for this player.\n\n"
   "@param name Name of the action sequence to set\n"
   "@param hold Set to false to get a callback on the datablock when the sequence ends (PlayerData::animationDone()).  "
   "When set to true no callback is made.\n"
   "@param fsp True if first person and none of the spine nodes in the shape should animate.  False will allow the shape's "
   "spine nodes to animate.\n"
   "@return True if succesful, false if failed\n"
   
   "@note The spine nodes for the Player's shape are named as follows:\n\n<ul>"
   "<li>Bip01 Pelvis</li>"
   "<li>Bip01 Spine</li>"
   "<li>Bip01 Spine1</li>"
   "<li>Bip01 Spine2</li>"
   "<li>Bip01 Neck</li>"
   "<li>Bip01 Head</li></ul>\n\n"
   
   "You cannot use setActionThread() to have the Player play one of the motion "
   "determined action animation sequences.  These sequences are chosen based on how "
   "the Player moves and the Player's current pose.  The names of these sequences are:\n\n<ul>"
   "<li>root</li>"
   "<li>run</li>"
   "<li>side</li>"
   "<li>side_right</li>"
   "<li>crouch_root</li>"
   "<li>crouch_forward</li>"
   "<li>crouch_backward</li>"
   "<li>crouch_side</li>"
   "<li>crouch_right</li>"
   "<li>prone_root</li>"
   "<li>prone_forward</li>"
   "<li>prone_backward</li>"
   "<li>swim_root</li>"
   "<li>swim_forward</li>"
   "<li>swim_backward</li>"
   "<li>swim_left</li>"
   "<li>swim_right</li>"
   "<li>fall</li>"
   "<li>jump</li>"
   "<li>standjump</li>"
   "<li>land</li>"
   "<li>jet</li></ul>\n\n"
   
   "If the player moves in any direction then the animation sequence set using this "
   "method will be cancelled and the chosen mation-based sequence will take over.  This makes "
   "great for times when the Player cannot move, such as when mounted, or when it doesn't matter "
   "if the action sequence changes, such as waving and saluting.\n"
   
   "@tsexample\n"
      "// Place the player in a sitting position after being mounted\n"
      "%player.setActionThread( \"sitting\", true, true );\n"
	"@endtsexample\n")
{
   return object->setActionThread( name, hold, true, fsp);
}

DefineEngineMethod( Player, setControlObject, bool, ( ShapeBase* obj ),,
   "@brief Set the object to be controlled by this player\n\n"

   "It is possible to have the moves sent to the Player object from the "
   "GameConnection to be passed along to another object.  This happens, for example "
   "when a player is mounted to a vehicle.  The move commands pass through the Player "
   "and on to the vehicle (while the player remains stationary within the vehicle).  "
   "With setControlObject() you can have the Player pass along its moves to any object.  "
   "One possible use is for a player to move a remote controlled vehicle.  In this case "
   "the player does not mount the vehicle directly, but still wants to be able to control it.\n"

   "@param obj Object to control with this player\n"
   "@return True if the object is valid, false if not\n"

   "@see getControlObject()\n"
   "@see clearControlObject()\n"
   "@see GameConnection::setControlObject()")
{
   if (obj) {
      object->setControlObject(obj);
      return true;
   }
   else
      object->setControlObject(0);
   return false;
}

DefineEngineMethod( Player, getControlObject, S32, (),,
   "@brief Get the current object we are controlling.\n\n"
   "@return ID of the ShapeBase object we control, or 0 if not controlling an "
   "object.\n"
   "@see setControlObject()\n"
   "@see clearControlObject()")
{
   ShapeBase* controlObject = object->getControlObject();
   return controlObject ? controlObject->getId(): 0;
}

DefineEngineMethod( Player, clearControlObject, void, (),,
   "@brief Clears the player's current control object.\n\n"
   "Returns control to the player. This internally calls "
   "Player::setControlObject(0).\n"
   "@tsexample\n"
		"%player.clearControlObject();\n"
      "echo(%player.getControlObject()); //<-- Returns 0, player assumes control\n"
      "%player.setControlObject(%vehicle);\n"
      "echo(%player.getControlObject()); //<-- Returns %vehicle, player controls the vehicle now.\n"
	"@endtsexample\n"
   "@note If the player does not have a control object, the player will receive all moves "
   "from its GameConnection.  If you're looking to remove control from the player itself "
   "(i.e. stop sending moves to the player) use GameConnection::setControlObject() to transfer "
   "control to another object, such as a camera.\n"
   "@see setControlObject()\n"
   "@see getControlObject()\n"
   "@see GameConnection::setControlObject()\n")
{
   object->setControlObject(0);
}

DefineEngineMethod( Player, checkDismountPoint, bool, ( Point3F oldPos, Point3F pos ),,
   "@brief Check if it is safe to dismount at this position.\n\n"

   "Internally this method casts a ray from oldPos to pos to determine if it hits the "
   "terrain, an interior object, a water object, another player, a static shape, "
   "a vehicle (exluding the one currently mounted), or physical zone.  If this ray "
   "is in the clear, then the player's bounding box is also checked for a collision at "
   "the pos position.  If this displaced bounding box is also in the clear, then "
   "checkDismountPoint() returns true.\n"

   "@param oldPos The player's current position\n"
   "@param pos The dismount position to check\n"
   "@return True if the dismount position is clear, false if not\n"
   
   "@note The player must be already mounted for this method to not assert.\n")
{
   MatrixF oldPosMat(true);
   oldPosMat.setColumn(3, oldPos);
   MatrixF posMat(true);
   posMat.setColumn(3, pos);
   return object->checkDismountPosition(oldPosMat, posMat);
}

DefineEngineMethod( Player, getNumDeathAnimations, S32, ( ),,
   "@brief Get the number of death animations available to this player.\n\n"
   "Death animations are assumed to be named death1-N using consecutive indices." )
{
   S32 count = 0;
   const PlayerData * db = dynamic_cast<PlayerData*>( object->getDataBlock() );
   if ( db )
   {

      for ( S32 i = 0; i < db->actionCount; i++ )
         if ( db->actionList[i].death )
            count++;
   }
   return count;
}

DefineEngineMethod( Player, getXJumpCharge, F32, (),,
   "@brief Get the object's current xJump charge.\n\n"
   "@return x-jump charge\n")
{
   return object->getXJumpCharge();
}

//----------------------------------------------------------------------------
void Player::consoleInit()
{
   Con::addVariable("$player::renderMyPlayer",TypeBool, &sRenderMyPlayer,
      "@brief Determines if the player is rendered or not.\n\n"
      "Used on the client side to disable the rendering of all Player objects.  This is "
      "mainly for the tools or debugging.\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::renderMyItems",TypeBool, &sRenderMyItems,
      "@brief Determines if mounted shapes are rendered or not.\n\n"
      "Used on the client side to disable the rendering of all Player mounted objects.  This is "
      "mainly used for the tools or debugging.\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::renderCollision", TypeBool, &sRenderPlayerCollision, 
      "@brief Determines if the player's collision mesh should be rendered.\n\n"
      "This is mainly used for the tools and debugging.\n"
	   "@ingroup GameObjects\n");

   Con::addVariable("$player::minWarpTicks",TypeF32,&sMinWarpTicks, 
      "@brief Fraction of tick at which instant warp occures on the client.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::maxWarpTicks",TypeS32,&sMaxWarpTicks, 
      "@brief When a warp needs to occur due to the client being too far off from the server, this is the "
      "maximum number of ticks we'll allow the client to warp to catch up.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::maxPredictionTicks",TypeS32,&sMaxPredictionTicks, 
      "@brief Maximum number of ticks to predict on the client from the last known move obtained from the server.\n\n"
	   "@ingroup GameObjects\n");

   Con::addVariable("$player::maxImpulseVelocity", TypeF32, &sMaxImpulseVelocity, 
      "@brief The maximum velocity allowed due to a single impulse.\n\n"
	   "@ingroup GameObjects\n");

   // Move triggers
   Con::addVariable("$player::instantXJumpTrigger", TypeS32, &sInstantXJumpTrigger, 
      "@brief The move trigger index used for player instant x-jumping.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::chargedXJumpTrigger", TypeS32, &sChargedXJumpTrigger, 
      "@brief The move trigger index used for player charged x-jumping.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::slideTrigger", TypeS32, &sSlideTrigger, 
      "@brief The move trigger index used for player sliding.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::jumpTrigger", TypeS32, &sJumpTrigger, 
      "@brief The move trigger index used for player jumping.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::crouchTrigger", TypeS32, &sCrouchTrigger, 
      "@brief The move trigger index used for player crouching.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::proneTrigger", TypeS32, &sProneTrigger, 
      "@brief The move trigger index used for player prone pose.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::sprintTrigger", TypeS32, &sSprintTrigger, 
      "@brief The move trigger index used for player sprinting.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::imageTrigger0", TypeS32, &sImageTrigger0, 
      "@brief The move trigger index used to trigger mounted image 0.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::imageTrigger1", TypeS32, &sImageTrigger1, 
      "@brief The move trigger index used to trigger mounted image 1 or alternate fire "
      "on mounted image 0.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::jumpJetTrigger", TypeS32, &sJumpJetTrigger, 
      "@brief The move trigger index used for player jump jetting.\n\n"
	   "@ingroup GameObjects\n");
   Con::addVariable("$player::vehicleDismountTrigger", TypeS32, &sVehicleDismountTrigger, 
      "@brief The move trigger index used to dismount player.\n\n"
	   "@ingroup GameObjects\n");

   // ExtendedMove support
   Con::addVariable("$player::extendedMoveHeadPosRotIndex", TypeS32, &smExtendedMoveHeadPosRotIndex, 
      "@brief The ExtendedMove position/rotation index used for head movements.\n\n"
	   "@ingroup GameObjects\n");
}

//--------------------------------------------------------------------------
void Player::calcClassRenderData()
{
   Parent::calcClassRenderData();

   // If nothing is mounted do not perform the calculations below.  Otherwise,
   // we'll end up with a bad ray cast as both nmat and smat will be the
   // Player's transform.
   MountedImage& image = mMountedImageList[0];
   if (!image.dataBlock)
   {
      mWeaponBackFraction = 0.0f;
      return;
   }

   disableCollision();
   MatrixF nmat;
   MatrixF smat;
   Parent::getRetractionTransform(0,&nmat);
   Parent::getImageTransform(0, &smat);

   // See if we are pushed into a wall...
   Point3F start, end;
   smat.getColumn(3, &start);
   nmat.getColumn(3, &end);

   RayInfo rinfo;
   if (getContainer()->castRay(start, end, sCollisionMoveMask & ~(WaterObjectType|PhysicalZoneObjectType|MarkerObjectType), &rinfo)) {
      if (rinfo.t < 1.0f)
         mWeaponBackFraction = 1.0f - rinfo.t;
      else
         mWeaponBackFraction = 0.0f;
   } else {
      mWeaponBackFraction = 0.0f;
   }
   enableCollision();
}

//-----------------------------------------------------------------------------

void Player::playFootstepSound( bool triggeredLeft, Material* contactMaterial, SceneObject* contactObject )
{
   MatrixF footMat = getTransform();
   if( mWaterCoverage > 0.0 )
   {
      // Treading water.

      if(mWaterCoverage < mDataBlock->footSplashHeight)
		{
			if(triggeredLeft)
				SFX->playOnce( mDataBlock->sound[ PlayerData::FootShallowSplash ], &footMat );
			else
				SFX->playOnce( mDataBlock->sound[ PlayerData::OtherFootShallowSplash ], &footMat );
		}
      else
      {
         if(mWaterCoverage < 1.0)
			{
				if(triggeredLeft)
					SFX->playOnce( mDataBlock->sound[ PlayerData::FootWading ], &footMat );
				else
					SFX->playOnce( mDataBlock->sound[ PlayerData::OtherFootWading ], &footMat );
			}
         else
         {
            if(triggeredLeft)
            {
               SFX->playOnce( mDataBlock->sound[ PlayerData::FootUnderWater ], &footMat );
               SFX->playOnce( mDataBlock->sound[ PlayerData::FootBubbles ], &footMat );
            }
				else
				{
               SFX->playOnce( mDataBlock->sound[ PlayerData::OtherFootUnderWater ], &footMat );
               SFX->playOnce( mDataBlock->sound[ PlayerData::OtherFootBubbles ], &footMat );
				}
         }
      }
   }
   else if( contactMaterial && contactMaterial->mFootstepSoundCustom )
   {
      // Footstep sound defined on material.
      SFX->playOnce( contactMaterial->mFootstepSoundCustom, &footMat );
   }
   else
   {
      // Play default sound.

      S32 sound = -1;
      if( contactMaterial && contactMaterial->mFootstepSoundId != -1 )
         sound = contactMaterial->mFootstepSoundId;
      else if( contactObject && contactObject->getTypeMask() & VehicleObjectType )
         sound = 2;

		if(triggeredLeft)
		{
			switch(sound)
			{
				case 0: // Soft
					SFX->playOnce( mDataBlock->sound[PlayerData::FootSoft], &footMat );
					break;
				case 1: // Hard
					SFX->playOnce( mDataBlock->sound[PlayerData::FootHard], &footMat );
					break;
				case 2: // Metal
					SFX->playOnce( mDataBlock->sound[PlayerData::FootMetal], &footMat );
					break;
				case 3: // Snow
					SFX->playOnce( mDataBlock->sound[PlayerData::FootSnow], &footMat );
					break;
				/*
				default: //Hard
					SFX->playOnce( mDataBlock->sound[PlayerData::FootHard], &footMat );
					break;
				*/
			}
		}
		else
		{
			switch(sound)
			{
				case 0: // Soft
					SFX->playOnce( mDataBlock->sound[PlayerData::OtherFootSoft], &footMat );
					break;
				case 1: // Hard
					SFX->playOnce( mDataBlock->sound[PlayerData::OtherFootHard], &footMat );
					break;
				case 2: // Metal
					SFX->playOnce( mDataBlock->sound[PlayerData::OtherFootMetal], &footMat );
					break;
				case 3: // Snow
					SFX->playOnce( mDataBlock->sound[PlayerData::OtherFootSnow], &footMat );
					break;
				/*
				default: //Hard
					SFX->playOnce( mDataBlock->sound[PlayerData::FootHard], &footMat );
					break;
				*/
			}
		}
   }
}

void Player:: playImpactSound()
{
   if( mWaterCoverage == 0.0f )
   {
      Point3F pos;
      RayInfo rInfo;
      MatrixF mat = getTransform();
      mat.mulP(Point3F(mDataBlock->decalOffset,0.0f,0.0f), &pos);

      if( gClientContainer.castRay( Point3F( pos.x, pos.y, pos.z + 0.01f ),
                                    Point3F( pos.x, pos.y, pos.z - 2.0f ),
                                    STATIC_COLLISION_TYPEMASK | VehicleObjectType,
                                    &rInfo ) )
      {
         Material* material = ( rInfo.material ? dynamic_cast< Material* >( rInfo.material->getMaterial() ) : 0 );

         if( material && material->mImpactSoundCustom )
            SFX->playOnce( material->mImpactSoundCustom, &getTransform() );
         else
         {
            S32 sound = -1;
            if( material && material->mImpactSoundId )
               sound = material->mImpactSoundId;
            else if( rInfo.object->getTypeMask() & VehicleObjectType )
               sound = 2; // Play metal;

            switch( sound )
            {
            case 0:
               //Soft
               SFX->playOnce( mDataBlock->sound[ PlayerData::ImpactSoft ], &getTransform() );
               break;
            case 1:
               //Hard
               SFX->playOnce( mDataBlock->sound[ PlayerData::ImpactHard ], &getTransform() );
               break;
            case 2:
               //Metal
               SFX->playOnce( mDataBlock->sound[ PlayerData::ImpactMetal ], &getTransform() );
               break;
            case 3:
               //Snow
               SFX->playOnce( mDataBlock->sound[ PlayerData::ImpactSnow ], &getTransform() );
               break;
               /*
            default:
               //Hard
               alxPlay(mDataBlock->sound[PlayerData::ImpactHard], &getTransform());
               break;
               */
            }
         }
      }
   }

   mImpactSound = 0;
}

//--------------------------------------------------------------------------
// Update splash
//--------------------------------------------------------------------------

void Player::updateSplash()
{
   F32 speed = getVelocity().len();
   if( speed < mDataBlock->splashVelocity || isMounted() ) return;

   Point3F curPos = getPosition();

   if ( curPos.equal( mLastPos ) )
      return;

   if (pointInWater( curPos )) {
      if (!pointInWater( mLastPos )) {
         Point3F norm = getVelocity();
         norm.normalize();

         // make sure player is moving vertically at good pace before playing splash
         F32 splashAng = mDataBlock->splashAngle / 360.0;
         if( mDot( norm, Point3F(0.0, 0.0, -1.0) ) < splashAng )
            return;


         RayInfo rInfo;
         if (gClientContainer.castRay(mLastPos, curPos,
               WaterObjectType, &rInfo)) {
            createSplash( rInfo.point, speed );
            mBubbleEmitterTime = 0.0;
         }

      }
   }
}


//--------------------------------------------------------------------------

void Player::updateFroth( F32 dt )
{
   // update bubbles
   Point3F moveDir = getVelocity();
   mBubbleEmitterTime += dt;

   if (mBubbleEmitterTime < mDataBlock->bubbleEmitTime) {
      if (mSplashEmitter[PlayerData::BUBBLE_EMITTER]) {
         Point3F emissionPoint = getRenderPosition();
         U32 emitNum = PlayerData::BUBBLE_EMITTER;
         mSplashEmitter[emitNum]->emitParticles(mLastPos, emissionPoint,
            Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(dt * 1000.0));
      }
   }

   Point3F contactPoint;
   if (!collidingWithWater(contactPoint)) {
      mLastWaterPos = mLastPos;
      return;
   }

   F32 speed = moveDir.len();
   if ( speed < mDataBlock->splashVelEpsilon ) 
      speed = 0.0;

   U32 emitRate = (U32) (speed * mDataBlock->splashFreqMod * dt);

   // If we're in the water, swimming, but not
   // moving, then lets emit some particles because
   // we're treading water.  
   if ( mSwimming && speed == 0.0 )
   {
      emitRate = (U32) (2.0f * mDataBlock->splashFreqMod * dt);
   }   

   U32 i;
   for ( i=0; i<PlayerData::BUBBLE_EMITTER; i++ ) {
      if (mSplashEmitter[i] )
         mSplashEmitter[i]->emitParticles( mLastWaterPos,
            contactPoint, Point3F( 0.0, 0.0, 1.0 ),
            moveDir, emitRate );
   }
   mLastWaterPos = contactPoint;
}

void Player::updateWaterSounds(F32 dt)
{
   if ( mWaterCoverage < 1.0f || mDamageState != Enabled )
   {
      // Stop everything
      if ( mMoveBubbleSound )
         mMoveBubbleSound->stop();
      if ( mWaterBreathSound )
         mWaterBreathSound->stop();
      return;
   }

   if ( mMoveBubbleSound )
   {
      // We're under water and still alive, so let's play something
      if ( mVelocity.len() > 1.0f )
      {
         if ( !mMoveBubbleSound->isPlaying() )
            mMoveBubbleSound->play();

         mMoveBubbleSound->setTransform( getTransform() );
      }
      else
         mMoveBubbleSound->stop();
   }

   if ( mWaterBreathSound )
   {
      if ( !mWaterBreathSound->isPlaying() )
         mWaterBreathSound->play();

      mWaterBreathSound->setTransform( getTransform() );
   }
}

void Player::updateSlideParticles(F32 dt)
{
	Point3F emissionPoint = getRenderPosition();
	Point3F moveDir = getVelocity();

	if(!this->isSliding() || mDamageState != Enabled)
		return;

	if(mSlideContact < 2)
	{
		F32 amount = 1 - mSlideContact / 2;
		for(int i = PlayerData::ContactTrail1; i <= PlayerData::ContactTrail3; i++)
		{
			if(mSlideEmitter[i])
			{
				mSlideEmitter[i]->setPalette(this->getPalette());
				mSlideEmitter[i]->emitParticles(mLastPos, emissionPoint,
					Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(amount * dt * 1000.0));	
			}
		}
		for(int i = PlayerData::ContactRelative1; i <= PlayerData::ContactRelative3; i++)
		{
			if(mSlideEmitter[i])
			{
				mSlideEmitter[i]->setPalette(this->getPalette());
				mSlideEmitter[i]->emitParticles(mLastPos, emissionPoint,
					Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(amount * dt * 1000.0));	
			}
		}
	}

	for(int i = PlayerData::Trail1; i <= PlayerData::Trail3; i++)
	{
		if(mSlideEmitter[i])
		{
			mSlideEmitter[i]->setPalette(this->getPalette());
			mSlideEmitter[i]->emitParticles(mLastPos, emissionPoint,
				Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(dt * 1000.0));
		}
	}
	for(int i = PlayerData::Relative1; i <= PlayerData::Relative3; i++)
	{
		if(mSlideEmitter[i])
		{
			mSlideEmitter[i]->setPalette(this->getPalette());
			mSlideEmitter[i]->emitParticles(mLastPos, emissionPoint,
				Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(dt * 1000.0));
		}
	}
}

void Player::updateSlideSound(F32 dt)
{
   if(this->isSliding() && mDamageState == Enabled)
   {
      if(mSlideSound)
      {
         if(!mSlideSound->isPlaying())
         {
            //Con::printf("%s: Starting slide sound!", isGhost() ? "CLNT" : "SRVR");
            mSlideSound->play();
         }
         mSlideSound->setTransform(this->getTransform());
      }

      if(mSlideContactSound)
      {
         if(mSlideContact < 2)
         {
            if(!mSlideContactSound->isPlaying())
            {
               //Con::printf("%s: Starting slide contact sound!", isGhost() ? "CLNT" : "SRVR");
               mSlideContactSound->play();
            }

            F32 gain = 1 - mSlideContact/2;
            mSlideContactSound->setTransform(this->getRenderTransform());
            mSlideContactSound->setVolume(gain);
         }
         else
         {
            if(mSlideContactSound->isPlaying())
            {
               //Con::printf("%s: Stopping slide contact sound!", isGhost() ? "CLNT" : "SRVR");
               mSlideContactSound->stop();
            }
         }
      }
   }
   else
   {
      if(mSlideSound)
      {
         //Con::printf("%s: Stopping slide sound!", isGhost() ? "CLNT" : "SRVR");
         mSlideSound->stop();
      }

      if(mSlideContactSound)
      {
         //Con::printf("%s: Stopping slide contact sound!", isGhost() ? "CLNT" : "SRVR");
         mSlideContactSound->stop();
      }
   }
}

void Player::updateSkidParticles(F32 dt)
{
   if(!this->isSkidding())
      return;

   Point3F emissionPoint = getRenderPosition();
   Point3F moveDir = getVelocity();
  
   F32 amount = 1 - mDataBlock->skidSpeed / moveDir.len();
   for(int i = 0; i <= PlayerData::NumSkidEmitters; i++)
   {
      if(mSkidEmitter[i])
      {
         mSkidEmitter[i]->setPalette(this->getPalette());
         mSkidEmitter[i]->emitParticles(mLastPos, emissionPoint,
            Point3F( 0.0, 0.0, 1.0 ), moveDir, (U32)(amount * dt * 1000.0));	
      }
   }
}

void Player::updateSkidSound(F32 dt)
{
	if(!mSkidSound)
		return;

   if(this->isSkidding())
	{
      if(!mSkidSound->isPlaying())
      {
         //Con::printf("%s: Starting skid sound!", isGhost() ? "CLNT" : "SRVR");
         mSkidSound->play();
      }
      mSkidSound->setTransform(this->getTransform());
 	}
	else
	{
      if(mSkidSound->isPlaying())
      {
         //Con::printf("%s: Stopping skid sound!", isGhost() ? "CLNT" : "SRVR");
         mSkidSound->stop();
      }
 	}
}

void Player::updateXJumpChargeSound(F32 dt)
{
	if(!mXJumpChargeSound)
		return;

   if(mXJumpChargeInProgress && mDamageState == Enabled)
   {
      if(!mXJumpChargeSound->isPlaying())
         mXJumpChargeSound->play();
      mXJumpChargeSound->setTransform(this->getTransform());
      mXJumpChargeSound->setPitch(1+mXJumpCharge/mDataBlock->maxEnergy[mDataBlock->xJumpEnergySlot]);
   }
   else
   {
      if(mXJumpChargeSound->isPlaying())
         mXJumpChargeSound->stop();
   }
}

void Player::updateFirstPersonWeaponBob(F32 dt)
{
   if(mFirstPersonWeaponBob.state == FirstPersonWeaponBob::NoBob)
   {
      mFirstPersonWeaponBob.pos *= 0.95 * (1-dt);
      if(mFirstPersonWeaponBob.pos.len() < 0.01)
         mFirstPersonWeaponBob.pos.set(0,0,0);
   }
   else
   {
      F32 vel = this->getVelocity().len();
      mFirstPersonWeaponBob.counter += vel * 0.5 * dt;
      mFirstPersonWeaponBob.pos.x = 0.05 * mSin(mFirstPersonWeaponBob.counter);
   }
}


//--------------------------------------------------------------------------
// Returns true if player is intersecting a water surface
//--------------------------------------------------------------------------
bool Player::collidingWithWater( Point3F &waterHeight )
{
   if ( !mCurrentWaterObject )
      return false;
   
   Point3F curPos = getPosition();

   if ( mWorldBox.maxExtents.z < mLiquidHeight )
      return false;

   curPos.z = mLiquidHeight;

   waterHeight = getPosition();
   waterHeight.z = mLiquidHeight;

   return true;
}

//--------------------------------------------------------------------------

void Player::createSplash( Point3F &pos, F32 speed )
{
   if ( speed >= mDataBlock->hardSplashSoundVel )
      SFX->playOnce( mDataBlock->sound[PlayerData::ImpactWaterHard], &getTransform() );
   else if ( speed >= mDataBlock->medSplashSoundVel )
      SFX->playOnce( mDataBlock->sound[PlayerData::ImpactWaterMedium], &getTransform() );
   else
      SFX->playOnce( mDataBlock->sound[PlayerData::ImpactWaterEasy], &getTransform() );

   if( mDataBlock->splash )
   {
      MatrixF trans = getTransform();
      trans.setPosition( pos );
      Splash *splash = new Splash;
      splash->setPalette(this->getPalette());
      splash->onNewDataBlock( mDataBlock->splash, false );
      splash->setTransform( trans );
      splash->setInitialState( trans.getPosition(), Point3F( 0.0, 0.0, 1.0 ) );
      if (!splash->registerObject())
         delete splash;
   }
}


bool Player::isControlObject()
{
   GameConnection* connection = GameConnection::getConnectionToServer();
   if( !connection ) return false;
   ShapeBase *obj = dynamic_cast<ShapeBase*>(connection->getControlObject());
   return ( obj == this );
}


void Player::prepRenderImage( SceneRenderState* state )
{
   bool renderPlayer = true;
   bool renderItems = true;

   /*
   if ( mPhysicsRep && Con::getBoolVariable("$PhysicsPlayer::DebugRender",false) )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( mPhysicsRep, &PhysicsPlayer::renderDebug );
      ri->objectIndex = -1;
      ri->type = RenderPassManager::RIT_Editor;
      state->getRenderPass()->addInst( ri );
   }
   */

   // Debug rendering for all convexes in the Players working list.
   if ( sRenderPlayerCollision )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &Player::renderConvex );
      ri->objectIndex = -1;
      ri->type = RenderPassManager::RIT_Editor;
      state->getRenderPass()->addInst( ri );
   }

   GameConnection* connection = GameConnection::getConnectionToServer();
   if( connection && connection->getControlObject() == this && connection->isFirstPerson() )
   {
      // If we're first person and we are not rendering the player
      // then disable all shadow rendering... a floating gun shadow sucks.
      if ( state->isShadowPass() && !mDataBlock->renderFirstPerson && !mDataBlock->firstPersonShadows )
         return;

      renderPlayer = mDataBlock->renderFirstPerson || !state->isDiffusePass();

      if( !sRenderMyPlayer )
         renderPlayer = false;
      if( !sRenderMyItems )
         renderItems = false;
   }

   // Call the protected base class to do the work 
   // now that we know if we're rendering the player
   // and mounted shapes.
   return ShapeBase::_prepRenderImage( state, 
                                       renderPlayer, 
                                       renderItems );
}

void Player::renderConvex( ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat )
{
   GFX->enterDebugEvent( ColorI(255,0,255), "Player_renderConvex" );
   mConvex.renderWorkingList();
   GFX->leaveDebugEvent();
}
