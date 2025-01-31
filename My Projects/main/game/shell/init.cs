// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

//-----------------------------------------------------------------------------
// Variables used by client scripts & code.  The ones marked with (c)
// are accessed from code.  Variables preceeded by Pref:: are client
// preferences and stored automatically in the ~/client/prefs.cs file
// in between sessions.
//
//    (c) Client::MissionFile             Mission file name
//    ( ) Client::Password                Password for server join

//    (?) Pref::Player::CurrentFOV
//    (?) Pref::Player::DefaultFov
//    ( ) Pref::Input::KeyboardTurnSpeed

//    (c) pref::Master[n]                 List of master servers
//    (c) pref::Net::RegionMask
//    (c) pref::Client::ServerFavoriteCount
//    (c) pref::Client::ServerFavorite[FavoriteCount]
//    .. Many more prefs... need to finish this off

// Moves, not finished with this either...
//    (c) firstPerson
//    $mv*Action...

//-----------------------------------------------------------------------------
// These are variables used to control the shell scripts and
// can be overriden by mods:
//-----------------------------------------------------------------------------

function initDedicated()
{
   if($dedicatedArg $= "")
   {
      error("Error: Missing Command Line argument. Usage: -dedicated <game>");
      quit();
   }
   echo("\n--------- Starting Dedicated Server ---------");
   // Pass global arguments to server...
   %args = "";
   for (%i = 1; %i < $Game::argc ; %i++)
      %args = %args SPC $Game::argv[%i];
   startServer($dedicatedArg, %args);
}

//-----------------------------------------------------------------------------

function initGUI()
{
   echo("\n--------- Initializing " @ $appName @ ": Client Scripts ---------");
   
   // Set actual window title.
   if(isObject(Canvas))
      Canvas.setWindowTitle($appName @ " (" @ $GameVersionString @ ")");

   // Make sure this variable reflects the correct state.
   $Server::Dedicated = false;

   // Game information used to query the master server
   $Client::GameTypeQuery = "Any";
   $Client::MissionTypeQuery = "Any";

   // These should be game specific GuiProfiles.  Custom profiles are saved out
   // from the Gui Editor.  Either of these may override any that already exist.
   exec("./gui/defaultGameProfiles.cs");
   exec("./gui/customProfiles.cs");
   
   // The common module provides basic client functionality
   initBaseClient();

   // Use our prefs to configure our Canvas/Window
   configureCanvas();

   // Load up the Game GUIs
   exec("./gui/hudlessGui.gui");

   // Gui scripts
   exec("./gui/hilight.cs");
   exec("./gui/mainMenuGui.cs");
   exec("./gui/startupGui.cs");
   exec("./gui/chooseLevelDlg.cs");
   exec("./gui/preloadGui.cs");
   exec("./gui/optionsDlg.cs");
   exec("./gui/optPlayer.cs");
   exec("./gui/optGraphics.cs");
   exec("./gui/optAudio.cs");
   exec("./gui/optGame.cs");
   exec("./gui/motdDlg.cs");
   exec("./gui/welcomeDlg.cs");
   exec("./gui/ingameMenuDlg.cs");

   // Load up the shell GUIs
   exec("./gui/mainMenuGui.gui");
   exec("./gui/recordingsDlg.gui");
   exec("./gui/joinServerDlg.gui");
   exec("./gui/StartupGui.gui");
   exec("./gui/chooseLevelDlg.gui");
   exec("./gui/preloadGui.gui");
   exec("./gui/optionsDlg.gui");
   exec("./gui/optPlayer.gui");
   exec("./gui/optGraphics.gui");
   exec("./gui/optAudio.gui");
   exec("./gui/optGame.gui");
   exec("./gui/motdDlg.gui");
   exec("./gui/welcomeDlg.gui");
   exec("./gui/ingameMenuDlg.gui");

   // Client scripts
   exec("./client.cs");
   exec("./serverConnection.cs");
   
   loadAutoexec("ClientInit");

   // Really shouldn't be starting the networking unless we are
   // going to connect to a remote server, or host a multi-player
   // game.
   setNetPort(0);

   // Copy saved script prefs into C++ code.
   setDefaultFov( $pref::Player::defaultFov );
   setZoomSpeed( $pref::Player::zoomSpeed );

   // Start up the main menu... this is separated out into a
   // method for easier mod override.

   if ($startWorldEditor || $startGUIEditor) {
      // Editor GUI's will start up in the primary main.cs once
      // engine is initialized.
      return;
   }

   // Connect to server if requested.
   if ($JoinGameAddress !$= "") {
      connect($JoinGameAddress, "", $Pref::Player::Name);
   }
   else {
      // Otherwise go to the splash screen.
      Canvas.setCursor("DefaultCursor");
      loadMainMenu();
   }   
}


//-----------------------------------------------------------------------------

function loadMainMenu()
{
   // Startup the client with the Main menu...
   if (isObject( MainMenuGui ))
      Canvas.setContent( MainMenuGui );
   
   Canvas.setCursor("DefaultCursor");

   // first check if we have a level file to load
   if ($levelToLoad !$= "")
   {
      %levelFile = "levels/";
      %ext = getSubStr($levelToLoad, strlen($levelToLoad) - 3, 3);
      if(%ext !$= "mis")
         %levelFile = %levelFile @ $levelToLoad @ ".mis";
      else
         %levelFile = %levelFile @ $levelToLoad;

      // Clear out the $levelToLoad so we don't attempt to load the level again
      // later on.
      $levelToLoad = "";
      
      // let's make sure the file exists
      %file = findFirstFile(%levelFile);

      if(%file !$= "")
         createAndConnectToLocalServer( "SinglePlayer", %file );
   }
}

function savePrefs()
{
   %file = $SettingsDir@"/prefs.cs";
   echo("Saving prefs to" SPC %file);
   export("$pref::*", %file, False);
}

