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

	uint32_t screenTexturePixelCount = render::vscreen->Width * render::vscreen->Height;

	constexpr uint16_t renderTextureWidth = 1024;
	constexpr uint16_t renderTextureHeight = 512;
	uint32_t renderTextureByteCount = n3ds_graphics::GetTextureSize(renderTextureWidth, renderTextureHeight, GPU_RGBA8, 0);
	uint8_t *renderTextureData = (uint8_t *)linearAlloc(renderTextureByteCount);
	memset(renderTextureData, 0, renderTextureByteCount);

	C3D_Tex renderTextureObject;
	n3ds_graphics::CreateTextureObject(&renderTextureObject, renderTextureWidth, renderTextureHeight, GPU_RGBA8, GPU_CLAMP_TO_EDGE, GPU_LINEAR);
	n3ds_graphics::BindTextureObject(&renderTextureObject, 0);

	// Precalculate the Morton swizzle

	uint8_t **swizzle = GenerateMortonSwizzle(renderTextureData, renderTextureWidth, screenTexturePixelCount);

	// Initialize input

	n3ds_input::Initialize();

	// Initialize game

	pb::reset_table();
	pb::firsttime_setup();
	pb::replay_level(0);

	// Camera and bottom-screen UI

	n3ds_ui::Initialize();

	auto cameraMode = static_cast<n3ds_camera::Mode>(options::get_int("Camera Mode", static_cast<int>(n3ds_camera::Mode::Follow)));
	if (static_cast<int>(cameraMode) < 0 || cameraMode >= n3ds_camera::Mode::Count)
		cameraMode = n3ds_camera::Mode::Follow;
	n3ds_camera::State cameraState;
	n3ds_camera::Reset(&cameraState, cameraMode);

	// Begin main loop

	bQuit = false;

	while (n3ds_graphics::IsMainLoopRunning() && !bQuit)
	{
		// Input

		n3ds_input::ScanPads();

		uint32_t buttonsDown = n3ds_input::GetButtonsDown();
		uint32_t buttonsUp = n3ds_input::GetButtonsUp();

		if (buttonsDown & KEY_Y)
			break;

		if (buttonsDown & KEY_START)
			pause();

		if (buttonsDown & KEY_X)
		{
			new_game();
			n3ds_camera::Reset(&cameraState, cameraMode);
		}

		if (buttonsDown & KEY_SELECT)
		{
			cameraMode = n3ds_camera::NextMode(cameraMode);
			n3ds_camera::Reset(&cameraState, cameraMode);
			options::set_int("Camera Mode", static_cast<int>(cameraMode));
			options::save();
		}

		pb::keydown(buttonsDown);
		pb::keyup(buttonsUp);

		// A finished game may have queued high scores; ask for names now,
		// between frames, since the system keyboard is a separate applet.
		if (high_score::HasPendingEntry())
		{
			high_score::ProcessPendingEntry();
			n3ds_input::ScanPads(); // drop button presses made in the keyboard
			continue;
		}

		if (!single_step)
		{
			// Update game when not paused

			pb::frame(1000.0f / 60.0f);

			// Swizzle the texture, copy it to render texture and upload

			SwizzleAndCopy(swizzle, screenTexturePixelCount);
			n3ds_graphics::UploadTextureObject(&renderTextureObject, renderTextureData);

			n3ds_camera::Update(&cameraState, cameraMode, LowestBallRow());
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

		n3ds_graphics::BeginRender(IsNew3DS ? true : single_step);

		n3ds_camera::View view;
		n3ds_camera::Compute(cameraMode, cameraState,
		                     static_cast<float>(render::get_offset_x()), static_cast<float>(render::get_offset_y()), &view);

		float tableUv[4];
		uvRect(3.0f, 0.0f, n3ds_camera::TableWidth, static_cast<float>(render::vscreen->Height), tableUv);

		n3ds_graphics::DrawTopRenderTarget(0x000000ff);
		n3ds_graphics::SetProjection(&view.projection);
		n3ds_graphics::DrawQuadMatrix(&view.modelView, tableUv[0], tableUv[1], tableUv[2], tableUv[3]);

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

		n3ds_ui::DrawBottom(n3ds_graphics::GetBottomRenderTarget(), panelW, pb::highscore_table,
		                    n3ds_camera::ModeName(cameraMode), single_step != 0);

		n3ds_graphics::FinishRender();
	}

	printf("Uninitializing...");

	options::uninit();
	pb::uninit(); // writes the high score table into the settings
	options::save();
	midi::music_shutdown();
	Sound::Close();

	n3ds_ui::Dispose();

	delete[] swizzle;
	C3D_TexDelete(&renderTextureObject);
	linearFree(renderTextureData);
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

uint8_t **winmain::GenerateMortonSwizzle(uint8_t *renderTextureData, const uint16_t renderTextureWidth, const uint32_t screenTexturePixelCount)
{
	uint8_t **swizzle = new uint8_t *[screenTexturePixelCount >> 1];

	uint32_t dstOffset = 0;
	uint32_t widthCount = 0;
	constexpr uint32_t tileSize = 8;
	constexpr uint32_t subTileSize1 = 4;
	constexpr uint32_t subTileSize2 = 2;
	uint32_t widthBytes = render::vscreen->Width * tileSize * 4;

	for (int32_t y = 0; y < render::vscreen->Height; y += tileSize)
	{
		for (int32_t x = 0; x < render::vscreen->Width; x += tileSize)
		{
			for (uint8_t ty = 0; ty < tileSize; ty += subTileSize1)
			{
				for (uint8_t tx = 0; tx < tileSize; tx += subTileSize1)
				{
					for (uint8_t sty1 = 0; sty1 < subTileSize1; sty1 += subTileSize2)
					{
						for (uint8_t stx1 = 0; stx1 < subTileSize1; stx1 += subTileSize2)
						{
							for (uint8_t sty2 = 0; sty2 < subTileSize2; sty2++)
							{
								uint32_t index = (y + ty + sty1 + sty2) * render::vscreen->Width + (x + tx + stx1);
								swizzle[index >> 1] = &renderTextureData[dstOffset + 0];
								//swizzle[index + 1] = &renderTextureData[dstOffset + 4];

								dstOffset += 8;
								widthCount += 8;
								if (widthCount == widthBytes)
								{
									dstOffset += (renderTextureWidth - render::vscreen->Width) << 5;
									widthCount = 0;
								}
							}
						}
					}
				}
			}
		}
	}

	return swizzle;
}

void winmain::SwizzleAndCopy(uint8_t **swizzle, const uint32_t screenTexturePixelCount)
{
	// for (int32_t y = 0; y < render::vscreen->Height; y++)
	// {
	// 	for (int32_t x = 0; x < render::vscreen->Width; x++)
	// 	{
	// 		render::vscreen->BmpBufPtr1[y * render::vscreen->Width + x].rgba.Alpha = 0xFF;
	// 		render::vscreen->BmpBufPtr1[y * render::vscreen->Width + x].rgba.Blue = 0x00;
	// 		render::vscreen->BmpBufPtr1[y * render::vscreen->Width + x].rgba.Green = y & 0xff;
	// 		render::vscreen->BmpBufPtr1[y * render::vscreen->Width + x].rgba.Red = x & 0xff;
	// 	}
	// }

	for (uint32_t i = 0; i < screenTexturePixelCount; i += 16)
	{
		uint32_t *src = &render::vscreen->BmpBufPtr1[i].Color;
		uint8_t **dst = &swizzle[i >> 1];
		memcpy(dst[0], &src[0], 8);
		memcpy(dst[1], &src[2], 8);
		memcpy(dst[2], &src[4], 8);
		memcpy(dst[3], &src[6], 8);
		memcpy(dst[4], &src[8], 8);
		memcpy(dst[5], &src[10], 8);
		memcpy(dst[6], &src[12], 8);
		memcpy(dst[7], &src[14], 8);
	}
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