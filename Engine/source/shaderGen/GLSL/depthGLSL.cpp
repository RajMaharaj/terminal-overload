// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "shaderGen/GLSL/depthGLSL.h"

#include "materials/materialFeatureTypes.h"
#include "materials/materialFeatureData.h"


void EyeSpaceDepthOutGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                          const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   output = meta;

   // grab output
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outWSEyeVec = connectComp->getElement( RT_TEXCOORD );
   outWSEyeVec->setName( "wsEyeVec" );
   outWSEyeVec->setStructName( "OUT" );

   // grab incoming vert position   
   Var *wsPosition = new Var( "depthPos", "float3" );
   getWsPosition( componentList, fd.features[MFT_UseInstancing], meta, new DecOp( wsPosition ) );

   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if( !eyePos )
   {
      eyePos = new Var;
      eyePos->setType("float3");
      eyePos->setName("eyePosWorld");
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }

   meta->addStatement( new GenOp( "   @ = float4( @.xyz - @, 1 );\r\n", outWSEyeVec, wsPosition, eyePos ) );
}

void EyeSpaceDepthOutGLSL::processPix( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{      
   MultiLine *meta = new MultiLine;

   // grab connector position
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *wsEyeVec = connectComp->getElement( RT_TEXCOORD );
   wsEyeVec->setName( "wsEyeVec" );
   wsEyeVec->setStructName( "IN" );
   wsEyeVec->setType( "float4" );
   wsEyeVec->mapsToSampler = false;
   wsEyeVec->uniform = false;

   // get shader constants
   Var *vEye = new Var;
   vEye->setType("float3");
   vEye->setName("vEye");
   vEye->uniform = true;
   vEye->constSortPos = cspPass;

   // Expose the depth to the depth format feature
   Var *depthOut = new Var;
   depthOut->setType("float");
   depthOut->setName(getOutputVarName());

   LangElement *depthOutDecl = new DecOp( depthOut );

   meta->addStatement( new GenOp( "#ifndef CUBE_SHADOW_MAP\r\n" ) );
   meta->addStatement( new GenOp( "   @ = dot(@, (@.xyz / @.w));\r\n", depthOutDecl, vEye, wsEyeVec, wsEyeVec ) );
   meta->addStatement( new GenOp( "#else\r\n" ) );

   Var *farDist = (Var*)Var::find( "oneOverFarplane" );
   if ( !farDist )
   {
      farDist = new Var;
      farDist->setType("float4");
      farDist->setName("oneOverFarplane");
      farDist->uniform = true;
      farDist->constSortPos = cspPass;
   }

   meta->addStatement( new GenOp( "   @ = length( @.xyz / @.w ) * @.x;\r\n", depthOutDecl, wsEyeVec, wsEyeVec, farDist ) );      
   meta->addStatement( new GenOp( "#endif\r\n" ) );

   // If there isn't an output conditioner for the pre-pass, than just write
   // out the depth to rgba and return.
   if( !fd.features[MFT_PrePassConditioner] )
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( new GenOp( "float4(float3(@),1)", depthOut ), Material::None ) ) );
   
   output = meta;
}

ShaderFeature::Resources EyeSpaceDepthOutGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources temp;

   // Passing from VS->PS:
   // - world space position (wsPos)
   temp.numTexReg = 1; 

   return temp; 
}


void DepthOutGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Grab the output vert.
   Var *outPosition = (Var*)LangElement::find( "gl_Position" ); //hpos

   // Grab our output depth.
   Var *outDepth = connectComp->getElement( RT_TEXCOORD );
   outDepth->setName( "depth" );
   outDepth->setStructName( "OUT" );
   outDepth->setType( "float" );

   output = new GenOp( "   @ = @.z / @.w;\r\n", outDepth, outPosition, outPosition );
}

void DepthOutGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // grab connector position
   Var *depthVar = connectComp->getElement( RT_TEXCOORD );
   depthVar->setName( "depth" );
   depthVar->setStructName( "IN" );
   depthVar->setType( "float" );
   depthVar->mapsToSampler = false;
   depthVar->uniform = false;

   /*
   // Expose the depth to the depth format feature
   Var *depthOut = new Var;
   depthOut->setType("float");
   depthOut->setName(getOutputVarName());
   */

   LangElement *depthOut = new GenOp( "float4( @, 0, 0, 1 )", depthVar );

   output = new GenOp( "   @;\r\n", assignColor( depthOut, Material::None ) );
}

ShaderFeature::Resources DepthOutGLSL::getResources( const MaterialFeatureData &fd )
{
   // We pass the depth to the pixel shader.
   Resources temp;
   temp.numTexReg = 1; 

   return temp; 
}
