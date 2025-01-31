// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

function TE::joinTeam(%client, %teamId)
{
   //echo("TE::joinTeam()" SPC %client SPC "-> team" SPC %teamId);
   if(ETH::joinTeam(%client, %teamId))
   {
      TE::setupHud(%client);
      if($Server::PlayerCount == 1)
         TE::startNewRound();
      else
         TE::checkRoundEnd();

      return true;
   }
   
   return false;
}

function TE::startNewRound()
{
   echo("TE::startNewRound()");

   if(Game.zRestartRoundThread !$= "")
      cancel(Game.zRestartRoundThread);
   Game.zRestartRoundThread = "";
   
   Game.roundSetup = true;
   
   // Update team colors here so mappers don't have to restart the
   // server when experimenting with team colors.
   Game.team0.color = theLevelInfo.teamColors0;
   Game.team1.color = theLevelInfo.teamColors1;
   Game.team2.color = theLevelInfo.teamColors2;

   // Cleanup
   for( %idx = MissionCleanup.getCount()-1; %idx >= 0; %idx-- )
   {
      %obj = MissionCleanup.getObject(%idx);
      if(!%obj.isMethod("getType"))
         continue;
         
      if(%obj.getType() & $TypeMasks::CameraObjectType)
         continue;
         
      if(%obj.getType() & $TypeMasks::ProjectileObjectType
      || %obj.getType() & $TypeMasks::ShapeBaseObjectType)
         %obj.delete();
   }

   Game.team1.numPlayersOnRoundStart = 0;
   Game.team2.numPlayersOnRoundStart = 0;
   Game.team1.numCATs = 0;
   Game.team2.numCATs = 0;
   
   $Game::defaultPlayerClass = "Player";
   $Game::defaultPlayerDataBlock = "FrmStandardCat";
   
   echo("Respawning players...");

   for( %clientIndex = 0; %clientIndex < ClientGroup.getCount(); %clientIndex++ )
   {
      %client = ClientGroup.getObject(%clientIndex);
      
      TE::resetLoadout(%client);

      // Do not respawn observers.
      if(%client.team == Game.team1 || %client.team == Game.team2 )
         Game.preparePlayer(%client);
   }
   
   $Game::defaultPlayerClass = "Etherform";
   $Game::defaultPlayerDataBlock = "FrmEtherform";

   //serverUpdateMusic();
   //serverUpdateGameStatus();

   Game.roundSetup = false;
   echo("New round setup complete");
}

function TE::checkRoundEnd()
{
   echo("TE::checkRoundEnd()");
   //echo(Game.roundSetup);
   //echo(Game.zRestartRoundThread);

   if(Game.roundSetup || Game.zRestartRoundThread !$= "")
      return;

   error("Round ended");

   %team1numCATs = ETH::getTeamCATCount(1);
   %team2numCATs = ETH::getTeamCATCount(2);
      
   if(%team1numCATs == 0 && %team2numCATs > 0)
   {
      Game.team2.score++;
      centerPrintAll("Team 2 has won this round!",3);
      serverPlay2D(BlueVictorySound);
      Game.zRestartRoundThread = schedule(5000, MissionGroup, "TE::startNewRound");
   }
   else if(%team2numCATs == 0 && %team1numCATs > 0)
   {
      Game.team1.score++;
      centerPrintAll("Team 1 has won this round!",3);
      serverPlay2D(RedVictorySound);
      Game.zRestartRoundThread = schedule(5000, MissionGroup, "TE::startNewRound");
   }
   else if(%team2numCATs == 0 && %team1numCATs == 0)
   {
      centerPrintAll("It's a draw!",3);
      serverPlay2D(RedVictorySound);
      Game.zRestartRoundThread = schedule(5000, MissionGroup, "TE::startNewRound");
   }
}

function TE::setupHud(%client)
{
   // HudIcons
   %client.HudIcons_SetHudInfoDatasetType_Color(2);
   %client.HudIcons_SetHudInfoDatasetType_Icon(3);
   %client.HudIcons_ClearColors();
   //%client.HudIcons_AddColor(1, Game.team1.color);
   //%client.HudIcons_AddColor(2, Game.team2.color);
   %client.HudIcons_ClearIcons();
   //%client.MinimapHud_AddIcon(1, "content/xa/notc/core/icons/p1/class0.8x8.png", 8);
   //%client.MinimapHud_AddIcon(2, "content/xa/notc/core/icons/p1/class1.8x8.png", 8);
   //%client.MinimapHud_AddIcon(3, "content/xa/notc/core/icons/p1/class2.8x8.png", 8);
   //%client.MinimapHud_AddIcon(4, "content/xa/notc/core/icons/p1/class3.8x8.png", 8);
   %client.HudIcons_AddIcon(128, "content/xa/notc/core/icons/p1/etherform.256x256.png", 0);

   // MinimapHUD
   %client.MinimapHud_SetHudInfoDatasetType_Color(2);
   %client.MinimapHud_SetHudInfoDatasetType_Icon(3);
   %client.MinimapHud_ClearColors();
   %client.MinimapHud_AddColor(1, Game.team1.color);
   %client.MinimapHud_AddColor(2, Game.team2.color);
   %client.MinimapHud_ClearIcons();
   %client.MinimapHud_AddIcon(1, "content/xa/notc/core/icons/p1/cat.9x9.png", 9);
   %client.MinimapHud_AddIcon(128, "content/xa/notc/core/icons/p1/etherform.8x8.png", 8);

   if(%client.zInitialHudSetupDone && !%client.zDemoRecordingSetupInProgress)
      return;

   // LoadoutHud
   %active[0] = false;
   %active[1] = false;
   %active[2] = false;
   %active[3] = false;
   %active[4] = false;
   %active[5] = false;
   %active[6] = false;
   %icon[0] = "content/xa/notc/core/icons/p1/smg1.32x32.png";
   %icon[1] = "content/xa/notc/core/icons/p1/mgl1.32x32.png";
   %icon[2] = "content/xa/notc/core/icons/p1/sr1.32x32.png";
   %icon[3] = "content/xa/notc/core/icons/p1/mg1.32x32.png";
   for(%i = 0; %i < 6; %i++)
      %client.LoadoutHud_UpdateSlot(%i, %active[%i], %icon[%i], %client.zLoadoutProgress[%i]);
   %client.LoadoutHud_SelectSlot(%client.zActiveLoadout);

   %client.zInitialHudSetupDone = true;
}

function TE::resetLoadout(%client)
{
   for(%i = 0; %i < 6; %i++)
   {
      %client.zLoadoutProgress[%i] = 1.0;
      %client.zLoadoutProgressDt[%i] = 0.0;
   }
}

function TE::loadoutEtherform(%player)
{
   // Setup ShapeBase HudInfo object icon
   %player.zShapeBaseHudInfo.setDatasetType(2, $HudInfoDatasetType::IconID);
   %player.zShapeBaseHudInfo.setDatasetIntField(2, 128);
   
   copyPalette(%player, %player.zBallastShape1);
   copyPalette(%player, %player.zBallastShape2);
}

function TE::loadoutPlayer(%player)
{
   //echo("TE::loadoutPlayer()");

   %client = %player.client;
   if(!isObject(%client))
      return;

   // Setup ShapeBase HudInfo object icon
   %player.zShapeBaseHudInfo.setDatasetType(2, $HudInfoDatasetType::IconID);
   %player.zShapeBaseHudInfo.setDatasetIntField(2, 1);
   
   %player.setInventory(ItemDamper, 1);
   if($Server::NOTC::Mutator::VAMP)
      %player.setInventory(ItemVAMP, 1);
   %player.setInventory(ItemImpShield, 1);
   %player.setInventory(ItemLauncher, 1);
   %player.setInventory(ItemBounce, 1);
   %player.setInventory(ItemXJump, 1);

   %player.setInventory(ItemG1Launcher, 1);
   
   %player.setInventory(ItemEtherboard, 1);
   %player.setInventory(ItemStealth, 1);
   
   %player.setInventory(WpnRazorDiscAmmo, 9999);
   
   %player.setInventory(WpnSMG3, 1);
   %player.setInventory(WpnRFL1, 1);
   %player.setInventory(WpnMGL2, 1);
   %player.setInventory(WpnSG3, 1);
   %player.setInventory(WpnSR2, 1);
   %player.setInventory(WpnSMG4, 1);
   %player.setInventory(WpnMG2, 1);
   %player.setInventory(WpnSG2, 1);
   
   %player.clearWeaponCycle();

   %player.addToWeaponCycle(WpnSMG3);
   %player.addToWeaponCycle(WpnRFL1);
   %player.addToWeaponCycle(WpnMGL2);
   %player.addToWeaponCycle(WpnSG3);
   %player.addToWeaponCycle(WpnSR2);
   %player.addToWeaponCycle(WpnSMG4);
   %player.addToWeaponCycle(WpnMG2);
   %player.addToWeaponCycle(WpnSG2);

   %player.mountImage(WpnSMG3Image, 0);
}

