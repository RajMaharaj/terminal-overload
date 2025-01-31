// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

//-----------------------------------------------------------------------------
// Data
//-----------------------------------------------------------------------------
uniform sampler2D diffuseMap, refractMap;
uniform vec4 shadeColor;

in vec2 TEX0;
in vec4 TEX1;

out vec4 OUT_col;

//-----------------------------------------------------------------------------
// Main                                                                        
//-----------------------------------------------------------------------------
void main()
{
   vec4 diffuseColor = texture( diffuseMap, TEX0 );
   vec4 reflectColor = textureProj( refractMap, TEX1 );

   OUT_col = diffuseColor + reflectColor * diffuseColor.a;
}
