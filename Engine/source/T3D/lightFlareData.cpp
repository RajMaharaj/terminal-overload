// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "T3D/lightFlareData.h"

#include "core/stream/bitStream.h"
#include "console/engineAPI.h"
#include "lighting/lightInfo.h"
#include "math/mathUtils.h"
#include "math/mathIO.h"
#include "scene/sceneRenderState.h"
#include "gfx/gfxOcclusionQuery.h"
#include "gfx/gfxDrawUtil.h"
#include "renderInstance/renderPassManager.h"
#include "T3D/gameBase/gameConnection.h"
#include "T3D/gameBase/processList.h"
#include "collision/collision.h"


const U32 LightFlareData::LosMask = STATIC_COLLISION_TYPEMASK |
                                    ShapeBaseObjectType |
                                    StaticShapeObjectType |
                                    ItemObjectType;


LightFlareState::~LightFlareState()
{
   delete occlusionQuery;
   delete fullPixelQuery;
}

void LightFlareState::clear()
{
   visChangedTime = 0;
   visible = false;
   scale = 1.0f;
   fullBrightness = 1.0f;
   lightMat = MatrixF::Identity;
   lightInfo = NULL;
   worldRadius = -1.0f;
   occlusion = -1.0f;
   occlusionQuery = NULL;
   fullPixelQuery = NULL;
}

Point3F LightFlareData::sBasePoints[] =
{ 
   Point3F( -0.5, 0.5, 0.0 ), 
   Point3F( -0.5, -0.5, 0.0 ),   
   Point3F( 0.5, -0.5, 0.0 ),  
   Point3F( 0.5, 0.5, 0.0 ) 
};


IMPLEMENT_CO_DATABLOCK_V1( LightFlareData );

ConsoleDocClass( LightFlareData,
   "@brief Defines a light flare effect usable by scene lights.\n\n"      
   
   "%LightFlareData is a datablock which defines a type of flare effect. "
   "This may then be referenced by other classes which support the rendering "
   "of a flare: Sun, ScatterSky, LightBase.\n\n"
   
   "A flare contains one or more elements defined in the element* named fields "
   "of %LightFlareData, with a maximum of ten elements. Each element is rendered "
   "as a 2D sprite in screenspace.\n\n"

   "@tsexample\n"
   "// example from Full Template, core/art/datablocks/lights.cs\n"
   "datablock LightFlareData( LightFlareExample0 )\n"
   "{\n"
   "   overallScale = 2.0;\n"
   "   flareEnabled = true;\n"
   "   renderReflectPass = true;\n"
   "   flareTexture = \"./../special/lensFlareSheet1\";\n"
   "   occlusionRadius = 0.25;\n"
   "   \n"
   "   elementRect[0] = \"0 512 512 512\";\n"
   "   elementDist[0] = 0.0;\n"
   "   elementScale[0] = 0.5;\n"
   "   elementTint[0] = \"1.0 1.0 1.0\";\n"
   "   elementRotate[0] = false;\n"
   "   elementUseLightColor[0] = false;\n"
   "   \n"
   "   elementRect[1] = \"512 0 512 512\";\n"
   "   elementDist[1] = 0.0;\n"
   "   elementScale[1] = 2.0;\n"
   "   elementTint[1] = \"0.5 0.5 0.5\";\n"
   "   elementRotate[1] = false;\n"
   "   elementUseLightColor[1] = false;\n"
   "};\n"
   "@endtsexample\n"
   "The elementDist field defines where along the flare's beam the element appears. "   
   "A distance of 0.0 is directly over the light source, a distance of 1.0 "
   "is at the screen center, and a distance of 2.0 is at the position of the "
   "light source mirrored across the screen center.\n"
   "@image html images/lightFlareData_diagram.png\n"
   "@ingroup Lighting"   
);

LightFlareData::LightFlareData()
 : mFlareEnabled( true ),
   mElementCount( 0 ),
   mScale( 1.0f ),
   mLOSMask( LosMask ),
   mOcclusionRadius( 0.0f ),
   mRenderReflectPass( true )
{
   dMemset( mElementRect, 0, sizeof( RectF ) * MAX_ELEMENTS );   
   dMemset( mElementScale, 0, sizeof( F32 ) * MAX_ELEMENTS );
   dMemset( mElementTint, 0, sizeof( ColorF ) * MAX_ELEMENTS );
   dMemset( mElementRotate, 0, sizeof( bool ) * MAX_ELEMENTS );
   dMemset( mElementUseLightColor, 0, sizeof( bool ) * MAX_ELEMENTS );   

   for ( U32 i = 0; i < MAX_ELEMENTS; i++ )   
      mElementDist[i] = -1.0f;   
}

LightFlareData::~LightFlareData()
{
}

void LightFlareData::initPersistFields()
{
   addGroup( "LightFlareData" );

      addField( "overallScale", TypeF32, Offset( mScale, LightFlareData ),
         "Size scale applied to all elements of the flare." );

      addField( "losMask", TypeS32, Offset( mLOSMask, LightFlareData ), 
         "Object type mask for LOS test." );

      addField( "occlusionRadius", TypeF32, Offset( mOcclusionRadius, LightFlareData ), 
         "If positive an occlusion query is used to test flare visibility, else it uses simple raycasts." );

      addField( "renderReflectPass", TypeBool, Offset( mRenderReflectPass, LightFlareData ), 
         "If false the flare does not render in reflections, else only non-zero distance elements are rendered." );

   endGroup( "LightFlareData" );

   addGroup( "FlareElements" );

      addField( "flareEnabled", TypeBool, Offset( mFlareEnabled, LightFlareData ),
         "Allows the user to disable this flare globally for any lights referencing it." );

      addField( "flareTexture", TypeImageFilename, Offset( mFlareTextureName, LightFlareData ),
         "The texture / sprite sheet for this flare." );

      addArray( "Elements", MAX_ELEMENTS );

         addField( "elementRect", TypeRectF, Offset( mElementRect, LightFlareData ), MAX_ELEMENTS,
            "A rectangle specified in pixels of the flareTexture image." );

         addField( "elementDist", TypeF32, Offset( mElementDist, LightFlareData ), MAX_ELEMENTS,
            "Where this element appears along the flare beam." );

         addField( "elementScale", TypeF32, Offset( mElementScale, LightFlareData ), MAX_ELEMENTS,
            "Size scale applied to this element." );

         addField( "elementTint", TypeColorF, Offset( mElementTint, LightFlareData ), MAX_ELEMENTS,
            "Used to modulate this element's color if elementUseLightColor "
            "is false.\n"
            "@see elementUseLightColor" );

         addField( "elementRotate", TypeBool, Offset( mElementRotate, LightFlareData ), MAX_ELEMENTS,
            "Defines if this element orients to point along the flare beam "
            "or if it is always upright." );

         addField( "elementUseLightColor", TypeBool, Offset( mElementUseLightColor, LightFlareData ), MAX_ELEMENTS,
            "If true this element's color is modulated by the light color. "
            "If false, elementTint will be used.\n"
            "@see elementTint" );

      endArray( "FlareElements" );

   endGroup( "Flares" );

   Parent::initPersistFields();
}

void LightFlareData::inspectPostApply()
{
   Parent::inspectPostApply();

   // Hack to allow changing properties in game.
   // Do the same work as preload.
   
   String str;
   _preload( false, str );
}

bool LightFlareData::preload( bool server, String &errorStr )
{
   if ( !Parent::preload( server, errorStr ) )
      return false;

   return _preload( server, errorStr );
}

void LightFlareData::packData( BitStream *stream )
{
   Parent::packData( stream );

   stream->writeFlag( mFlareEnabled );
   stream->write( mFlareTextureName );   
   stream->write( mScale );
   stream->write( mLOSMask );
   stream->write( mOcclusionRadius );
   stream->writeFlag( mRenderReflectPass );

   stream->write( mElementCount );

   for ( U32 i = 0; i < mElementCount; i++ )
   {
      mathWrite( *stream, mElementRect[i] );
      stream->write( mElementDist[i] );
      stream->write( mElementScale[i] );
      stream->write( mElementTint[i] );
      stream->writeFlag( mElementRotate[i] );
      stream->writeFlag( mElementUseLightColor[i] );
   }
}

void LightFlareData::unpackData( BitStream *stream )
{
   Parent::unpackData( stream );

   mFlareEnabled = stream->readFlag();
   stream->read( &mFlareTextureName );   
   stream->read( &mScale );
   stream->read( &mLOSMask );
   stream->read( &mOcclusionRadius );
   mRenderReflectPass = stream->readFlag();

   stream->read( &mElementCount );

   for ( U32 i = 0; i < mElementCount; i++ )
   {
      mathRead( *stream, &mElementRect[i] );
      stream->read( &mElementDist[i] );
      stream->read( &mElementScale[i] );
      stream->read( &mElementTint[i] );
      mElementRotate[i] = stream->readFlag();
      mElementUseLightColor[i] = stream->readFlag();
   }
}

bool LightFlareData::_testVisibility(const SceneRenderState *state, LightFlareState *flareState, U32 *outVisDelta, F32 *outOcclusionFade, Point3F *outLightPosSS)
{
   // Reflections use the results from the last forward
   // render so we don't need multiple queries.
   if ( state->isReflectPass() )
   {
      *outOcclusionFade = flareState->occlusion;
      *outVisDelta = Sim::getCurrentTime() - flareState->visChangedTime;
      return flareState->visible;
   }

   // Initialize it to something first.
   *outOcclusionFade = 0;

   // First check to see if the flare point 
   // is on scren at all... if not then return
   // the last result.
   const Point3F &lightPos = flareState->lightMat.getPosition();  
   const RectI &viewport = GFX->getViewport();
   MatrixF projMatrix;
   state->getCameraFrustum().getProjectionMatrix(&projMatrix);
   if( state->isReflectPass() )
      projMatrix = state->getSceneManager()->getNonClipProjection();
   bool onScreen = MathUtils::mProjectWorldToScreen( lightPos, outLightPosSS, viewport, GFX->getWorldMatrix(), projMatrix );

   // It is onscreen, so raycast as a simple occlusion test.
   const LightInfo *lightInfo = flareState->lightInfo;
   const bool isVectorLight = lightInfo->getType() == LightInfo::Vector;

   const bool useOcclusionQuery = isVectorLight ? flareState->worldRadius > 0.0f : mOcclusionRadius > 0.0f;
   bool needsRaycast = true;

   // NOTE: if hardware does not support HOQ it will return NULL
   // and we will retry every time but there is not currently a good place
   // for one-shot initialization of LightFlareState
   if ( useOcclusionQuery )
   {
      if ( flareState->occlusionQuery == NULL )
         flareState->occlusionQuery = GFX->createOcclusionQuery();
      if ( flareState->fullPixelQuery == NULL )
         flareState->fullPixelQuery = GFX->createOcclusionQuery();

      // Always treat light as onscreen if using HOQ
      // it will be faded out if offscreen anyway.
      onScreen = true;

      // NOTE: These queries frame lock us as we block to get the
      // results.  This is ok as long as long as we're not too GPU
      // bound... else we waste CPU time here waiting for it when
      // we could have been doing other CPU work instead.

      // Test the hardware queries for rendered pixels.
      U32 pixels = 0, fullPixels = 0;
      GFXOcclusionQuery::OcclusionQueryStatus status = flareState->occlusionQuery->getStatus( true, &pixels );
      flareState->fullPixelQuery->getStatus( true, &fullPixels );
      if ( status != GFXOcclusionQuery::Occluded && fullPixels != 0 )
         *outOcclusionFade = mClampF( (F32)pixels / (F32)fullPixels, 0.0f, 1.0f );

      // If we got a result then we don't need to fallback to the raycast.
      if ( status != GFXOcclusionQuery::Unset )
         needsRaycast = false;

      // Setup the new queries.
      RenderPassManager *rpm = state->getRenderPass();
      OccluderRenderInst *ri = rpm->allocInst<OccluderRenderInst>();   
      ri->type = RenderPassManager::RIT_Occluder;
      ri->query = flareState->occlusionQuery;   
      ri->query2 = flareState->fullPixelQuery;
      ri->isSphere = true;
      ri->position = lightPos;
      if ( isVectorLight && flareState->worldRadius > 0.0f )         
         ri->scale.set( flareState->worldRadius );
      else
         ri->scale.set( mOcclusionRadius );
      ri->orientation = rpm->allocUniqueXform( lightInfo->getTransform() );         
      
      // Submit the queries.
      state->getRenderPass()->addInst( ri );
   }

   const Point3F &camPos = state->getCameraPosition();

   if ( needsRaycast )
   {
      // Use a raycast to determine occlusion.
      GameConnection *conn = GameConnection::getConnectionToServer();
      if ( !conn )
         return false;

      const bool fps = conn->isFirstPerson();
      GameBase *control = conn->getControlObject();
      if ( control && fps )
         control->disableCollision();

      RayInfo rayInfo;

      if ( !gClientContainer.castRay( camPos, lightPos, mLOSMask, &rayInfo ) )
         *outOcclusionFade = 1.0f;

      if ( control && fps )
         control->enableCollision();
   }

   // The raycast and hardware occlusion query only calculate if
   // the flare is on screen... if does not account for being 
   // partially offscreen.
   //
   // The code here clips a box against the viewport to 
   // get an approximate percentage of onscreen area.
   //
   F32 worldRadius = flareState->worldRadius > 0 ? flareState->worldRadius : mOcclusionRadius;
   if ( worldRadius > 0.0f )
   {
      F32 dist = ( camPos - lightPos ).len();
      F32 pixelRadius = state->projectRadius(dist, worldRadius);

      RectI visRect( outLightPosSS->x - pixelRadius, outLightPosSS->y - pixelRadius, 
                     pixelRadius * 2.0f, pixelRadius * 2.0f ); 
      F32 fullArea = visRect.area();

      if ( visRect.intersect( viewport ) )
      {
         F32 visArea = visRect.area();
         *outOcclusionFade *= visArea / fullArea;
         onScreen = true;
      }
      else
         *outOcclusionFade = 0.0f;
   }
   
   const bool lightVisible = onScreen && *outOcclusionFade > 0.0f;

   // To perform a fade in/out when we gain or lose visibility
   // we must update/store the visibility state and time.
   const U32 currentTime = Sim::getCurrentTime();
   if ( lightVisible != flareState->visible )
   {
      flareState->visible = lightVisible;
      flareState->visChangedTime = currentTime;
   }

   // Return the visibility delta for time fading.
   *outVisDelta = currentTime - flareState->visChangedTime;

   // Store the final occlusion fade so that it can
   // be used in reflection rendering later.
   flareState->occlusion = *outOcclusionFade;

   return lightVisible;
}

void LightFlareData::prepRender( SceneRenderState *state, LightFlareState *flareState )
{
   PROFILE_SCOPE( LightFlareData_prepRender );

   const LightInfo *lightInfo = flareState->lightInfo;

   if (  mIsZero( flareState->fullBrightness ) ||
         mIsZero( lightInfo->getBrightness() ) )
      return;

   // Figure out the element count to render.
   U32 elementCount = mElementCount;
   const bool isReflectPass = state->isReflectPass();
   if ( isReflectPass )
   {
      // Then we don't render anything this pass.
      if ( !mRenderReflectPass )
         return;

      // Find the zero distance elements which make 
      // up the corona of the light flare.
      elementCount = 0.0f;
      for ( U32 i=0; i < mElementCount; i++ )
         if ( mIsZero( mElementDist[i] ) )
            elementCount++;
   }

   // Better have something to render.
   if ( elementCount == 0 )
      return;
  
   U32 visDelta = U32_MAX;
   F32 occlusionFade = 1.0f;
   Point3F lightPosSS;
   bool lightVisible = _testVisibility( state, flareState, &visDelta, &occlusionFade, &lightPosSS );
   
   // We can only skip rendering if the light is not 
   // visible, and it has elapsed the fade out time.
   if (  mIsZero( occlusionFade ) ||
         !lightVisible && visDelta > FadeOutTime )
      return;

   const RectI &viewport = GFX->getViewport();
   Point3F oneOverViewportExtent( 1.0f / (F32)viewport.extent.x, 1.0f / (F32)viewport.extent.y, 0.0f );

   // Really convert it to screen space.
   lightPosSS.x -= viewport.point.x;
   lightPosSS.y -= viewport.point.y;
   lightPosSS *= oneOverViewportExtent;
   lightPosSS = ( lightPosSS * 2.0f ) - Point3F::One;
   lightPosSS.y = -lightPosSS.y;
   lightPosSS.z = 0.0f;

   // Take any projection offset into account so that the point where the flare's
   // elements converge is at the 'eye' point rather than the center of the viewport.
   const Point2F& projOffset = state->getCameraFrustum().getProjectionOffset();
   Point3F flareVec( -lightPosSS + Point3F(projOffset.x, projOffset.y, 0.0f) );
   const F32 flareLength = flareVec.len();
   if ( flareLength > 0.0f )
      flareVec *= 1.0f / flareLength;

   // Setup the flare quad points.
   Point3F rotatedBasePoints[4];
   dMemcpy(rotatedBasePoints, sBasePoints, sizeof( sBasePoints ));

   // Rotate the flare quad.
   F32 rot = mAcos( -1.0f * flareVec.x );
   rot *= flareVec.y > 0.0f ? -1.0f : 1.0f;
   MathUtils::vectorRotateZAxis( rot, rotatedBasePoints, 4 );

   // Here we calculate a the light source's influence on 
   // the effect's size and brightness.

   // Scale based on the current light brightness compared to its normal output.
   F32 lightSourceBrightnessScale = lightInfo->getBrightness() / flareState->fullBrightness;

   const Point3F &camPos = state->getCameraPosition();
   const Point3F &lightPos = flareState->lightMat.getPosition();   
   const bool isVectorLight = lightInfo->getType() == LightInfo::Vector;

   // Scale based on world space distance from camera to light source.
   F32 distToCamera = ( camPos - lightPos ).len();
   F32 lightSourceWSDistanceScale = isVectorLight && distToCamera > 0.0f ? 1.0f : getMin( 10.0f / distToCamera, 10.0f );

   // Scale based on screen space distance from screen position of light source to the screen center.
   F32 lightSourceSSDistanceScale = getMax( ( 1.5f - flareLength ) / 1.5f, 0.0f );

   // Scale based on recent visibility changes, fading in or out.
   F32 fadeInOutScale = 1.0f;   
   if (  lightVisible &&
         visDelta < FadeInTime && 
         flareState->occlusion > 0.0f )
      fadeInOutScale = (F32)visDelta / (F32)FadeInTime;
   else if (   !lightVisible && 
               visDelta < FadeOutTime )
      fadeInOutScale = 1.0f - (F32)visDelta / (F32)FadeOutTime;

   // This combined scale influences the size of all elements this effect renders.
   // Note we also add in a scale that is user specified in the Light.
   F32 lightSourceIntensityScale = lightSourceBrightnessScale * 
                                   lightSourceWSDistanceScale * 
                                   lightSourceSSDistanceScale * 
                                   fadeInOutScale * 
                                   flareState->scale *
                                   occlusionFade;

   if ( mIsZero( lightSourceIntensityScale ) )
      return;

   // The baseColor which modulates the color of all elements.
   //
   // These are the factors which affect the "alpha" of the flare effect.
   // Modulate more in as appropriate.
   ColorF baseColor = ColorF::WHITE * lightSourceBrightnessScale * occlusionFade;

   // Setup the vertex buffer for the maximum flare elements.
   const U32 vertCount = 4 * mElementCount;
   if (  flareState->vertBuffer.isNull() || 
         flareState->vertBuffer->mNumVerts != vertCount )
         flareState->vertBuffer.set( GFX, vertCount, GFXBufferTypeDynamic );

   GFXVertexPCT *vert = flareState->vertBuffer.lock();

   const Point2F oneOverTexSize( 1.0f / (F32)mFlareTexture.getWidth(), 1.0f / (F32)mFlareTexture.getHeight() );

   for ( U32 i = 0; i < mElementCount; i++ )
   {      
      // Skip non-zero elements for reflections.
      if ( isReflectPass && mElementDist[i] > 0.0f )
         continue;

      Point3F *basePos = mElementRotate[i] ? rotatedBasePoints : sBasePoints;

      ColorF color( baseColor * mElementTint[i] );
      if ( mElementUseLightColor[i] )
         color *= lightInfo->getColor();
      color.clamp();

      Point3F pos( lightPosSS + flareVec * mElementDist[i] * flareLength );

      const RectF &rect = mElementRect[i];
      Point3F size( rect.extent.x, rect.extent.y, 1.0f );
      size *= mElementScale[i] * mScale * lightSourceIntensityScale;

      AssertFatal( size.x >= 0.0f, "LightFlareData::prepRender - Got a negative element size?" );

      if ( size.x < 100.0f )
      {
         F32 alphaScale = mPow( size.x / 100.0f, 2 );
         color *= alphaScale;
      }

      Point2F texCoordMin, texCoordMax;
      texCoordMin = rect.point * oneOverTexSize;
      texCoordMax = ( rect.point + rect.extent ) * oneOverTexSize;          

      size.x = getMax( size.x, 1.0f );
      size.y = getMax( size.y, 1.0f );
      size *= oneOverViewportExtent;

      vert->color = color;
      vert->point = ( basePos[0] * size ) + pos;      
      vert->texCoord.set( texCoordMin.x, texCoordMax.y );
      vert++;

      vert->color = color;
      vert->point = ( basePos[1] * size ) + pos;
      vert->texCoord.set( texCoordMax.x, texCoordMax.y );
      vert++;

      vert->color = color;
      vert->point = ( basePos[2] * size ) + pos;
      vert->texCoord.set( texCoordMax.x, texCoordMin.y );
      vert++;

      vert->color = color;
      vert->point = ( basePos[3] * size ) + pos;
      vert->texCoord.set( texCoordMin.x, texCoordMin.y );
      vert++;
   }   

   flareState->vertBuffer.unlock();   

   RenderPassManager *rpm = state->getRenderPass();

   // Create and submit the render instance.   
   ParticleRenderInst *ri = rpm->allocInst<ParticleRenderInst>();
   ri->type = RenderPassManager::RIT_Particle;
   ri->vertBuff = &flareState->vertBuffer;
   ri->primBuff = &mFlarePrimBuffer;
   ri->translucentSort = true;
   ri->sortDistSq = ( lightPos - camPos ).lenSquared();
   ri->modelViewProj = &MatrixF::Identity;
   ri->bbModelViewProj = &MatrixF::Identity;
   ri->count = elementCount;
   ri->blendStyle = ParticleRenderInst::BlendGreyscale;
   ri->diffuseTex = mFlareTexture;
   ri->softnessDistance = 1.0f; 
   ri->defaultKey = ri->diffuseTex ? (uintptr_t)ri->diffuseTex : (uintptr_t)ri->vertBuff; // Sort by texture too.

   // NOTE: Offscreen partical code is currently disabled.
   ri->systemState = PSS_AwaitingHighResDraw;

   rpm->addInst( ri );
}

bool LightFlareData::_preload( bool server, String &errorStr )
{
   mElementCount = 0;
   for ( U32 i = 0; i < MAX_ELEMENTS; i++ )
   {
      if ( mElementDist[i] == -1 )
         break;
      mElementCount = i + 1;
   }   

   if ( mElementCount > 0 )
      _makePrimBuffer( &mFlarePrimBuffer, mElementCount );

   if ( !server )
   {
      if ( mFlareTextureName.isNotEmpty() )      
         mFlareTexture.set( mFlareTextureName, &GFXDefaultStaticDiffuseProfile, "FlareTexture" );  
   }

   return true;
}

void LightFlareData::_makePrimBuffer( GFXPrimitiveBufferHandle *pb, U32 count )
{
   // create index buffer based on that size
   U32 indexListSize = count * 6; // 6 indices per particle
   U16 *indices = new U16[ indexListSize ];

   for ( U32 i = 0; i < count; i++ )
   {
      // this index ordering should be optimal (hopefully) for the vertex cache
      U16 *idx = &indices[i*6];
      volatile U32 offset = i * 4;  // set to volatile to fix VC6 Release mode compiler bug
      idx[0] = 0 + offset;
      idx[1] = 1 + offset;
      idx[2] = 3 + offset;
      idx[3] = 1 + offset;
      idx[4] = 3 + offset;
      idx[5] = 2 + offset; 
   }

   U16 *ibIndices;
   GFXBufferType bufferType = GFXBufferTypeStatic;

#ifdef TORQUE_OS_XENON
   // Because of the way the volatile buffers work on Xenon this is the only
   // way to do this.
   bufferType = GFXBufferTypeVolatile;
#endif
   pb->set( GFX, indexListSize, 0, bufferType );
   pb->lock( &ibIndices );
   dMemcpy( ibIndices, indices, indexListSize * sizeof(U16) );
   pb->unlock();

   delete [] indices;
}

DefineEngineMethod( LightFlareData, apply, void, (),,
                   "Intended as a helper to developers and editor scripts.\n"
                   "Force trigger an inspectPostApply"
                   )
{
   object->inspectPostApply();
}