

#pragma once




#include <windows.h>

#include <iostream>
#include <chrono>
#include <vector>
#include <list>
#include <thread>
#include <atomic>
#include <condition_variable>

enum COLOUR
{
	FG_BLACK		= 0x0000,
	FG_DARK_BLUE    = 0x0001,	
	FG_DARK_RED     = 0x0004,
	FG_GREY			= 0x0007,
	FG_DARK_GREY    = 0x0008,
	FG_BLUE			= 0x0009,
	FG_GREEN		= 0x000A,
	FG_RED			= 0x000C,
	FG_YELLOW		= 0x000E,
	FG_WHITE		= 0x000F,
	BG_BLACK		= 0x0000,
	BG_DARK_RED		= 0x0040,
	BG_RED			= 0x00C0,
	BG_WHITE		= 0x00F0,
};

enum PIXEL_TYPE
{
	PIXEL_SOLID = 0x2588,
	PIXEL_THREEQUARTERS = 0x2593,
	PIXEL_HALF = 0x2592,
	PIXEL_QUARTER = 0x2591,
};

class Glyphs
{
public:
	Glyphs()
	{

	}

	Glyphs(int w, int h)
	{
		Create(w, h);
	}

	Glyphs(std::wstring sFile)
	{
		if (!Load(sFile))
			Create(8, 8);
	}

	int nWidth = 0;
	int nHeight = 0;

private:
	short *m_Glyphs = nullptr;
	short *m_Colours = nullptr;

	void Create(int w, int h)
	{
		nWidth = w;
		nHeight = h;
		m_Glyphs = new short[w*h];
		m_Colours = new short[w*h];
		for (int i = 0; i < w*h; i++)
		{
			m_Glyphs[i] = L' ';
			m_Colours[i] = FG_WHITE;
		}
	}

public:
	

	void SetColour(int x, int y, short c)
	{
		if (x <0 || x >= nWidth || y < 0 || y >= nHeight)
			return;
		else
			m_Colours[y * nWidth + x] = c;
	}

	

	short GetColour(int x, int y)
	{
		if (x <0 || x >= nWidth || y < 0 || y >= nHeight)
			return FG_BLACK;
		else
			return m_Colours[y * nWidth + x];
	}

	
	short SampleColour(float x, float y)
	{
		int sx = (int)(x * (float)nWidth);
		int sy = (int)(y * (float)nHeight-1.0f);
		if (sx <0 || sx >= nWidth || sy < 0 || sy >= nHeight)
			return FG_BLACK;
		else
			return m_Colours[sy * nWidth + sx];
	}

	

	bool Load(std::wstring sFile)
	{
		delete[] m_Glyphs;
		delete[] m_Colours;
		nWidth = 0;
		nHeight = 0;

		FILE *f = nullptr;
		_wfopen_s(&f, sFile.c_str(), L"rb");
		if (f == nullptr)
			return false;

		std::fread(&nWidth, sizeof(int), 1, f);
		std::fread(&nHeight, sizeof(int), 1, f);

		Create(nWidth, nHeight);

		std::fread(m_Colours, sizeof(short), nWidth * nHeight, f);
		std::fread(m_Glyphs, sizeof(short), nWidth * nHeight, f);

		std::fclose(f);
		return true;
	}
};

class GridEngine
{
public:
	GridEngine()
	{
		m_nScreenWidth = 80;
		m_nScreenHeight = 30;

		m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		m_hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

		std::memset(m_keyNewState, 0, 256 * sizeof(short));
		std::memset(m_keyOldState, 0, 256 * sizeof(short));
		std::memset(m_keys, 0, 256 * sizeof(sKeyState));
		m_mousePosX = 0;
		m_mousePosY = 0;

		m_bEnableSound = false;

		m_sAppName = L"Default";
	}


	int ConstructConsole(int width, int height, int fontw, int fonth)
	{
		if (m_hConsole == INVALID_HANDLE_VALUE)
			return Error(L"Bad Handle");

		m_nScreenWidth = width;
		m_nScreenHeight = height;

		// Change console visual size to a minimum so ScreenBuffer can shrink
		// below the actual visual size
		m_rectWindow = { 0, 0, 1, 1 };
		SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow);

		// Set the size of the screen buffer
		COORD coord = { (short)m_nScreenWidth, (short)m_nScreenHeight };
		if (!SetConsoleScreenBufferSize(m_hConsole, coord))
			Error(L"SetConsoleScreenBufferSize");

		// Assign screen buffer to the console
		if (!SetConsoleActiveScreenBuffer(m_hConsole))
			return Error(L"SetConsoleActiveScreenBuffer");
		
		// Set the font size now that the screen buffer has been assigned to the console
		CONSOLE_FONT_INFOEX cfi;
		cfi.cbSize = sizeof(cfi);
		cfi.nFont = 0;
		cfi.dwFontSize.X = fontw;
		cfi.dwFontSize.Y = fonth;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;


		wcscpy_s(cfi.FaceName, L"Consolas");
		if (!SetCurrentConsoleFontEx(m_hConsole, false, &cfi))
			return Error(L"SetCurrentConsoleFontEx");

		// Get screen buffer info and check the maximum allowed window size. Return
		// error if exceeded, so user knows their dimensions/fontsize are too large
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (!GetConsoleScreenBufferInfo(m_hConsole, &csbi))
			return Error(L"GetConsoleScreenBufferInfo");
		if (m_nScreenHeight > csbi.dwMaximumWindowSize.Y)
			return Error(L"Screen Height / Font Height Too Big");
		if (m_nScreenWidth > csbi.dwMaximumWindowSize.X)
			return Error(L"Screen Width / Font Width Too Big");

		// Set Physical Console Window Size
		m_rectWindow = { 0, 0, (short)m_nScreenWidth - 1, (short)m_nScreenHeight - 1 };
		if (!SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow))
			return Error(L"SetConsoleWindowInfo");

		// Set flags to allow mouse input		
		if (!SetConsoleMode(m_hConsoleIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT))
			return Error(L"SetConsoleMode");

		// Allocate memory for screen buffer
		m_bufScreen = new CHAR_INFO[m_nScreenWidth*m_nScreenHeight];
		memset(m_bufScreen, 0, sizeof(CHAR_INFO) * m_nScreenWidth * m_nScreenHeight);

		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CloseHandler, TRUE);
		return 1;
	}

	virtual void Draw(int x, int y, short c = 0x2588, short col = 0x000F)
	{
		if (x >= 0 && x < m_nScreenWidth && y >= 0 && y < m_nScreenHeight)
		{
			m_bufScreen[y * m_nScreenWidth + x].Char.UnicodeChar = c;
			m_bufScreen[y * m_nScreenWidth + x].Attributes = col;
		}
	}

	void Fill(int x1, int y1, int x2, int y2, short c = 0x2588, short col = 0x000F)
	{
		Clip(x1, y1);
		Clip(x2, y2);
		for (int x = x1; x < x2; x++)
			for (int y = y1; y < y2; y++)
				Draw(x, y, c, col);
	}

	
	void Clip(int &x, int &y)
	{
		if (x < 0) x = 0;
		if (x >= m_nScreenWidth) x = m_nScreenWidth;
		if (y < 0) y = 0;
		if (y >= m_nScreenHeight) y = m_nScreenHeight;
	}

	void DrawLine(int x1, int y1, int x2, int y2, short c = 0x2588, short col = 0x000F)
	{
		int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
		dx = x2 - x1; dy = y2 - y1;
		dx1 = abs(dx); dy1 = abs(dy);
		px = 2 * dy1 - dx1;	py = 2 * dx1 - dy1;
		if (dy1 <= dx1)
		{
			if (dx >= 0)
				{ x = x1; y = y1; xe = x2; }
			else
				{ x = x2; y = y2; xe = x1;}

			Draw(x, y, c, col);
			
			for (i = 0; x<xe; i++)
			{
				x = x + 1;
				if (px<0)
					px = px + 2 * dy1;
				else
				{
					if ((dx<0 && dy<0) || (dx>0 && dy>0)) y = y + 1; else y = y - 1;
					px = px + 2 * (dy1 - dx1);
				}
				Draw(x, y, c, col);
			}
		}
		else
		{
			if (dy >= 0)
				{ x = x1; y = y1; ye = y2; }
			else
				{ x = x2; y = y2; ye = y1; }

			Draw(x, y, c, col);

			for (i = 0; y<ye; i++)
			{
				y = y + 1;
				if (py <= 0)
					py = py + 2 * dx1;
				else
				{
					if ((dx<0 && dy<0) || (dx>0 && dy>0)) x = x + 1; else x = x - 1;
					py = py + 2 * (dx1 - dy1);
				}
				Draw(x, y, c, col);
			}
		}
	}


	
	
	~GridEngine()
	{
		SetConsoleActiveScreenBuffer(m_hOriginalConsole);
		delete[] m_bufScreen;
	}

public:
	void Start()
	{	
		// Start the thread
		m_bAtomActive = true;
		std::thread t = std::thread(&GridEngine::GridThread, this);

		// Wait for thread to be exited
		t.join();
	}

	int ScreenWidth()
	{
		return m_nScreenWidth;
	}

	int ScreenHeight() 
	{
		return m_nScreenHeight;
	}

private:
	void GridThread()
	{
		// Create user resources as part of this thread
		if (!OnUserCreate()) 
			m_bAtomActive = false;

		// Check if sound system should be enabled
		
		auto tp1 = std::chrono::system_clock::now();
		auto tp2 = std::chrono::system_clock::now();

		while (m_bAtomActive)
		{
			// Run as fast as possible
			while (m_bAtomActive)
			{
				// Handle Timing
				tp2 = std::chrono::system_clock::now();
				std::chrono::duration<float> elapsedTime = tp2 - tp1;
				tp1 = tp2;
				float fElapsedTime = elapsedTime.count();

				

				// Handle Mouse Input - Check for window events
				INPUT_RECORD inBuf[32];
				DWORD events = 0;
				GetNumberOfConsoleInputEvents(m_hConsoleIn, &events);
				if (events > 0)
					ReadConsoleInput(m_hConsoleIn, inBuf, events, &events);

				// Handle events - we only care about mouse clicks and movement
				// for now
				for (DWORD i = 0; i < events; i++)
				{
					switch (inBuf[i].EventType)
					{
					case FOCUS_EVENT:
					{
						m_bConsoleInFocus = inBuf[i].Event.FocusEvent.bSetFocus;
					}
					break;

					case MOUSE_EVENT:
					{
						switch (inBuf[i].Event.MouseEvent.dwEventFlags)
						{
						case MOUSE_MOVED:
						{
							m_mousePosX = inBuf[i].Event.MouseEvent.dwMousePosition.X;
							m_mousePosY = inBuf[i].Event.MouseEvent.dwMousePosition.Y;
						}
						break;

						case 0:
						{
							for (int m = 0; m < 5; m++)
								m_mouseNewState[m] = (inBuf[i].Event.MouseEvent.dwButtonState & (1 << m)) > 0;

						}
						break;

						default:
							break;
						}
					}
					break;

					default:
						break;
						// We don't care just at the moment
					}
				}

				for (int m = 0; m < 5; m++)
				{
					m_mouse[m].bPressed = false;
					m_mouse[m].bReleased = false;

					if (m_mouseNewState[m] != m_mouseOldState[m])
					{
						if (m_mouseNewState[m])
						{
							m_mouse[m].bPressed = true;
							m_mouse[m].bHeld = true;
						}
						else
						{
							m_mouse[m].bReleased = true;
							m_mouse[m].bHeld = false;
						}
					}

					m_mouseOldState[m] = m_mouseNewState[m];
				}


				// Handle Frame Update
				if (!OnUserUpdate(fElapsedTime))
					m_bAtomActive = false;

				// Update Title & Present Screen Buffer
				wchar_t s[256];
				swprintf_s(s, 256, L"Pathfinder - %s - FPS: %3.2f", m_sAppName.c_str(), 1.0f / fElapsedTime);
				SetConsoleTitle(s);
				WriteConsoleOutput(m_hConsole, m_bufScreen, { (short)m_nScreenWidth, (short)m_nScreenHeight }, { 0,0 }, &m_rectWindow);
			}

		

			// Allow the user to free resources if they have overrided the destroy function
			if (OnUserDestroy())
			{
				// User has permitted destroy, so exit and clean up
				delete[] m_bufScreen;
				SetConsoleActiveScreenBuffer(m_hOriginalConsole);
				m_cvGameFinished.notify_one();
			}
			else
			{
				// User denied destroy for some reason, so continue running
				m_bAtomActive = true;
			}
		}
	}

public:
	// User MUST OVERRIDE THESE!!
	virtual bool OnUserCreate()							= 0;
	virtual bool OnUserUpdate(float fElapsedTime)		= 0;	

	// Optional for clean up 
	virtual bool OnUserDestroy()						{ return true; }




	

	

	
	
	

	unsigned int m_nSampleRate;
	unsigned int m_nChannels;
	unsigned int m_nBlockCount;
	unsigned int m_nBlockSamples;
	unsigned int m_nBlockCurrent;

	short* m_pBlockMemory = nullptr;
	WAVEHDR *m_pWaveHeaders = nullptr;
	HWAVEOUT m_hwDevice = nullptr;

	std::thread m_AudioThread;
	std::atomic<bool> m_bAudioThreadActive = false;
	std::atomic<unsigned int> m_nBlockFree = 0;
	std::condition_variable m_cvBlockNotZero;
	std::mutex m_muxBlockNotZero;
	std::atomic<float> m_fGlobalTime = 0.0f;

	

protected:
	

	struct sKeyState
	{
		bool bPressed;
		bool bReleased;
		bool bHeld;
	} m_keys[256], m_mouse[5];

	int m_mousePosX;
	int m_mousePosY;

public:
	sKeyState GetKey(int nKeyID){ return m_keys[nKeyID]; }
	int GetMouseX() { return m_mousePosX; }
	int GetMouseY() { return m_mousePosY; }
	sKeyState GetMouse(int nMouseButtonID) { return m_mouse[nMouseButtonID]; }
	bool IsFocused() { return m_bConsoleInFocus; }


protected:
	int Error(const wchar_t *msg)
	{
		wchar_t buf[256];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
		SetConsoleActiveScreenBuffer(m_hOriginalConsole);
		wprintf(L"ERROR: %s\n\t%s\n", msg, buf);
		return 0;
	}

	static BOOL CloseHandler(DWORD evt)
	{
		// Note this gets called in a seperate OS thread, so it must
		// only exit when the game has finished cleaning up, or else
		// the process will be killed before OnUserDestroy() has finished
		if (evt == CTRL_CLOSE_EVENT)
		{
			m_bAtomActive = false;

			// Wait for thread to be exited
			std::unique_lock<std::mutex> ul(m_muxGame);
			m_cvGameFinished.wait(ul);
		}
		return true;
	}

protected:
	int m_nScreenWidth;
	int m_nScreenHeight;
	CHAR_INFO *m_bufScreen;
	std::wstring m_sAppName;
	HANDLE m_hOriginalConsole;
	CONSOLE_SCREEN_BUFFER_INFO m_OriginalConsoleInfo;
	HANDLE m_hConsole;
	HANDLE m_hConsoleIn;
	SMALL_RECT m_rectWindow;
	short m_keyOldState[256] = { 0 };
	short m_keyNewState[256] = { 0 };
	bool m_mouseOldState[5] = { 0 };
	bool m_mouseNewState[5] = { 0 };
	bool m_bConsoleInFocus = true;	
	bool m_bEnableSound = false;

	// These need to be static because of the OnDestroy call the OS may make. The OS
	// spawns a special thread just for that
	static std::atomic<bool> m_bAtomActive;
	static std::condition_variable m_cvGameFinished;
	static std::mutex m_muxGame;
};

// Define our static variables
std::atomic<bool> GridEngine::m_bAtomActive(false);
std::condition_variable GridEngine::m_cvGameFinished;
std::mutex GridEngine::m_muxGame;
