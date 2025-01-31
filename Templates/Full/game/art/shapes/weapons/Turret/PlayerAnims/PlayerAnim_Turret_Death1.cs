// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

singleton TSShapeConstructor(PlayerAnim_Turret_Death1DAE)
{
   baseShape = "./PlayerAnim_Turret_Death1.dae";
   neverImport = "EnvironmentAmbientLight";
   loadLights = "0";
};

function PlayerAnim_Turret_Death1DAE::onLoad(%this)
{
   %this.setSequenceCyclic("ambient", "false");
   %this.addSequence("ambient", "Death1", "260", "300");
}
