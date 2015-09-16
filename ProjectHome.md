A simple SDL-based console for the SRV-1 camera module (single camera) from Surveyor, with added support for sending commands directly through the console.

By pressing 'a' the archive feature is enabled.

By pressing RETURN, the console goes into "Input mode" which enables the user to issue a command to the SRV module. See "Surveyor Commands" in the links-list for more information about these commands. The reply from each command is displayed is printed to the linux console from where the console was launched.

The project uses the following SDL libraries:
  * SDL (main)
  * SDL\_net
  * SDL\_image
  * SDL\_ttf

all of these can be downloaded from libSDL's main webpage (see link).

You'll also need a True Type Font, I'm using the FreeSans.ttf font from the "freefont-ttf-20080912.tar.gz" -package, downloadable from http://ftp.gnu.org/gnu/freefont/

All you have to do before compiling is setting the up 2 defines, the path to the font, and the ip-address of your SRV.

Please report any error or comments in the Wiki-tab.

Good luck!