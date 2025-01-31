// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

function EVisibility::onWake( %this )
{
   // Create the array if it
   // doesn't already exist.
   if ( !isObject( %this.array ) )
      %this.array = new ArrayObject();

   // Create the array if it
   // doesn't already exist.
   if ( !isObject( %this.classArray ) )
   {
      %this.classArray = new ArrayObject();
      %this.addClassOptions();   
   }

   %this.updateOptions();

}

function EVisibility::updateOptions( %this )
{
   // First clear the stack control.
   %this-->theVisOptionsList.clear();   
    
   // Go through all the
   // parameters in our array and
   // create a check box for each.
   for ( %i = 0; %i < %this.array.count(); %i++ )
   {
      %text = "  " @ %this.array.getValue( %i );
      %val = %this.array.getKey( %i );
      %var = getWord( %val, 0 );
      %toggleFunction = getWord( %val, 1 );         
      
      %textLength = strlen( %text );
      
      %cmd = "";
      if ( %toggleFunction !$= "" )
         %cmd = %toggleFunction @ "( $thisControl.getValue() );";      
      
      %checkBox = new GuiCheckBoxCtrl()
      {
         canSaveDynamicFields = "0";
         isContainer = "0";
         Profile = "ToolsGuiCheckBoxListProfile";
         HorizSizing = "right";
         VertSizing = "bottom";
         Position = "0 0";
         Extent = (%textLength * 4) @ " 18";
         MinExtent = "8 2";
         canSave = "1";
         Visible = "1";
         Variable = %var;
         tooltipprofile = "ToolsGuiToolTipProfile";
         hovertime = "1000";
         text = %text;
         groupNum = "-1";
         buttonType = "ToggleButton";
         useMouseEvents = "0";
         useInactiveState = "0";
         Command = %cmd;
      };

      %this-->theVisOptionsList.addGuiControl( %checkBox );
   }   
}

function EVisibility::addOption( %this, %text, %varName, %toggleFunction )
{
   // Create the array if it
   // doesn't already exist.
   if ( !isObject( %this.array ) )
      %this.array = new ArrayObject();   
   
   %this.array.push_back( %varName @ " " @ %toggleFunction, %text );
   %this.array.uniqueKey();  
   %this.array.sortd(); 
   %this.updateOptions();
}

function EVisibility::addClassOptions( %this )
{
   %visList = %this-->theClassVisList;
   %selList = %this-->theClassSelList;
   
   // First clear the stack control.
   
   %visList.clear();
   %selList.clear();

   %classList = enumerateConsoleClasses( "SceneObject" );
   %classCount = getFieldCount( %classList );
   
   for ( %i = 0; %i < %classCount; %i++ )
   {
      %className = getField( %classList, %i );
      %this.classArray.push_back( %className );
   }
   
   // Remove duplicates and sort by key.
   %this.classArray.uniqueKey();
   %this.classArray.sortkd();
   
   // Go through all the
   // parameters in our array and
   // create a check box for each.
   for ( %i = 0; %i < %this.classArray.count(); %i++ )
   {
      %class = %this.classArray.getKey( %i );
      
      %visVar = "$" @ %class @ "::isRenderable";
      %selVar = "$" @ %class @ "::isSelectable";
      
      %textLength = strlen( %class );
      %text = "  " @ %class;
      
      // Add visibility toggle.
      
      %visCheckBox = new GuiCheckBoxCtrl()
      {
         canSaveDynamicFields = "0";
         isContainer = "0";
         Profile = "ToolsGuiCheckBoxListFlipedProfile";
         HorizSizing = "right";
         VertSizing = "bottom";
         Position = "0 0";
         Extent = (%textLength * 4) @ " 18";
         MinExtent = "8 2";
         canSave = "1";
         Visible = "1";
         Variable = %visVar;
         tooltipprofile = "ToolsGuiToolTipProfile";
         hovertime = "1000";
         tooltip = "Show/hide all " @ %class @ " objects.";
         text = %text;
         groupNum = "-1";
         buttonType = "ToggleButton";
         useMouseEvents = "0";
         useInactiveState = "0";
      };

      %visList.addGuiControl( %visCheckBox );

      // Add selectability toggle.
      
      %selCheckBox = new GuiCheckBoxCtrl()
      {
         canSaveDynamicFields = "0";
         isContainer = "0";
         Profile = "ToolsGuiCheckBoxListFlipedProfile";
         HorizSizing = "right";
         VertSizing = "bottom";
         Position = "0 0";
         Extent = (%textLength * 4) @ " 18";
         MinExtent = "8 2";
         canSave = "1";
         Visible = "1";
         Variable = %selVar;
         tooltipprofile = "ToolsGuiToolTipProfile";
         hovertime = "1000";
         tooltip = "Enable/disable selection of all " @ %class @ " objects.";
         text = %text;
         groupNum = "-1";
         buttonType = "ToggleButton";
         useMouseEvents = "0";
         useInactiveState = "0";
      };

      %selList.addGuiControl( %selCheckBox );
   }
}
