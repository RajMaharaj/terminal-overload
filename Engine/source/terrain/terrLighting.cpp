// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "terrain/terrRender.h"
#include "lighting/lightInfo.h"
#include "scene/sceneRenderState.h"

/*

U32 TerrainRender::testSquareLights(GridSquare *sq, S32 level, const Point2I &pos, U32 lightMask)
{
   
   // Calculate our Box3F for this GridSquare
   Point3F boxMin(pos.x * mSquareSize + mBlockPos.x, pos.y * mSquareSize + mBlockPos.y, fixedToFloat(sq->minHeight));
   F32 blockSize = F32(mSquareSize * (1 << level));
   F32 blockHeight = fixedToFloat(sq->maxHeight - sq->minHeight);
   Point3F boxMax(boxMin);
   boxMax += Point3F(blockSize, blockSize, blockHeight);
   Box3F gridBox(boxMin, boxMax);

   U32 retMask = 0;

   for(S32 i = 0; (lightMask >> i) != 0; i++)
   {
      if(lightMask & (1 << i))
      {
         if (mTerrainLights[i].light->mType != LightInfo::Vector)
         {
            // test the visibility of this light to box         
            F32 dist = gridBox.getDistanceFromPoint(mTerrainLights[i].pos);
            static F32 minDist = 1e14f;
            minDist = getMin(minDist, dist);
            if(dist < mTerrainLights[i].radius)
               retMask |= (1 << i);
         } else {
            retMask  |= (1 << i);
         }
      }
   }
   return retMask;
}

void TerrainRender::buildLightArray(SceneState * state)
{
   PROFILE_SCOPE(TerrainRender_buildLightArray);

   mDynamicLightCount = 0;
   if ((mTerrainLighting == NULL) || (!TerrainRender::mEnableTerrainDynLights))
      return;

   static LightInfoList lights;
   lights.clear();   

   LIGHTMGR->getBestLights(lights);
   // create terrain lights from these...
   U32 curIndex = 0;
   for(U32 i = 0; i < lights.size(); i++)
   {
      LightInfo* light = lights[i];
      if((light->mType != LightInfo::Point) && (light->mType != LightInfo::Spot))
         continue;

      // set the 'fo
      TerrLightInfo & info = mTerrainLights[curIndex++];
      mCurrentBlock->getWorldTransform().mulP(light->mPos, &info.pos);
      info.radius = light->getRadius();
      info.light = light;
   }

   mDynamicLightCount = curIndex;
}

*/
