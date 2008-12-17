/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details (www.gnu.org/licenses)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_net.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#define IP_ADDRESS	"192.168.0.100"
#define MTU		2048
#define TIMEOUT	500			// if larger pictures than 320x240 is required you should increase this
#define FONT	"/usr/share/fonts/truetype/freefont/FreeSans.ttf"
#define FONT_SIZE	14
#define CONSOLE		1
#define CMD			2
#define DISPLAY		3

// Function declarations
void recieve_map(TCPsocket tcpsock, SDLNet_SocketSet socSet, SDL_Surface *screen);
void sendCmd(TCPsocket tcpsock, SDLNet_SocketSet socSet, SDL_Surface *screen);
static inline paintPixel(SDL_Surface *screen, unsigned int x, unsigned int y, unsigned char R, unsigned char G, unsigned char B);
void clrCmd(SDL_Surface *screen, unsigned char area);

int main(int argc, char* argv[])
{
    FILE *fp;
    int archive_flag = 0;
    int framecount = 0;
    char filename[20];
    int index = 0;
    int i1, size;
    int result, ready, timeout;
    int imageReady = 0;
    char imageBuf[MTU*10];
    char buf[MTU], *cp;
    char msg[2] = {'I', 0};
    char msg1[5] = { 'M', 0x01, 0x01, 0x01, 0};
    IPaddress ipaddress;
    TCPsocket tcpsock;
    SDLNet_SocketSet socSet;
    unsigned int flags = SDL_DOUBLEBUF|SDL_HWSURFACE;
    SDL_Surface *screen;
    SDL_Surface *image;
    unsigned int nrSocketsReady;

    // initialize SDL and SDL_net
    if((SDL_Init(SDL_INIT_VIDEO) == -1)) { 
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        return 1;
    }
    if(SDLNet_Init() == -1) {
        printf("SDLNet_Init: %s\n", SDLNet_GetError());
        return 2;
    }
    if(TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        return 3;
    }
    // initialize the screen
    screen = SDL_SetVideoMode(320, 260, 32, flags);
    SDLNet_ResolveHost(&ipaddress, IP_ADDRESS, 10001);
    tcpsock = SDLNet_TCP_Open(&ipaddress);
    if(!tcpsock) {
        printf("SDLNet_TCP_Open 1: %s\n", SDLNet_GetError());
        return 2;
    }
    socSet = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(socSet, tcpsock);
    msg1[0] = msg1[1] = msg1[2] = msg1[3] = 0;
    
    SDL_Event event;
    int quit = 0;

    imageReady = 0;
    timeout = 0;

    // main loop
    for (; !quit;) {
        // check keyboard events
        *msg1 = 0;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
				case SDL_MOUSEBUTTONUP:
					if(event.button.button == SDL_BUTTON_LEFT){
						printf("X: %d Y: %d\n", event.button.x, event.button.y);
					}
					break;
				case SDL_KEYDOWN:
					switch( event.key.keysym.sym ) {
						case SDLK_ESCAPE:
							quit = 1;
							break;
						case SDLK_a:
							archive_flag = 1;
							framecount = 0;
							printf(" archiving enabled - files saved in ./archives/ directory\n");
							break;
						case SDLK_RETURN:
							printf("Entering input mode...\n");
							sendCmd(tcpsock,socSet, screen);
							break;
					}
					break;
			}
        }

        index = 0;
        imageBuf[0] = 0;
        imageReady = 0;

		// send 'I' command
		result = SDLNet_TCP_Send(tcpsock, msg, 1);
		if(result < 1)
			printf("SDLNet_TCP_Send 1: %s\n", SDLNet_GetError());

		// look for frames
		while (!imageReady) {	
			if (!imageReady) {
				nrSocketsReady = SDLNet_CheckSockets(socSet, TIMEOUT);
				if (nrSocketsReady == -1) {
					printf("SDLNet_CheckSockets: %s\n", SDLNet_GetError());
					perror("SDLNet_CheckSockets");
				}
				else if (nrSocketsReady > 0) {
					if (SDLNet_CheckSockets(socSet, TIMEOUT)) {
						if (SDLNet_SocketReady(socSet)) {
							result = SDLNet_TCP_Recv(tcpsock, buf, MTU);
							memcpy(imageBuf+index, buf, result);
							index += result;
							if ((buf[result-2] == -1) && (buf[result-1] == -39))
								imageReady = 1;
						}
					}
				}
				else {
					printf("\n\rNo sockets ready.\n\r");
					break;
				}
			}
		}

		// make certain that captured frames are valid
		if (!imageReady || imageBuf[0]!='#' || imageBuf[1]!='#' || imageBuf[2]!='I') {
			imageReady = 0;
			printf("bad image, or Checksockets() timed out!\n");
			continue;
		}
		size = (unsigned int)imageBuf[6] + (unsigned int)imageBuf[7]*256 + (unsigned int)imageBuf[8]*65536;
		if (size > index-10) {
			printf("bad image size:  %d %d %d\n", size, index, framecount);
			imageReady = 0;
			continue;
		}
		imageReady = 0;
		timeout = 0;
		SDL_RWops *rwop;
		rwop = SDL_RWFromMem(&(imageBuf[10]), index-10);
		image  = IMG_LoadJPG_RW(rwop);
		cp = (char *)image->pixels;

		if (archive_flag) {
			sprintf(filename, "archives/%.4d.ppm", framecount);
			fp = fopen(filename, "w");
			fprintf(fp,"P6\n320\n240\n255\n", 15);
			fwrite(cp, 1, 230400, fp);
			fclose(fp);
		}
		framecount++;
		SDL_BlitSurface(image, NULL, screen, NULL);
		SDL_Flip(screen);
		SDL_FreeRW(rwop);
		SDL_FreeSurface(image);
	}

	TTF_Quit();
    SDLNet_TCP_Close(tcpsock);
    SDLNet_Quit();
    SDL_Quit();
    return 0;
}

void sendCmd(TCPsocket tcpsock, SDLNet_SocketSet socSet, SDL_Surface *screen)
{
	int i, result;
	unsigned char cmd[30];			// Buffer to hold the input string
	unsigned char temp[31];			// temporary buffer used for printing (one extra char for the '_' -char)
	unsigned char buffer[MTU];
	SDL_Surface *text = NULL;
	SDL_Color textColor = {0,255,0,0};
	SDL_Color bgColor = {0,0,0,0};
	unsigned char len = 0;			// the length of the current command
	unsigned char done = 0, exit = 0;	// flags
	TTF_Font *font = NULL;
	SDL_Event event;
	SDL_Rect cmdPos = {16,240,320,20};
	SDL_Rect consolePos = {0,240,320,20};
	signed int nrSocketsReady;
	
	// prepare command buffers
	for (i=0;i<30;i++) {
		cmd[i] = '\0';
		temp[i] = '\0';
	}
	temp[30] = '\0';
	
	// console chars
	temp[0] = '#';
	temp[1] = '>';
	temp[2] = '_';
	
	//setup the text param's and render console chars to "screen"
	SDL_EnableUNICODE(SDL_ENABLE);
	font = TTF_OpenFont(FONT, FONT_SIZE);
	if(!font) {
		printf("TTF_OpenFont: %s\n", TTF_GetError());
	}
	text = TTF_RenderText_Shaded(font, temp, textColor, bgColor);
	if (text == NULL) {
		printf("TTF_OpenFont: %s\n", TTF_GetError());
	}
	if (SDL_BlitSurface(text,NULL, screen, &consolePos) != 0) {
		printf("TTF_OpenFont: %s\n", TTF_GetError());
	}
	SDL_UpdateRect(screen, 0, 0, 0, 0);
	SDL_FreeSurface(text);		
	
	strcpy(temp,cmd);
	
	while((!done) && (!exit)) {
		while(SDL_PollEvent(&event)) {
			//If a key was pressed
			if( event.type == SDL_KEYDOWN ) {
				
				strcpy(temp,cmd);			// reset temp
				len = 0;
				do {						// Find the length of the current command
					if (cmd[len] != '\0')
						len++;
					else
						break;
				} while(len < 29);	// save the last '\0' char
				
				if (event.key.keysym.sym == SDLK_RETURN) {
					done = 1;					// flag the command as complete, exit loop
				}
				//If backspace was pressed and the string isn't blank
				else if((event.key.keysym.sym == SDLK_BACKSPACE ) && (len > 0)) {
					cmd[len - 1] = '\0';		// Remove a character from the end
				}
				else if (event.key.keysym.sym == SDLK_ESCAPE) {
					exit = 1;					// exits loop
				}
				else if ((len < 29) && (event.key.keysym.unicode >= (unsigned short)' ') && (event.key.keysym.unicode <= (unsigned short)'}')) {
					cmd[len] = (char)event.key.keysym.unicode;		//Append the character to the command string
				}
				else if (len >= 29) {
					printf("command too long!\n\r");
				}
				
				if (strcmp(cmd,temp) != 0) {
					clrCmd(screen, CMD);
					
					memset(temp,'\0',30);
					strcpy(temp,cmd);
					len = 0;
					do {
						if (cmd[len] != '\0')
							len++;
						else
							break;
					} while(len < 29);			// save place for the '_' and '\0' char
					
					temp[len] = '_';			// add an underscore (just because it looks good)
					text = TTF_RenderText_Shaded(font, temp, textColor, bgColor);	// Render a new text surface
					SDL_BlitSurface(text,NULL, screen, &cmdPos);		// put the text onto the "screen"
					SDL_UpdateRect(screen, 0, 0, 0, 0);					// update the text area
					SDL_FreeSurface(text);								// Free the surface in memory
				}
			}
		}
	}
	
	if (!exit) {
		printf("entered command: \"%s\"\n\r",cmd);	// prints the command send to the SRV
		
		len = 0;
		do {
			if ((cmd[len] != ' ') && (cmd[len] != '\0'))
				len++;
			else
				break;
		} while(len < 30);
		
		if (len > 0) {
			// Send command to SRV-1
			result = SDLNet_TCP_Send(tcpsock, cmd, len);
			if(result < 1)
				printf("SDLNet_TCP_Send 1: %s\n", SDLNet_GetError());
			
			for (i=0;i<5;i++) {				// try to recieve an answer within 5 attempts
				nrSocketsReady = SDLNet_CheckSockets(socSet, TIMEOUT);
				if (nrSocketsReady > 0) {
					if (SDLNet_SocketReady(socSet))	{
						result = SDLNet_TCP_Recv(tcpsock, buffer, MTU);
						if (result <= 0) {
							printf("no reply, or reply reception failed\n\r");
						}
						break;
					}
				}
				else if (nrSocketsReady == -1) {
					printf("SDLNet_CheckSockets: %s\n", SDLNet_GetError());
					perror("SDLNet_CheckSockets");
				}
				else
					printf("reception attempt #%i failed\n\r",i+1);
			}
			
			if (result > 0) {
				buffer[result] = '\0';
				printf("Received reply: %s\n\r",buffer);
			}
		}
	}
	//erase command line
	clrCmd(screen, DISPLAY);

	// Clean up
	SDL_EnableUNICODE(SDL_DISABLE);
	TTF_CloseFont(font);
}

void clrCmd(SDL_Surface *screen, unsigned char area)
{
	unsigned int color = SDL_MapRGB(screen->format, 0, 0, 0);
	SDL_Rect console = {0,240,320,20};
	SDL_Rect cmd = {16,240,320,20};
	if (area == CONSOLE)			// clear entire console area
		SDL_FillRect(screen, &console, color);
	else if (area == CMD)			// clears only the current command
		SDL_FillRect(screen, &cmd, color);
	else if (area == DISPLAY)		// clears the entire display (good when changing camera resolution)
		SDL_FillRect(screen, NULL, color);
	else
		printf("unknown area to clear...\n\r");
	
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}
