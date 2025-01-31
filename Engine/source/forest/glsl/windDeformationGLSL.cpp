// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "forest/glsl/windDeformationGLSL.h"
#include "forest/windDeformation.h"

#include "forest/forestItem.h"
#include "forest/forestWindMgr.h"
#include "materials/sceneData.h"
#include "scene/sceneRenderState.h"

#include "shaderGen/shaderGen.h"
#include "shaderGen/featureMgr.h"
#include "shaderGen/langElement.h"
#include "shaderGen/shaderOp.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxStructs.h"
#include "core/module.h"


static void _onRegisterFeatures( GFXAdapterType type )
{
   if ( type != OpenGL )
      return;

   FEATUREMGR->registerFeature( MFT_WindEffect, new WindDeformationGLSL );
}


MODULE_BEGIN( WindDeformationGLSL )

   MODULE_INIT_AFTER( ShaderGen )
   
   MODULE_INIT
   {
      SHADERGEN->getFeatureInitSignal().notify( _onRegisterFeatures );   
   }

MODULE_END;


WindDeformationGLSL::WindDeformationGLSL()
   : mDep( "shaders/common/gl/wind.glsl" )
{
   addDependency( &mDep );
}

void WindDeformationGLSL::determineFeature(  Material *material,
                                             const GFXVertexFormat *vertexFormat,
                                             U32 stageNum,
                                             const FeatureType &type,
                                             const FeatureSet &features,
                                             MaterialFeatureData *outFeatureData )
{
   bool enabled = vertexFormat->hasColor() && features.hasFeature( MFT_WindEffect );
   outFeatureData->features.setFeature( type, enabled );   
}

void WindDeformationGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   // We combined all the tree parameters into one float4 to
   // save constant space and reduce the memory copied to the
   // card.
   //
   // .x = bend scale
   // .y = branch amplitude
   // .z = detail amplitude
   // .w = detail frequency
   //
   Var *windParams = new Var( "windParams", "vec4" );
      windParams->uniform = true;
      windParams->constSortPos = cspPotentialPrimitive;

   // If we're instancing then we need to instance the wind direction
   // and speed as its unique for each tree instance.
   Var *windDirAndSpeed;
   if ( fd.features[MFT_UseInstancing] ) 
   {
      ShaderConnector *vertStruct = dynamic_cast<ShaderConnector *>( componentList[C_VERT_STRUCT] );
      windDirAndSpeed = vertStruct->getElement( RT_TEXCOORD );
      windDirAndSpeed->setStructName( "IN" );
      windDirAndSpeed->setName( "inst_windDirAndSpeed" );
      windDirAndSpeed->setType( "vec3" );

      mInstancingFormat->addElement( "windDirAndSpeed", GFXDeclType_Float3, windDirAndSpeed->constNum );
   }
   else
   {
      windDirAndSpeed = new Var( "windDirAndSpeed", "vec3" );
      windDirAndSpeed->uniform = true;
      windDirAndSpeed->constSortPos = cspPrimitive;
   }

   Var *accumTime = (Var*)LangElement::find( "accumTime" );
   if ( !accumTime )
   {
      accumTime = new Var( "accumTime", "float" );
      accumTime->uniform = true;
      accumTime->constSortPos = cspPass;  
   }

   // Get the transform to world space.
   Var *objTrans = getObjTrans( componentList, fd.features[MFT_UseInstancing], meta );

   // First check for an input position from a previous feature
   // then look for the default vertex position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   // Copy the input position to the output first as 
   // the wind effects are conditional.
   Var *outPosition = (Var*)LangElement::find( "inPosition" );
   if ( !outPosition )
   {
      outPosition = new Var;
      outPosition->setType( "vec3" );
      outPosition->setName( "inPosition" );
      meta->addStatement( new GenOp("   @ = @.xyz;\r\n", new DecOp( outPosition ), inPosition ) );
   }

   // Get the incoming color data
   Var *inColor = (Var*)LangElement::find( "diffuse" );

   // Do a dynamic branch based on wind force.
   if ( GFX->getPixelShaderVersion() >= 3.0f )
      meta->addStatement( new GenOp("   if ( any( bvec3(@) ) ) {\r\n", windDirAndSpeed ) );

   // Do the branch and detail bending first so that 
   // it can work in pure object space of the tree.
   LangElement *effect = 
      new GenOp(  "windBranchBending( "

                     "@, "                  // vPos
                     "normalize( IN_normal ), " // vNormal

                     "@, " // fTime
                     "@.z, " // fWindSpeed

                     "@.g, "  // fBranchPhase
                     "@.y, "    // fBranchAmp
                     "@.r, "  // fBranchAtten

                     "dot( @[3], vec4(1) ), "    // fDetailPhase
                     "@.z, "  // fDetailAmp
                     "@.w, "  // fDetailFreq
                     
                     "@.b )", // fEdgeAtten

         outPosition,    // vPos
                        // vNormal

         accumTime,  // fTime
         windDirAndSpeed,  // fWindSpeed

         inColor,    // fBranchPhase
         windParams,  // fBranchAmp
         inColor,    // fBranchAtten

         objTrans,     // fDetailPhase
         windParams, // fDetailAmp
         windParams, // fDetailFreq

         inColor ); // fEdgeAtten

   meta->addStatement( new GenOp( "   @ = @;\r\n", outPosition, effect ) );

   // Now do the trunk bending.
   meta->addStatement( new GenOp("   @ = windTrunkBending( @, @.xy, @.z * @.x );\r\n", 
      outPosition, outPosition, windDirAndSpeed, outPosition, windParams ) );

   // End the dynamic branch.
   if ( GFX->getPixelShaderVersion() >= 3.0f )
      meta->addStatement( new GenOp("   } // [branch]\r\n" ) );
}

ShaderFeatureConstHandles* WindDeformationGLSL::createConstHandles( GFXShader *shader, SimObject *userObject )
{
   WindDeformationConstHandles *handles = new WindDeformationConstHandles();
   handles->init( shader );      

   return handles;
}
