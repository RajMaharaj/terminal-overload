// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.


//---------------------------------------------------------------------------
// Message Hud
//---------------------------------------------------------------------------

// chat hud sizes in lines
$outerChatLenY[1] = 4;
$outerChatLenY[2] = 9;
$outerChatLenY[3] = 13;

// Only play sound files that are <= 5000ms in length.
$MaxMessageWavLength = 5000;

// Helper function to play a sound file if the message indicates.
// Returns starting position of wave file indicator.
function playMessageSound(%message, %voice, %pitch)
{
   // Search for wav tag marker.
   %wavStart = strstr(%message, "~w");
   if (%wavStart == -1)
   {
      return -1;
   }

   %wav = getSubStr(%message, %wavStart + 2, 1000);

   if (%voice !$= "")
   {
      %wavFile = "content/o/torque3d/3.0/sound/voice/" @ %voice @ "/" @ %wav;
   }
   else
      %wavFile = "content/o/torque3d/3.0/sound/" @ %wav;

   if (strstr(%wavFile, ".wav") != (strlen(%wavFile) - 4))
      %wavFile = %wavFile @ ".wav";

   // XXX This only expands to a single filepath, of course; it
   // would be nice to support checking in each mod path if we
   // have multiple mods active.
   %wavFile = ExpandFilename(%wavFile);

   %wavSource = sfxCreateSource(AudioMessage, %wavFile);

   if (isObject(%wavSource))
   {
      %wavLengthMS = %wavSource.getDuration() * %pitch;

      if (%wavLengthMS == 0)
         error("** WAV file \"" @ %wavFile @ "\" is nonexistent or sound is zero-length **");
      else if (%wavLengthMS <= $MaxMessageWavLength)
      {
         if (isObject($ClientChatHandle[%sender]))
            $ClientChatHandle[%sender].delete();

         $ClientChatHandle[%sender] = %wavSource;

         if (%pitch != 1.0)
            $ClientChatHandle[%sender].setPitch(%pitch);

         $ClientChatHandle[%sender].play();
      }
      else
         error("** WAV file \"" @ %wavFile @ "\" is too long **");
   }
   else
      error("** Unable to load WAV file : \"" @ %wavFile @ "\" **");

   return %wavStart;
}


// All messages are stored in this HudMessageVector, the actual
// MainChatHud only displays the contents of this vector.

new MessageVector(HudMessageVector);
$LastHudTarget = 0;


//-----------------------------------------------------------------------------
function onChatMessage(%message, %voice, %pitch)
{
   // XXX Client objects on the server must have voiceTag and voicePitch
   // fields for values to be passed in for %voice and %pitch... in
   // this example mod, they don't have those fields.

   // Clients are not allowed to trigger general game sounds with their
   // chat messages... a voice directory MUST be specified.
   if (%voice $= "") {
      %voice = "default";
   }

   // See if there's a sound at the end of the message, and play it.
   if ((%wavStart = playMessageSound(%message, %voice, %pitch)) != -1) {
      // Remove the sound marker from the end of the message.
      %message = getSubStr(%message, 0, %wavStart);
   }

   // Chat goes to the chat HUD.
   if (getWordCount(%message)) {
      ChatHud.addLine(%message);
   }
}

function onServerMessage(%message)
{
   // See if there's a sound at the end of the message, and play it.
   if ((%wavStart = playMessageSound(%message)) != -1) {
      // Remove the sound marker from the end of the message.
      %message = getSubStr(%message, 0, %wavStart);
   }

   // Server messages go to the chat HUD too.
   if (getWordCount(%message)) {
      ChatHud.addLine(%message);
   }
}



//-----------------------------------------------------------------------------
// MainChatHud methods
//-----------------------------------------------------------------------------

function MainChatHud::onWake( %this )
{
   // set the chat hud to the users pref
   %this.setChatHudLength( $Pref::ChatHudLength );
}


//------------------------------------------------------------------------------

function MainChatHud::setChatHudLength( %this, %length )
{
   %textHeight = ChatHud.Profile.fontSize + ChatHud.lineSpacing;
   if (%textHeight <= 0)
      %textHeight = 12;
   %lengthInPixels = $outerChatLenY[%length] * %textHeight;
   %chatMargin = getWord(OuterChatHud.extent, 1) - getWord(ChatScrollHud.Extent, 1)
                  + 2 * ChatScrollHud.profile.borderThickness;
   OuterChatHud.setExtent(firstWord(OuterChatHud.extent), %lengthInPixels + %chatMargin);
   ChatScrollHud.scrollToBottom();
   ChatPageDown.setVisible(false);
}


//------------------------------------------------------------------------------

function MainChatHud::nextChatHudLen( %this )
{
   %len = $pref::ChatHudLength++;
   if ($pref::ChatHudLength == 4)
      $pref::ChatHudLength = 1;
   %this.setChatHudLength($pref::ChatHudLength);
}


//-----------------------------------------------------------------------------
// ChatHud methods
// This is the actual message vector/text control which is part of
// the MainChatHud dialog
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

function ChatHud::addLine(%this,%text)
{
   //first, see if we're "scrolled up"...
   %textHeight = %this.profile.fontSize + %this.lineSpacing;
   if (%textHeight <= 0)
      %textHeight = 12;

   %scrollBox = %this.getGroup();
   %chatScrollHeight = getWord(%scrollBox.extent, 1) - 2 * %scrollBox.profile.borderThickness;
   %chatPosition = getWord(%this.extent, 1) - %chatScrollHeight + getWord(%this.position, 1) - %scrollBox.profile.borderThickness;
   %linesToScroll = mFloor((%chatPosition / %textHeight) + 0.5);
   if (%linesToScroll > 0)
      %origPosition = %this.position;

   //remove old messages from the top only if scrolled down all the way
   while( !chatPageDown.isVisible() && HudMessageVector.getNumLines() && (HudMessageVector.getNumLines() >= $pref::HudMessageLogSize))
   {
      %tag = HudMessageVector.getLineTag(0);
      if(%tag != 0)
         %tag.delete();
      HudMessageVector.popFrontLine();
   }

   //add the message...
   HudMessageVector.pushBackLine(%text, $LastHudTarget);
   $LastHudTarget = 0;

   //now that we've added the message, see if we need to reset the position
   if (%linesToScroll > 0)
   {
      chatPageDown.setVisible(true);
      %this.position = %origPosition;
   }
   else
      chatPageDown.setVisible(false);
}


//-----------------------------------------------------------------------------

function ChatHud::pageUp(%this)
{
   // Find out the text line height
   %textHeight = %this.profile.fontSize + %this.lineSpacing;
   if (%textHeight <= 0)
      %textHeight = 12;

   %scrollBox = %this.getGroup();

   // Find out how many lines per page are visible
   %chatScrollHeight = getWord(%scrollBox.extent, 1) - 2 * %scrollBox.profile.borderThickness;
   if (%chatScrollHeight <= 0)
      return;

   %pageLines = mFloor(%chatScrollHeight / %textHeight) - 1; // have a 1 line overlap on pages
   if (%pageLines <= 0)
      %pageLines = 1;

   // See how many lines we actually can scroll up:
   %chatPosition = -1 * (getWord(%this.position, 1) - %scrollBox.profile.borderThickness);
   %linesToScroll = mFloor((%chatPosition / %textHeight) + 0.5);
   if (%linesToScroll <= 0)
      return;

   if (%linesToScroll > %pageLines)
      %scrollLines = %pageLines;
   else
      %scrollLines = %linesToScroll;

   // Now set the position
   %this.position = firstWord(%this.position) SPC (getWord(%this.position, 1) + (%scrollLines * %textHeight));

   // Display the pageup icon
   chatPageDown.setVisible(true);
}


//-----------------------------------------------------------------------------

function ChatHud::pageDown(%this)
{
   // Find out the text line height
   %textHeight = %this.profile.fontSize + %this.lineSpacing;
   if (%textHeight <= 0)
      %textHeight = 12;

   %scrollBox = %this.getGroup();

   // Find out how many lines per page are visible
   %chatScrollHeight = getWord(%scrollBox.extent, 1) - 2 * %scrollBox.profile.borderThickness;
   if (%chatScrollHeight <= 0)
      return;

   %pageLines = mFloor(%chatScrollHeight / %textHeight) - 1;
   if (%pageLines <= 0)
      %pageLines = 1;

   // See how many lines we actually can scroll down:
   %chatPosition = getWord(%this.extent, 1) - %chatScrollHeight + getWord(%this.position, 1) - %scrollBox.profile.borderThickness;
   %linesToScroll = mFloor((%chatPosition / %textHeight) + 0.5);
   if (%linesToScroll <= 0)
      return;

   if (%linesToScroll > %pageLines)
      %scrollLines = %pageLines;
   else
      %scrollLines = %linesToScroll;

   // Now set the position
   %this.position = firstWord(%this.position) SPC (getWord(%this.position, 1) - (%scrollLines * %textHeight));

   // See if we have should (still) display the pagedown icon
   if (%scrollLines < %linesToScroll)
      chatPageDown.setVisible(true);
   else
      chatPageDown.setVisible(false);
}


//-----------------------------------------------------------------------------
// Support functions
//-----------------------------------------------------------------------------

function pageUpMessageHud()
{
   ChatHud.pageUp();
}

function pageDownMessageHud()
{
   ChatHud.pageDown();
}

function cycleMessageHudSize()
{
   MainChatHud.nextChatHudLen();
}

//----------------------------------------------------------------------------
// Enter Chat Message Hud
//----------------------------------------------------------------------------

//------------------------------------------------------------------------------

function MessageHud::open(%this)
{
   %offset = 6;

   if(%this.isAwake())
      return;

   if(%this.isTeamMsg)
   {
      notcMessageHud_Text.setText("CHAT.TEAM>");
      notcMessageHud_Edit.setPosition(107, 2);
      notcMessageHud_Edit.setExtent("386, 24");
      notcMessageHud_Edit.setText(notcMessageHud_Edit.zCurrentTeamMsg);
   }
   else
   {
      notcMessageHud_Text.setText("CHAT.GLOBAL>");
      notcMessageHud_Edit.setPosition(126, 2);
      notcMessageHud_Edit.setExtent("367 24");
      notcMessageHud_Edit.setText(notcMessageHud_Edit.zCurrentMsg);
   }

   Canvas.pushDialog(%this);

   //%this.setVisible(true);
   deactivateKeyboard();
   notcMessageHud_Edit.makeFirstResponder(true);
}

//------------------------------------------------------------------------------

function MessageHud::close(%this)
{
   if(%this.isTeamMsg)
      notcMessageHud_Edit.zCurrentTeamMsg = notcMessageHud_Edit.getText();
   else
      notcMessageHud_Edit.zCurrentMsg = notcMessageHud_Edit.getText();

   Canvas.popDialog(%this);
   //%this.setVisible(false);
   if ( $enableDirectInput )
      activateKeyboard();
}


//------------------------------------------------------------------------------

function MessageHud::toggleState(%this)
{
   if(%this.isAwake())
      %this.close();
   else
      %this.open();
}

//------------------------------------------------------------------------------

function notcMessageHud_Edit::onEscape(%this)
{
   MessageHud.close();
}

//------------------------------------------------------------------------------

function notcMessageHud_Edit::eval(%this)
{
   %text = collapseEscape(trim(%this.getValue()));

   if(%text !$= "")
   {
      if(MessageHud.isTeamMsg)
         commandToServer('teamMessageSent', %text);
      else
         commandToServer('messageSent', %text);
   }
   
   notcMessageHud_Edit.setValue("");

   MessageHud.close();
}


//----------------------------------------------------------------------------
// MessageHud key handlers

function toggleMessageHud(%make)
{
   if(%make)
   {
      MessageHud.isTeamMsg = false;
      MessageHud.toggleState();
   }
}

function teamMessageHud(%make)
{
   if(%make)
   {
      MessageHud.isTeamMsg = true;
      MessageHud.toggleState();
   }
}


