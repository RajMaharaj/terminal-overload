// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

function XaNotc1CatSniperMoveMap_yaw(%val)
{
   MouseInputGraph_input(0, %val);

   %yawAdj = XaNotc1CatMoveMap_getMouseAdjustAmount(%val);
   if(ServerConnection.isControlObjectRotDampedCamera())
   {
      // Clamp and scale
      %yawAdj = mClamp(%yawAdj, -m2Pi()+0.01, m2Pi()-0.01);
      %yawAdj *= 0.5;
   }
   
   MoveManager_addYaw(%yawAdj);
}

function XaNotc1CatSniperMoveMap_pitch(%val)
{
   MouseInputGraph_input(1, %val);

   %pitchAdj = XaNotc1CatMoveMap_getMouseAdjustAmount(%val);
   if(ServerConnection.isControlObjectRotDampedCamera())
   {
      // Clamp and scale
      %pitchAdj = mClamp(%pitchAdj, -m2Pi()+0.01, m2Pi()-0.01);
      %pitchAdj *= 0.5;
   }

   if($Pref::NOTC1::MouseInvertY)
      %pitchAdj = -%pitchAdj;
      
   if(XaNotc1CatSniperMoveMap.zAdjustSights)
   {
      %val = mClamp(%val*$pref::Input::LinkMouseSensitivity*0.01, -1, 1);
      notcCatSniperGui.setElevation(notcCatSniperGui.zElevation - %val);
   }
   else
      MoveManager_addPitch(%pitchAdj);
}

function XaNotc1CatSniperMoveMap_trigger1(%val)
{
   XaNotc1CatSniperMoveMap.zAdjustSights = %val;
}

function XaNotc1CatSniperMoveMap_trigger3(%val)
{
   notcCatSniperGui.zScanForTargets = %val;
   if(%val)
      notcCatSniperGui.scanForTarget();
}

function XaNotc1CatSniperMoveMap_mouseZoom(%val)
{
	if(Canvas.isCursorOn())
		return;

   if(!ServerConnection.isFirstPerson())
      return;

   if($MouseZoomSteps $= "")
      $MouseZoomSteps = $Pref::NOTC1::MouseZoomSteps;
      
   %oldFov = $cameraFov;

	%minFov = ServerConnection.getControlObject().getDataBlock().cameraMinFov;
   %maxFov = 45;
   
   %newFov = %oldFov * (%val > 0 ? 0.5 : 2);
   %newFov = mClamp(%newFov, %minFov, %maxFov);
   
   if(%newFov == %oldFov)
      return;

   $cameraFov = %newFov;
   setFov(%newFov);
   notcCatSniperGui.updateView();
   sfxPlayOnce(AudioGui, "content/o/rotc/p.5.4/sounds/rotc/weaponSwitch.wav");
}

