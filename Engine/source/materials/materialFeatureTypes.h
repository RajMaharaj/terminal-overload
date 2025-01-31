// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _MATERIALFEATURETYPES_H_
#define _MATERIALFEATURETYPES_H_

#ifndef _FEATURETYPE_H_
#include "shaderGen/featureType.h"
#endif


///
enum MaterialFeatureGroup
{
   /// One or more pre-transform features are 
   /// allowed at any one time and are executed
   /// in order to each other.
   MFG_PreTransform,

   /// Only one transform feature is allowed at
   /// any one time.
   MFG_Transform,

   /// 
   MFG_PostTransform,

   /// The features that need to occur before texturing
   /// takes place.  Usually these are features that will
   /// manipulate or generate texture coords.
   MFG_PreTexture,

   /// The different diffuse color features including
   /// textures and colors.
   MFG_Texture,

   /// 
   MFG_PreLighting,

   /// 
   MFG_Lighting,

   /// 
   MFG_PostLighting,

   /// Final features like fogging.
   MFG_PostProcess,

   /// Miscellaneous features that require no specialized 
   /// ShaderFeature object and are just queried as flags.
   MFG_Misc = -1,
};

/// If defined then this shader should use hardware mesh instancing.
DeclareFeatureType( MFT_UseInstancing );

/// The standard vertex transform.
DeclareFeatureType( MFT_VertTransform );

/// A special transform with paraboloid warp used 
/// in shadow and reflection rendering.
DeclareFeatureType( MFT_ParaboloidVertTransform );

/// This feature is queried from the MFT_ParaboloidVertTransform 
/// feature to detect if it needs to generate a single pass.
DeclareFeatureType( MFT_IsSinglePassParaboloid );

/// This feature does normal map decompression for DXT1/5.
DeclareFeatureType( MFT_IsDXTnm );

DeclareFeatureType( MFT_TexAnim );
DeclareFeatureType( MFT_Parallax );

DeclareFeatureType( MFT_DiffuseMap );
DeclareFeatureType( MFT_OverlayMap );
DeclareFeatureType( MFT_DetailMap );
DeclareFeatureType( MFT_DiffuseColor );
DeclareFeatureType( MFT_DetailNormalMap );

DeclareFeatureType( MFT_AccuMap );
DeclareFeatureType( MFT_AccuScale );
DeclareFeatureType( MFT_AccuDirection );
DeclareFeatureType( MFT_AccuStrength );
DeclareFeatureType( MFT_AccuCoverage );
DeclareFeatureType( MFT_AccuSpecular );

/// This feature enables vertex coloring for the diffuse channel.
DeclareFeatureType( MFT_DiffuseVertColor );

/// This feature is used to do alpha test clipping in
/// the shader which can be faster on SM3 and is needed
/// when the render state alpha test is not available.
DeclareFeatureType( MFT_AlphaTest );

DeclareFeatureType( MFT_NormalMap );
DeclareFeatureType( MFT_RTLighting );

DeclareFeatureType( MFT_IsEmissive );
DeclareFeatureType( MFT_SubSurface );
DeclareFeatureType( MFT_LightMap );
DeclareFeatureType( MFT_ToneMap );
DeclareFeatureType( MFT_VertLit );
DeclareFeatureType( MFT_VertLitTone );

DeclareFeatureType( MFT_EnvMap );
DeclareFeatureType( MFT_CubeMap );
DeclareFeatureType( MFT_PixSpecular );
DeclareFeatureType( MFT_SpecularMap );
DeclareFeatureType( MFT_GlossMap );

/// This feature is only used to detect alpha transparency
/// and does not have any code associtated with it. 
DeclareFeatureType( MFT_IsTranslucent );

///
DeclareFeatureType( MFT_IsTranslucentZWrite );

/// This feature causes MFT_NormalMap to set the world
/// space normal vector to the output color rgb.
DeclareFeatureType( MFT_NormalsOut );

DeclareFeatureType( MFT_MinnaertShading );
DeclareFeatureType( MFT_GlowMask );
DeclareFeatureType( MFT_Visibility );
DeclareFeatureType( MFT_EyeSpaceDepthOut );
DeclareFeatureType( MFT_DepthOut );
DeclareFeatureType( MFT_Fog );

/// This should be the last feature of any material that
/// renders to a HDR render target.  It converts the high
/// dynamic range color into the correct HDR encoded format.
DeclareFeatureType( MFT_HDROut );

///
DeclareFeatureType( MFT_PrePassConditioner );
DeclareFeatureType( MFT_InterlacedPrePass );

/// This feature causes MFT_ToneMap and MFT_LightMap to output their light color
/// to the second render-target
DeclareFeatureType( MFT_LightbufferMRT );

/// This feature outputs black to RenderTarget1
DeclareFeatureType( MFT_RenderTarget1_Zero );

DeclareFeatureType( MFT_Foliage );

// Texture atlasing features
DeclareFeatureType( MFT_DiffuseMapAtlas );
DeclareFeatureType( MFT_NormalMapAtlas );

// Particle features
DeclareFeatureType( MFT_ParticleNormal );

/// This feature is used to indicate that the material should use forward shading
/// instead of deferred shading (if applicable)
DeclareFeatureType( MFT_ForwardShading );

/// A special vertex feature which unpacks the imposter vertex
/// so that the rest of the material features can work on it.
DeclareFeatureType( MFT_ImposterVert );


#endif // _MATERIALFEATURETYPES_H_
