// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

new ShaderData( AL_ShadowVisualizeShader )
{
   DXVertexShaderFile = "shaders/common/guiMaterialV.hlsl";
   DXPixelShaderFile  = "shaders/common/lighting/advanced/dbgShadowVisualizeP.hlsl";
   
   OGLVertexShaderFile = "shaders/common/gl/guiMaterialV.glsl";
   OGLPixelShaderFile  = "shaders/common/lighting/advanced/gl/dbgShadowVisualizeP.glsl";
   
   samplerNames[0] = "$shadowMap";
   samplerNames[1] = "$depthViz";
   
   pixVersion = 2.0;
};

new CustomMaterial( AL_ShadowVisualizeMaterial )
{
   shader = AL_ShadowVisualizeShader;
   stateBlock = AL_DepthVisualizeState;
   
   sampler["shadowMap"] = "#AL_ShadowVizTexture";
   sampler["depthViz"] = "depthviz";

   pixVersion = 2.0;
};

singleton GuiControlProfile( AL_ShadowLabelTextProfile )
{
   fontColor = "0 0 0";
   autoSizeWidth = true;
   autoSizeHeight = true;
   justify = "left";
   fontSize = 14;
};

/// Toggles the visualization of the pre-pass lighting buffer.
function toggleShadowViz()
{
   if ( AL_ShadowVizOverlayCtrl.isAwake() )
   {
      setShadowVizLight( 0 );
      Canvas.popDialog( AL_ShadowVizOverlayCtrl );
   }
   else
   {
      Canvas.pushDialog( AL_ShadowVizOverlayCtrl, 100 );
      _setShadowVizLight( EWorldEditor.getSelectedObject( 0 ) );
   }
}

/// Called from the WorldEditor when an object is selected.
function _setShadowVizLight( %light, %force )
{
   if ( !AL_ShadowVizOverlayCtrl.isAwake() )   
      return;
      
   if ( AL_ShadowVizOverlayCtrl.isLocked && !%force )
      return;
      
   // Resolve the object to the client side.
   if ( isObject( %light ) )
   {      
      %clientLight = serverToClientObject( %light );
      %sizeAndAspect = setShadowVizLight( %clientLight );
   }      
   
   AL_ShadowVizOverlayCtrl-->MatCtrl.setMaterial( "AL_ShadowVisualizeMaterial" );      
   
   %text = "ShadowViz";
   if ( isObject( %light ) )
      %text = %text @ " : " @ getWord( %sizeAndAspect, 0 ) @ " x " @ getWord( %sizeAndAspect, 1 );
      
   AL_ShadowVizOverlayCtrl-->WindowCtrl.text = %text;   
}

/// For convenience, push the viz dialog and set the light manually from the console.
function showShadowVizForLight( %light )
{   
   if ( !AL_ShadowVizOverlayCtrl.isAwake() )
      Canvas.pushDialog( AL_ShadowVizOverlayCtrl, 100 );
   _setShadowVizLight( %light, true );
}

// Prevent shadowViz from changing lights in response to editor selection
// events until unlock is called. The only way a vis light will change while locked
// is if showShadowVizForLight is explicitly called by the user.
function lockShadowViz()
{
   AL_ShadowVizOverlayCtrl.islocked = true;
}

function unlockShadowViz()
{
   AL_ShadowVizOverlayCtrl.islocked = false;
}