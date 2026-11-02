#include "pch.h"
#include "winmain.h"

#include <malloc.h>

#include "control.h"
#include "midi.h"
#include "options.h"
#include "pb.h"
#include "pinball.h"
#include "render.h"
#include "Sound.h"
#include "n3ds_graphics.h"
#include "n3ds_input.h"
#include "n3ds_camera.h"
#include "n3ds_ui.h"
#include "n3ds_initials.h"
#include "n3ds_menu.h"
#include "n3ds_font.h"
#include "n3ds_dirty.h"
#include "n3ds_tiles.h"
#include "n3ds_controls.h"
#include "high_score.h"
#include "proj.h"
#include "TBall.h"
#include "TPinballTable.h"

int winmain::bQuit = 0;
int winmain::activated;
int winmain::DispFrameRate = 0;
int winmain::DispGRhistory = 0;
int winmain::single_step = 0;
int winmain::last_mouse_x;
int winmain::last_mouse_y;
int winmain::mouse_down;
int winmain::no_time_loss;

std::string winmain::DatFileName;
bool winmain::ShowSpriteViewer = false;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
char *winmain::BasePath;
bool winmain::IsNew3DS = false;
std::string winmain::FpsDetails;
double winmain::UpdateToFrameRatio;
winmain::DurationMs winmain::TargetFrameTime;
optionsStruct &winmain::Options = options::Options;
PrintConsole *winmain::Console = nullptr;

// Bottom-screen footer (view name + menu icon) that opens the menu on tap.
static constexpr int MenuTapX = 203, MenuTapY = 186;

// Table row (0 = top) of the ball nearest the flippers, or -1 if none in play.
static float LowestBallRow()
{
	if (!pb::MainTable)
		return -1.0f;

	float row = -1.0f;
	for (auto ball : pb::MainTable->BallList)
	{
		if (!ball->ActiveFlag)
			continue;
		int pos[2];
		proj::xform_to_2d(&ball->Position, pos);
		if (pos[1] > row)
			row = static_cast<float>(pos[1]);
	}
	return row;
}

int winmain::WinMain(LPCSTR lpCmdLine)
{
	std::set_new_handler(memalloc_failure);

	// Enable high speed mode for New 3DS

	APT_CheckNew3DS(&IsNew3DS);
	osSetSpeedupEnable(IsNew3DS);

	// Initialize graphics
	gfxInitDefault();

	// Set the base path for PINBALL.DAT
#if _ROMFS
	romfsInit();
	BasePath = (char *)"romfs:/";
#else
	BasePath = (char *)"sdmc:/3ds/SpaceCadetPinball/";
#endif
	pinball::quickFlag = 0; // strstr(lpCmdLine, "-quick") != nullptr;
	DatFileName = options::get_string("Pinball Data", pinball::get_rc_string(168, 0));

	// Check for full tilt .dat file and switch to it automatically

	auto cadetFilePath = pinball::make_path_name("CADET.DAT");
	auto cadetDat = fopen(cadetFilePath.c_str(), "r");
	if (cadetDat)
	{
		fclose(cadetDat);
		DatFileName = "CADET.DAT";
		pb::FullTiltMode = true;
	}

	// PB init from message handler

	{
		options::init();
		if (!Sound::Init(Options.SoundChannels, Options.Sounds))
			Options.Sounds = false;

		if (!pinball::quickFlag && !midi::music_init())
			Options.Music = false;

		if (pb::init())
		{
			PrintMessage("\x1b[14;0H  Could not load game data:\n  %s\n  file is missing.", DatFileName.c_str());
		}
	}

	// Initialize 3D graphics and matrices

	n3ds_graphics::Initialize();

	n3ds_graphics::SetTopOrthoProjectionMatrix(0, GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.0f);
	n3ds_graphics::SetBottomOrthoProjectionMatrix(0, GSP_SCREEN_HEIGHT_BOTTOM, 0, GSP_SCREEN_WIDTH, 0.1f, 1.0f);

	// Texture data and create texture object

	// The game's screen lives in two GPU textures in linear memory, used
	// alternately: the CPU fills one while the GPU may still be reading the
	// other. Only 8x8 tiles that changed since that buffer was last filled get
	// copied (n3ds_dirty), instead of the whole screen every frame.
	constexpr uint16_t renderTextureWidth = 1024;
	constexpr uint16_t renderTextureHeight = 512;
	C3D_Tex renderTextures[2];
	for (auto &tex : renderTextures)
	{
		C3D_TexInit(&tex, renderTextureWidth, renderTextureHeight, GPU_RGBA8);
		C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
		C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
		memset(tex.data, 0, tex.size);
	}
	int renderBuffer = 0;
	n3ds_dirty::Init(render::vscreen);

	auto uploadScreen = [&](int buffer)
	{
		C3D_Tex &tex = renderTextures[buffer];
		int firstRow, lastRow;
		n3ds_tiles::CopyDirtyTiles<n3ds_dirty::MaxTilesX>(
			static_cast<uint32_t *>(tex.data), renderTextureWidth,
			&render::vscreen->BmpBufPtr1[0].Color, render::vscreen->Stride,
			n3ds_dirty::tiles, n3ds_dirty::tilesX, n3ds_dirty::tilesY,
			static_cast<uint8_t>(1 << buffer), &firstRow, &lastRow);
		if (firstRow >= 0)
		{
			// A tile row is contiguous in memory: flush just the rows written
			const size_t rowBytes = static_cast<size_t>(renderTextureWidth) * 8 * 4;
			GSPGPU_FlushDataCache(static_cast<uint8_t *>(tex.data) + firstRow * rowBytes, (lastRow - firstRow + 1) * rowBytes);
		}
		n3ds_graphics::BindTextureObject(&tex, 0);
	};
	uploadScreen(renderBuffer);

	// Initialize input

	n3ds_input::Initialize();

	// Initialize game

	pb::reset_table();
	pb::firsttime_setup();
	pb::replay_level(0);

	// Camera and bottom-screen UI

	n3ds_ui::Initialize();
	n3ds_font::Initialize(); // packs the game's message font into a texture

	auto cameraMode = static_cast<n3ds_camera::Mode>(options::get_int("Camera Mode", static_cast<int>(n3ds_camera::Mode::Follow)));
	if (static_cast<int>(cameraMode) < 0 || cameraMode >= n3ds_camera::Mode::Count)
		cameraMode = n3ds_camera::Mode::Follow;
	n3ds_camera::State cameraState;
	n3ds_camera::Reset(&cameraState, cameraMode);

	n3ds_controls::Load();

	auto toggleCamera = [&]()
	{
		cameraMode = n3ds_camera::NextMode(cameraMode);
		n3ds_camera::Reset(&cameraState, cameraMode);
		options::set_int("Camera Mode", static_cast<int>(cameraMode));
		options::save();
	};

	// Leaving the menu: unpause, and drop flippers released while it was open
	// (the game ignores button releases while paused).
	auto resume = [&]()
	{
		end_pause();
		uint32_t flippers = n3ds_controls::Mask(n3ds_controls::LeftFlipper) | n3ds_controls::Mask(n3ds_controls::RightFlipper);
		pb::keyup(~n3ds_input::GetButtonsHeld() & flippers);
	};

	// Begin main loop

	bQuit = false;

	// Frame-time meter (Y while the menu is open toggles it)
	bool showPerf = false;
	uint64_t lastFrameStart = svcGetSystemTick();
	float cpuMsAverage = 0.0f;
	int droppedFrames = 0;
	char perfLine[32] = {};
	const double ticksPerMs = SYSCLOCK_ARM11 / 1000.0;

	while (n3ds_graphics::IsMainLoopRunning() && !bQuit)
	{
		uint64_t frameStart = svcGetSystemTick();
		double frameMs = (frameStart - lastFrameStart) / ticksPerMs;
		lastFrameStart = frameStart;
		if (showPerf && frameMs > 1000.0 / 60.0 * 1.5 && !single_step)
			droppedFrames++;

		// Input

		n3ds_input::ScanPads();

		uint32_t buttonsDown = n3ds_input::GetButtonsDown();
		uint32_t buttonsUp = n3ds_input::GetButtonsUp();
		uint32_t buttonsHeld = n3ds_input::GetButtonsHeld();

		// A finished game may have queued high scores: take initials one at a
		// time, with the table frozen behind the entry box.
		bool entering = n3ds_initials::Active();
		if (!entering && high_score::HasPendingEntry())
		{
			int score, rank;
			char label[32];
			if (high_score::PopPendingEntry(&score, &rank, label, sizeof label))
			{
				n3ds_initials::Begin(score, rank, label, options::get_string("Last Initials", "AAA").c_str());
				entering = true;
				buttonsDown = 0; // don't let this frame's press change a letter
			}
		}

		if (entering)
		{
			if (n3ds_initials::Update(buttonsDown, buttonsHeld))
			{
				options::set_string("Last Initials", n3ds_initials::Initials());
				high_score::CommitEntry(n3ds_initials::Score(), n3ds_initials::Initials()); // also saves
			}
			pb::keyup(buttonsUp & (KEY_L | KEY_R)); // let go of flippers held at game over
		}
		else
		{
			// Start, or a tap on the "START Menu" corner, opens the menu and pauses
			bool openMenu = buttonsDown & KEY_START;
			if ((buttonsDown & KEY_TOUCH) && !n3ds_menu::IsOpen())
			{
				touchPosition touch;
				hidTouchRead(&touch);
				openMenu |= touch.px >= MenuTapX && touch.py >= MenuTapY;
			}
			if (openMenu && !n3ds_menu::IsOpen())
			{
				n3ds_menu::Open();
				if (!single_step)
					pause();
				buttonsDown = 0; // this press only opened the menu
			}

			if (buttonsDown & KEY_SELECT)
				toggleCamera();

			if (n3ds_menu::IsOpen() && (buttonsDown & KEY_Y))
			{
				showPerf = !showPerf;
				droppedFrames = 0;
			}

			if (n3ds_menu::IsOpen())
			{
				switch (n3ds_menu::Update(buttonsDown, buttonsHeld))
				{
				case n3ds_menu::Command::None:
					break;
				case n3ds_menu::Command::ToggleCamera:
					toggleCamera();
					break;
				case n3ds_menu::Command::Close:
					resume();
					break;
				case n3ds_menu::Command::NewGame:
					resume();
					new_game();
					n3ds_camera::Reset(&cameraState, cameraMode);
					break;
				case n3ds_menu::Command::ToggleDemo:
					resume();
					pb::toggle_demo();
					n3ds_camera::Reset(&cameraState, cameraMode);
					break;
				case n3ds_menu::Command::Quit:
					bQuit = true;
					break;
				}
				buttonsDown = 0;
				buttonsUp = 0;
			}

			pb::keydown(buttonsDown);
			pb::keyup(buttonsUp);

			if (!single_step)
			{
				// Update game when not paused

				pb::frame(1000.0f / 60.0f);

				// Copy what changed into the texture the GPU isn't using

				renderBuffer ^= 1;
				uploadScreen(renderBuffer);

				n3ds_camera::Update(&cameraState, cameraMode, LowestBallRow());
			}
		}

		// Texture layout: the game's 600x416 screen sits at the top of the
		// 1024x512 texture, and texture v runs bottom-up.
		const float texW = renderTextureWidth, texH = renderTextureHeight;
		auto uvRect = [&](float x, float y, float w, float h, float *out)
		{
			out[0] = x / texW;
			out[1] = (texH - y - h) / texH;
			out[2] = w / texW;
			out[3] = h / texH;
		};

		// Render top screen: the table, through the camera

		if (showPerf)
		{
			// CPU time for this frame's game update and texture upload
			float cpuMs = static_cast<float>((svcGetSystemTick() - frameStart) / ticksPerMs);
			cpuMsAverage += (cpuMs - cpuMsAverage) * 0.1f;
			snprintf(perfLine, sizeof perfLine, "%.1fMS %dD", cpuMsAverage, droppedFrames);
		}

		n3ds_graphics::BeginRender(IsNew3DS ? true : single_step);
		n3ds_font::BeginFrame();

		n3ds_camera::View view;
		n3ds_camera::Compute(cameraMode, cameraState,
		                     static_cast<float>(render::get_offset_x()), static_cast<float>(render::get_offset_y()), &view);

		float tableUv[4];
		uvRect(3.0f, 0.0f, n3ds_camera::TableWidth, static_cast<float>(render::vscreen->Height), tableUv);

		n3ds_graphics::DrawTopRenderTarget(0x000000ff);
		n3ds_graphics::SetProjection(&view.projection);
		n3ds_graphics::DrawQuadMatrix(&view.modelView, tableUv[0], tableUv[1], tableUv[2], tableUv[3]);
		n3ds_initials::DrawTop(n3ds_graphics::GetTopRenderTarget());
		n3ds_menu::DrawTop(n3ds_graphics::GetTopRenderTarget());

		// Render bottom screen: the game's score panel at 1:1, high scores beside it

		float panelX, panelY, panelW, panelH;
		if (pb::FullTiltMode)
		{
			// Full Tilt's panel is one 203x400 strip at (386,12); its lower 240
			// rows hold the ball count, score and message boxes.
			panelX = 386.0f; panelW = 203.0f; panelH = 240.0f; panelY = 412.0f - panelH;
		}
		else
		{
			// 3D Pinball: score and message boxes below the logo
			panelX = 386.0f; panelY = 187.0f; panelW = 203.0f; panelH = 219.0f;
		}

		float panelUv[4];
		uvRect(panelX, panelY, panelW, panelH, panelUv);

		n3ds_graphics::DrawBottomRenderTarget(0x101418ff);
		n3ds_graphics::DrawQuad(
			0.0f,
			GSP_SCREEN_WIDTH - panelH, // top-aligned (y runs up)
			panelW,
			panelH,
			panelUv[0], panelUv[1], panelUv[2], panelUv[3]);

		if (n3ds_initials::Active())
			n3ds_initials::DrawBottom(n3ds_graphics::GetBottomRenderTarget());
		else if (n3ds_menu::IsOpen())
			n3ds_menu::DrawBottom(n3ds_graphics::GetBottomRenderTarget(), n3ds_camera::ModeName(cameraMode), DemoActive);
		else
			n3ds_ui::DrawBottom(n3ds_graphics::GetBottomRenderTarget(), panelW, pb::highscore_table,
			                    n3ds_camera::ModeName(cameraMode), showPerf ? perfLine : nullptr);

		n3ds_graphics::FinishRender();
	}

	printf("Uninitializing...");

	options::uninit();
	pb::uninit(); // writes the high score table into the settings
	options::save();
	midi::music_shutdown();
	Sound::Close();

	n3ds_font::Dispose();
	n3ds_ui::Dispose();

	for (auto &tex : renderTextures)
		C3D_TexDelete(&tex);
	n3ds_graphics::Dispose();
	gfxExit();

#if _ROMFS
	romfsExit();
#endif

	printf("Finished uninitializing.");

	return 0;
}

void winmain::memalloc_failure()
{
	midi::music_stop();
	Sound::Close();
	char *caption = pinball::get_rc_string(170, 0);
	char *text = pinball::get_rc_string(179, 0);

	PrintMessage("\x1b[14;0H  %s\n  %s", caption, text);
}

void winmain::end_pause()
{
	if (single_step)
	{
		pb::pause_continue();
		no_time_loss = 1;
	}
}

void winmain::new_game()
{
	end_pause();
	pb::replay_level(0);
}

void winmain::pause()
{
	pb::pause_continue();
	no_time_loss = 1;
}

void winmain::UpdateFrameRate()
{
	// UPS >= FPS
	auto fps = Options.FramesPerSecond, ups = Options.UpdatesPerSecond;
	UpdateToFrameRatio = static_cast<double>(ups) / fps;
	TargetFrameTime = DurationMs(1000.0 / ups);
}

void winmain::PrintMessage(const char *message, ...)
{
	va_list args;
	va_start(args, message);
	vprintf(message, args);
	va_end(args);

	// Initialize the console for printing error messages

	if (Console == nullptr)
		Console = consoleInit(GFX_TOP, NULL);

	while (aptMainLoop())
	{
		hidScanInput();
		uint32_t kDown = hidKeysDown();

		if (kDown & KEY_START)
			break;

		gfxFlushBuffers();
		gfxSwapBuffers();

		gspWaitForVBlank();
	}

	gfxExit();
	exit(EXIT_FAILURE);
	//svcBreak(USERBREAK_ASSERT);
}