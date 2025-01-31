// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

function XaNotcSettings1_MuxerGui::setContent(%this, %ctrl)
{
   //echo("XaNotcSettings1_MuxerGui::setContent():" SPC %ctrl);

   if(%this.zContent $= %ctrl)
      return;
   
   // Clean content container.
   if(isObject(%this.zContent) && isObject(%this.zContentOriginalParent))
      %this.zContentOriginalParent.add(%this.zContent);
   %this-->ContentContainer.clear();
   
   %content = %ctrl;
   %contentOriginalParent = %ctrl.getParent();
   
   if(isObject(%content))
   {
      %this-->ContentContainer.setExtent(%content.getExtent());
      %this-->ContentContainer.add(%content);
      %content.setPosition(0, 0);
      %this.zContent = %content;
      %this.zContentOriginalParent = %contentOriginalParent;
   }
}

function XaNotcSettings1_MuxerGui::onWake(%this)
{
   //echo("XaNotcSettings1_MuxerGui::onWake()");
   
   if($InGuiEditor)
      return;

   if(%this-->ContentContainer.getCount() == 0)
      %this-->MiscButton.performClick();
}

