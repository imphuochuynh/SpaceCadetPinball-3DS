// Simulate a ball dropping from the top to the flippers and measure camera smoothness.
#include <cstdio>
#include <cmath>
#include "../../SpaceCadetPinball/n3ds_camera.h"
using namespace n3ds_camera;
int main(int argc, char** argv)
{
	bool quantize = argc > 1;
	State s; Reset(&s, Mode::Follow);
	s.focusRow = 120; float prevFocus = s.focusRow, prevVel = 0, maxJerk = 0, sumJerk = 0; int n = 0;
	float y = 40, v = 0, worst = 0;
	for (int f = 0; f < 150; f++)
	{
		// fall, then a small rattle near the flippers
		if (y < 380) { v += 0.25; y += v; } else { y = 380 + 6 * sinf(f * 0.9f); }
		float reported = quantize ? floorf(y) : y;
		Update(&s, Mode::Follow, reported);
		float vel = s.focusRow - prevFocus, acc = vel - prevVel;
		if (f > 2) { maxJerk = fmaxf(maxJerk, fabsf(acc)); sumJerk += fabsf(acc); n++; }
		prevVel = vel; prevFocus = s.focusRow;
		worst = fmaxf(worst, fabsf(y - s.focusRow));
	}
	printf("ball farthest from screen center: %.0f rows (screen edge at 119)\n", worst);
	printf("%s: camera acceleration per frame  max %.2f px  avg %.2f px\n", quantize ? "whole-pixel ball (current)" : "sub-pixel ball", maxJerk, sumJerk / n);
}
