// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "renderInstance/renderGlowMgr.h"
#include "renderInstance/renderParticleMgr.h"

#include "scene/sceneManager.h"
#include "scene/sceneRenderState.h"
#include "materials/sceneData.h"
#include "materials/matInstance.h"
#include "materials/materialFeatureTypes.h"
#include "materials/processedMaterial.h"
#include "postFx/postEffect.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDebugEvent.h"
#include "math/util/matrixSet.h"

IMPLEMENT_CONOBJECT( RenderGlowMgr );


ConsoleDocClass( RenderGlowMgr, 
   "@brief A render bin for the glow pass.\n\n"
   "When the glow buffer PostEffect is enabled this bin gathers mesh render "
   "instances with glow materials and renders them to the #glowbuffer offscreen "
   "render target.\n\n"
   "This render target is then used by the 'GlowPostFx' PostEffect to blur and "
   "render the glowing portions of the screen.\n\n"
   "@ingroup RenderBin\n" );

const MatInstanceHookType RenderGlowMgr::GlowMaterialHook::Type( "Glow" );


RenderGlowMgr::GlowMaterialHook::GlowMaterialHook( BaseMatInstance *matInst )
   : mGlowMatInst( NULL )
{
   mGlowMatInst = (MatInstance*)matInst->getMaterial()->createMatInstance();
   mGlowMatInst->getFeaturesDelegate().bind( &GlowMaterialHook::_overrideFeatures );
   mGlowMatInst->setUserObject(matInst->getUserObject());
   mGlowMatInst->init(  matInst->getRequestedFeatures(), 
                        matInst->getVertexFormat() );
}

RenderGlowMgr::GlowMaterialHook::~GlowMaterialHook()
{
   SAFE_DELETE( mGlowMatInst );
}

void RenderGlowMgr::GlowMaterialHook::_overrideFeatures( ProcessedMaterial *mat,
                                                         U32 stageNum,
                                                         MaterialFeatureData &fd, 
                                                         const FeatureSet &features )
{
   // If this isn't a glow pass... then add the glow mask feature.
   if (  mat->getMaterial() && 
         !mat->getMaterial()->mGlow[stageNum] )
      fd.features.addFeature( MFT_GlowMask );

   // Don't allow fog or HDR encoding on 
   // the glow materials.
   fd.features.removeFeature( MFT_Fog );
   fd.features.removeFeature( MFT_HDROut );
}

RenderGlowMgr::RenderGlowMgr()
   : RenderTexTargetBinManager(  RenderPassManager::RIT_Mesh, 
                                 1.0f, 
                                 1.0f,
                                 GFXFormatR8G8B8A8,
                                 Point2I( 512, 512 ) )
{
   notifyType( RenderPassManager::RIT_Decal );
   notifyType( RenderPassManager::RIT_Translucent );
   notifyType( RenderPassManager::RIT_Particle );

   mParticleRenderMgr = NULL;

   mNamedTarget.registerWithName( "glowbuffer" );
   mTargetSizeType = WindowSize;
}

RenderGlowMgr::~RenderGlowMgr()
{
}

PostEffect* RenderGlowMgr::getGlowEffect()
{
   if ( !mGlowEffect )
      mGlowEffect = dynamic_cast<PostEffect*>( Sim::findObject( "GlowPostFx" ) );
   
   return mGlowEffect;
}

bool RenderGlowMgr::isGlowEnabled()
{
   return getGlowEffect() && getGlowEffect()->isEnabled();
}

void RenderGlowMgr::addElement( RenderInst *inst )
{
   // Skip out if we don't have the glow post 
   // effect enabled at this time.
   if ( !isGlowEnabled() )
      return;

   // TODO: We need to get the scene state here in a more reliable
   // manner so we can skip glow in a non-diffuse render pass.
   //if ( !mParentManager->getSceneManager()->getSceneState()->isDiffusePass() )
      //return RenderBinManager::arSkipped;
   ParticleRenderInst *particleInst = NULL;
   if(inst->type == RenderPassManager::RIT_Particle)
      particleInst = static_cast<ParticleRenderInst*>(inst);
   if(particleInst && particleInst->glow)
   {
      internalAddElement(inst);
      return;
   }

   // Skip it if we don't have a glowing material.
   BaseMatInstance *matInst = getMaterial( inst );
   if ( !matInst || !matInst->hasGlow() )   
      return;

   internalAddElement(inst);
}

void RenderGlowMgr::render( SceneRenderState *state )
{
   PROFILE_SCOPE( RenderGlowMgr_Render );
   
   if ( !isGlowEnabled() )
      return;

   const U32 binSize = mElementList.size();

   // If this is a non-diffuse pass or we have no objects to
   // render then tell the effect to skip rendering.
   if ( !state->isDiffusePass() || binSize == 0 )
   {
      getGlowEffect()->setSkip( true );
      return;
   }

   GFXDEBUGEVENT_SCOPE( RenderGlowMgr_Render, ColorI::GREEN );

   GFXTransformSaver saver;

   // Respect the current viewport
   mNamedTarget.setViewport(GFX->getViewport());

   // Tell the superclass we're about to render, preserve contents
   const bool isRenderingToTarget = _onPreRender( state, true );

   // Clear all the buffers to black.
   GFX->clear( GFXClearTarget, ColorI::BLACK, 1.0f, 0);

   // Restore transforms
   MatrixSet &matrixSet = getRenderPass()->getMatrixSet();
   matrixSet.restoreSceneViewProjection();

   // init loop data
   SceneData sgData;
   sgData.init( state, SceneData::GlowBin );

   for( U32 j=0; j<binSize; )
   {
      RenderInst *_ri = mElementList[j].inst;
      if(_ri->type == RenderPassManager::RIT_Particle)
      {
         // Find the particle render manager (if we don't have it)
         if(mParticleRenderMgr == NULL)
         {
            RenderPassManager *rpm = state->getRenderPass();
            for( U32 i = 0; i < rpm->getManagerCount(); i++ )
            {
               RenderBinManager *bin = rpm->getManager(i);
               if( bin->getRenderInstType() == RenderParticleMgr::RIT_Particles )
               {
                  mParticleRenderMgr = reinterpret_cast<RenderParticleMgr *>(bin);
                  break;
               }
            }
         }

         ParticleRenderInst *ri = static_cast<ParticleRenderInst*>(_ri);
         mParticleRenderMgr->renderParticle(ri, state);
         j++;
         continue;
      }

      MeshRenderInst *ri = static_cast<MeshRenderInst*>(_ri);

      setupSGData( ri, sgData );

      BaseMatInstance *mat = ri->matInst;
      GlowMaterialHook *hook = mat->getHook<GlowMaterialHook>();
      if ( !hook )
      {
         hook = new GlowMaterialHook( ri->matInst );
         ri->matInst->addHook( hook );
      }
      BaseMatInstance *glowMat = hook->getMatInstance();

      U32 matListEnd = j;

      while( glowMat && glowMat->setupPass( state, sgData ) )
      {
         U32 a;
         for( a=j; a<binSize; a++ )
         {
            if (mElementList[a].inst->type == RenderPassManager::RIT_Particle)
               break;

            MeshRenderInst *passRI = static_cast<MeshRenderInst*>(mElementList[a].inst);

            if ( newPassNeeded( ri, passRI ) )
               break;

            //GFXDEBUGEVENT_SCOPE_EX( RenderGlowMgr_RenderLoop, ColorI::GREEN, avar("%s", passRI->meshName) );

            sgData.palette = passRI->palette;

            matrixSet.setWorld(*passRI->objectToWorld);
            matrixSet.setView(*passRI->worldToCamera);
            matrixSet.setProjection(*passRI->projection);
            glowMat->setTransforms(matrixSet, state);
            glowMat->setSceneInfo(state, sgData);
            glowMat->setBuffers(passRI->vertBuff, passRI->primBuff);

            if ( passRI->prim )
               GFX->drawPrimitive( *passRI->prim );
            else
               GFX->drawPrimitive( passRI->primBuffIndex );
         }
         matListEnd = a;
         setupSGData( ri, sgData );
      }

      // force increment if none happened, otherwise go to end of batch
      j = ( j == matListEnd ) ? j+1 : matListEnd;
   }

   // Finish up.
   if ( isRenderingToTarget )
      _onPostRender();

   // Make sure the effect is gonna render.
   getGlowEffect()->setSkip( false );
}
