// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

package Commands
{
   function clientCmdSetPlayGui(%ctrl)
   {
      $PlayGui = %ctrl;
   }

   function clientCmdViewSetMotionBlur(%enabled, %velmul)
   {
      if(%enabled !$= "")
      {
         if(%enabled)
            MotionBlurFX.enable();
         else
            MotionBlurFX.disable();
      }

      if(%velmul !$= "")
         $PostFX::MotionBlur::VelMul = %velmul;
   }

   function clientCmdSyncClock(%time)
   {
      // Time update from the server, this is only sent at the start of a mission
      // or when a client joins a game in progress.
   }

   function clientCmdGridCreateExplosions(%grid, %datablock, %griddata)
   {
      if(!isObject(%datablock))
         return;

      %grid = ServerConnection.resolveGhostID(%grid);
      if(%grid == 0)
         return;

      %numExplosions = getWordCount(%griddata) / 4;
      for(%i = 0; %i < %numExplosions; %i++)
      {
         %gridPosX = getWord(%griddata, 4*%i+0);
         %gridPosY = getWord(%griddata, 4*%i+1);
         %gridPosZ = getWord(%griddata, 4*%i+2);
         %amount = getWord(%griddata, 4*%i+3);
         for(%j = 0; %j < %amount; %j++)
         {
            %gridPos = %gridPosX SPC %gridPosY SPC %gridPosZ + %j;
            %worldPos = %grid.gridToWorld(%gridPos);
            new Explosion()
            {
               position = %worldPos;
               dataBlock = %datablock;
            };
         }
      }
   }
   
   function clientCmdCreateExplosion(%datablock, %pos, %norm, %colorI)
   {
      if(!isObject(%datablock))
         return;
         
      if(%colorI $= "")
         %colorI = "255 255 255 255";

      %obj = new Explosion()
      {
         position = %pos;
         normal = %norm;
         dataBlock = %datablock;
         paletteColors[0] = %colorI;
      };
   }
   
   function clientCmdSetDamageDirection(%dir)
   {
      %content = Canvas.getContent();
      if(%content.isMethod("setDamageDirection"))
         %content.setDamageDirection(%dir);
   }
};

activatePackage(Commands);

