#pragma once

struct high_score_struct
{
	char Name[32];
	int Score;
};


class high_score
{
public:
	static int read(high_score_struct* table);
	static int write(high_score_struct* table);
	static void clear_table(high_score_struct* table);
	static int get_score_position(high_score_struct* table, int score);
	static int place_new_score_into(high_score_struct* table, int score, LPSTR scoreStr, int position);

	static void show_high_score_dialog(high_score_struct* table);
	static void show_and_set_high_score_dialog(high_score_struct* table, int score, int pos, LPCSTR defaultName);
	static void RenderHighScoreDialog();

	// 3DS: scores waiting for initials, queued at game end and entered one at
	// a time on screen (n3ds_initials).
	static bool HasPendingEntry();
	// Next score that still makes the table: its rank (1-5) and player label.
	static bool PopPendingEntry(int* score, int* rank, char* playerLabel, size_t labelSize);
	static void CommitEntry(int score, const char* name);
private :
	struct pending_entry
	{
		int score;
		char defaultName[32];
	};
	static std::vector<pending_entry> pending;
	static void SaveNow(high_score_struct* table);

	static int dlg_enter_name;
	static int dlg_score;
	static int dlg_position;
	static char default_name[32];
	static high_score_struct* dlg_hst;
	static bool ShowDialog;
};
