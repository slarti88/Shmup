/*
	This file is part of SHMUP.

    SHMUP is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SHMUP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/    
#include <windows.h>
#include <string.h>
#include <strsafe.h>

extern "C" {
#include "../src/dEngine.h"
#include "../src/commands.h"
#include "../src/timer.h"
#include "../src/menu.h"
#include "../src/io_interface.h"
}


#include "win32EGL.h"





void WIN_CheckError(char* errorHeader){

	DWORD errorCode;
	LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;


	errorCode = GetLastError();

	
	FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
 

	
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,(lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)errorHeader)+40)*sizeof(TCHAR)); 
   
	
	StringCchPrintf((LPTSTR)lpDisplayBuf, 
	                  LocalSize(lpDisplayBuf) / sizeof(TCHAR),
					  TEXT("%s failed with error %d: %s"),
					  errorHeader, 
					  errorCode, 
					  lpMsgBuf); 
	

	Log_Printf("'%s'\n",(char*)lpDisplayBuf);
	

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);

}

#define KEYDOWN(vk_code)  ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)
#define KEYUP(vk_code)  ((GetAsyncKeyState(vk_code) & 0x8000) ? 0 : 1)

#define MOUSE_L_BUTTON 0
#define MOUSE_R_BUTTON 1
int buttonState[2];
int lastPosition[2];
void WIN_ReadInputs(){

	io_event_s event;

	int buttonIsPressed = KEYDOWN(VK_LBUTTON);
	static int buttonWasPressed = 0;


	//Get the mouse coordinates in screenspace.
	CURSORINFO pci ;
	pci.cbSize = sizeof(pci);
	BOOL success = GetCursorInfo(&pci);

	if (!success){
		WIN_CheckError("GetCursorInfo");
		return;
	}

	//Convert the screenspage to windowspace coordinates.
	success = ScreenToClient(WIN_GetHWND(),&pci.ptScreenPos);
	if (!success){
		WIN_CheckError("ScreenToClient");
		return;
	}


	if (KEYDOWN(VK_ESCAPE) && engine.requiredSceneId != 0 && engine.sceneId != 0){
		MENU_Set(MENU_HOME);
		engine.requiredSceneId=0;
	}


	if (buttonIsPressed)
	{
		

		if (!buttonWasPressed)
		{
			//This is a began event
			event.type = IO_EVENT_BEGAN;
			event.position[X] = pci.ptScreenPos.x;
			event.position[Y] = pci.ptScreenPos.y;

			

			//Log_Printf("Click: [%d,%d].\n",event.position[X],event.position[Y]);
			IO_PushEvent(&event);
			
		}
		else
		{
			//This is maybe a moved event.
			if (pci.ptScreenPos.x != lastPosition[X] ||
				pci.ptScreenPos.y != lastPosition[Y]
				)
				{
					event.type = IO_EVENT_MOVED;
					event.position[X] = pci.ptScreenPos.x;
					event.position[Y] = pci.ptScreenPos.y;
					event.previousPosition[X] = lastPosition[X];
					event.previousPosition[Y] = lastPosition[Y];
					IO_PushEvent(&event);
				}
		}

		lastPosition[X] = pci.ptScreenPos.x;
		lastPosition[Y] = pci.ptScreenPos.y;
		buttonWasPressed = 1;
	}
	else
	{
		if (buttonWasPressed)
		{
			//This is an end event
			event.type = IO_EVENT_ENDED;
			IO_PushEvent(&event);
		}
		buttonWasPressed = 0;
	}


    


	//Log_Printf("wc: %d,%d\n",pci.ptScreenPos.x,pci.ptScreenPos.y);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	uchar engineParameters = 0;
	char cwd[256];
	WORD nBufferLength=256;
	char lpBuffer[256];

	Create_NativeWindow(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

	//Create a console so we can see outputs.
	AllocConsole();
	freopen("conin$","r",stdin);
	freopen("conout$","w",stdout);
	freopen("conout$","w",stderr);
	HWND  consoleHandle = GetConsoleWindow();
	MoveWindow(consoleHandle,1,1,680,480,1);
	printf("[main.cpp] Console initialized.\n");




	GetCurrentDirectoryA(nBufferLength,lpBuffer);
	memset(cwd,0,256);
	strcat(cwd,"RD=");
	strcat(cwd,lpBuffer);
	_putenv(cwd);

	memset(cwd,0,256);
	strcat(cwd,"WD=");
	strcat(cwd,lpBuffer);
	_putenv(cwd);

	engineParameters |= GL_11_RENDERER ;
		
	renderer.statsEnabled = 0;
	renderer.materialQuality = MATERIAL_QUALITY_HIGH;

   
    renderer.glBuffersDimensions[WIDTH] = WIN32_WINDOWS_WIDTH;
	renderer.glBuffersDimensions[HEIGHT] = WIN32_WINDOWS_HEIGHT;
	

	gameOn = 1;

	

	dEngine_Init();
	renderer.statsEnabled = 0;
	engine.licenseType = LICENSE_FULL;
	//This is only for windows build. Uses the viewport
	IO_Init();

	dEngine_InitDisplaySystem(engineParameters);

	renderer.props |= PROP_FOG;	

	memset(buttonState,0,sizeof(buttonState));

	/*
          The only complicated thing here is the time to sleep. timediff returned by the engine is telling us how long the frame should last.
		  Since a PC can render a frame in 1-2ms we need to substract frame hosting duration to timediff (either 16 or 17ms) and sleep for this amount of time.
	*/
	while(gameOn)
	{
		PumpWindowsMessages();

		// Check the state of the mouse and its position, may generate a touch_t if the 
		// left button is pressed.
		unsigned long startFrame = timeGetTime();


		WIN_ReadInputs();
		dEngine_HostFrame();
		EGLSwapBuffers();

		unsigned long endFrame = timeGetTime();

		unsigned long timeForFrame = endFrame - startFrame;
		
		int timeToSleep = timediff - timeForFrame;

		//Log_Printf("timeToSleep=%d\n",timeToSleep);
		// Game is clocked at 60Hz (timediff will be either 16 or 17, this value
		// comes from timer.c).
		if (timeToSleep > 0)
			Sleep(timeToSleep);
	}


	Destroy_NativeWindow();
	return 0;
}