// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

datablock SFXPlayList(WpnMGL1FireSoundList)
{
   description = AudioClose3D;
   track[0] = WpnMGL1FireSound;
};

datablock SFXProfile(WpnMGL1FireSound)
{
   filename = "content/o/rotc/p.5.4/sounds/rotc/fire8";
   description = AudioClose3D;
   preload = true;
};

datablock SFXProfile(WpnMGL1DryFireSound)
{
   filename = "content/o/rotc/p.5.4/sounds/rotc/weaponEmpty";
   description = AudioClose3D;
   preload = true;
};

datablock SFXProfile(WpnMGL1SwitchinSound)
{
   filename = "content/o/torque3d/3.0/sound/weapons/wpn_lurker_switchin";
   description = AudioClose3D;
   preload = true;
};

datablock SFXProfile(WpnMGL1ProjectileSound)
{
   filename = "content/o/rotc/p.5.4/sounds/rotc/slide3";
   description = AudioCloseLoop3D;
   preload = true;
};

datablock SFXProfile(WpnMGL1ProjectileExplosionSound)
{
   filename = "content/o/rotc/p.5.4/sounds/rotc/explosion13";
   description = AudioClose3D;
   preload = true;
};




