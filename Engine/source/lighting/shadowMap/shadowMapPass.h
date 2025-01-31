// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _SHADOWMAPPASS_H_
#define _SHADOWMAPPASS_H_

#ifndef _RENDERPASSMANAGER_H_
#include "renderInstance/renderPassManager.h"
#endif
#ifndef _RENDERMESHMGR_H_
#include "renderInstance/renderMeshMgr.h"
#endif
#ifndef _LIGHTINFO_H_
#include "lighting/lightInfo.h"
#endif
#ifndef _SHADOW_COMMON_H_
#include "lighting/shadowMap/shadowCommon.h"
#endif

class RenderMeshMgr;
class LightShadowMap;
class LightManager;
class ShadowMapManager;
class BaseMatInstance;
class RenderObjectMgr;
class RenderTerrainMgr;
class PlatformTimer;
class ShadowRenderPassManager;

/// ShadowMapPass, this is plugged into the SceneManager to generate 
/// ShadowMaps for the scene.
class ShadowMapPass
{
public:

   ShadowMapPass() {}   // Only called by ConsoleSystem
   ShadowMapPass(LightManager* LightManager, ShadowMapManager* ShadowManager);
   virtual ~ShadowMapPass();

   //
   // SceneRenderPass interface
   //

   /// Called to render a scene.
   void render(   SceneManager *sceneGraph, 
                  const SceneRenderState *diffuseState, 
                  U32 objectMask );

   /// Return the type of pass this is
   virtual const String& getPassType() const { return PassTypeName; };

   /// Return our sort value. (Go first in order to have shadow maps available for RIT_Objects)
   virtual F32 getSortValue() const { return 0.0f; }

   virtual bool geometryOnly() const { return true; }

   static const String PassTypeName;


   /// Used to for debugging performance by disabling
   /// shadow updates and rendering.
   static bool smDisableShadows;

   static bool smDisableShadowsEditor;
   static bool smDisableShadowsPref;

private:

   static U32 smActiveShadowMaps;
   static U32 smUpdatedShadowMaps;
   static U32 smNearShadowMaps;
   static U32 smShadowMapsDrawCalls;
   static U32 smShadowMapPolyCount;
   static U32 smRenderTargetChanges;
   static U32 smShadowPoolTexturesCount;
   static F32 smShadowPoolMemory;

   /// The milliseconds alotted for shadow map updates
   /// on a per frame basis.
   static U32 smRenderBudgetMs;

   PlatformTimer *mTimer;

   LightInfoList mLights;
   U32 mActiveLights;
   SimObjectPtr<ShadowRenderPassManager> mShadowRPM;
   LightManager* mLightManager;
   ShadowMapManager* mShadowManager;
};

class ShadowRenderPassManager : public RenderPassManager
{
   typedef RenderPassManager Parent;
public:
   ShadowRenderPassManager() : Parent() {}

   /// Add a RenderInstance to the list
   virtual void addInst( RenderInst *inst );
};

#endif // _SHADOWMAPPASS_H_
