#include "Watchdog.h"
#include <thread>

void Watchdog::start()
{
	std::thread{ &Watchdog::run, this }.detach();
}

void Watchdog::run()
{
	while (!terminate) {
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleepTime * 1000)));
		action(condition());
	}
}

//Keep Watchdog - Keep the big panel off until all panels are solved
//The condition is not entirely correct, need to figure out how to check for sure if the puzzle is solved

bool KeepWatchdog::condition() {
	if (!ready) {
		float power = ReadPanelData<float>(0x03317, POWER);
		if (!power) return false;
		ready = true;
	}
	int numTraced = ReadPanelData<int>(0x01BE9, TRACED_EDGES);
	int tracedptr = ReadPanelData<int>(0x01BE9, TRACED_EDGE_DATA);
	std::vector<SolutionPoint> traced; if (tracedptr) traced = ReadArray<SolutionPoint>(0x01BE9, TRACED_EDGE_DATA, numTraced);
	if (traced.size() < 12 || traced.size() > 26 || traced[traced.size() - 1].pointB != 25) return false;
	for (SolutionPoint p : traced) {
		if (p.pointA == 16 && p.pointB == 17 || p.pointB == 16 && p.pointA == 17) {
			return true;
		}
	}
	return false;
}

void KeepWatchdog::action(bool status) {
	if (status) {
		WritePanelData<float>(0x03317, POWER, { 1, 1 });
	}
	else {
		WritePanelData<float>(0x03317, POWER, { 0, 0 });
	}
}

//Arrow Watchdog - To run the arrow puzzles

bool ArrowWatchdog::condition() {
	int length = ReadPanelData<int>(id, TRACED_EDGES);
	return length != 0 && length != solLength;
}

void ArrowWatchdog::action(bool status) {
	if (!status) {
		sleepTime = 0.2f;
		return;
	}
	sleepTime = 0.01f;
	solLength = 0;
	int length = ReadPanelData<int>(id, TRACED_EDGES);
	if (length == tracedLength) return;
	initPath();
	if (tracedLength == solLength || tracedLength == solLength - 1) {
		for (int x = 1; x < width; x++) {
			for (int y = 1; y < height; y++) {
				if (!checkArrow(x, y)) {
					WritePanelData<int>(id, STYLE_FLAGS, { style | Panel::Style::HAS_TRIANGLES });
					return;
				}
			}
		}
		WritePanelData<int>(id, STYLE_FLAGS, { style & ~Panel::Style::HAS_TRIANGLES });
	}
}

void ArrowWatchdog::initPath()
{
	int numTraced = ReadPanelData<int>(id, TRACED_EDGES);
	int tracedptr = ReadPanelData<int>(id, TRACED_EDGE_DATA);
	if (!tracedptr) return;
	std::vector<SolutionPoint> traced = ReadArray<SolutionPoint>(id, TRACED_EDGE_DATA, numTraced);
	grid = backupGrid;
	tracedLength = numTraced;
	if (traced.size() == 0) return;
	int exitPos = (width / 2 + 1) * (height / 2 + 1);
	for (SolutionPoint p : traced) {
		int p1 = p.pointA, p2 = p.pointB;
		if (p1 == this->exitPos || p2 == this->exitPos) {
			solLength = numTraced + 1;
		}
		if (p1 == exitPos || p2 == exitPos) {
			solLength = numTraced;
			continue;
		}
		else if (p1 > exitPos || p2 > exitPos) continue;
		if (p1 == 0 && p2 == 0 || p1 < 0 || p2 < 0) {
			return;
		}
		int x1 = (p1 % (width / 2 + 1)) * 2, y1 = height - 1 - (p1 / (width / 2 + 1)) * 2;
		int x2 = (p2 % (width / 2 + 1)) * 2, y2 = height - 1 - (p2 / (width / 2 + 1)) * 2;
		grid[x1][y1] = PATH;
		grid[x2][y2] = PATH;
		grid[(x1 + x2) / 2][(y1 + y2) / 2] = PATH;
	}
}

bool ArrowWatchdog::checkArrow(int x, int y)
{
	int symbol = grid[x][y];
	if ((symbol & 0x700) != Decoration::Arrow)
		return true;
	int targetCount = (symbol & 0xf000) >> 12;
	Point dir = DIRECTIONS[(symbol & 0xf0000) >> 16];
	x += dir.first / 2; y += dir.second / 2;
	int count = 0;
	while (x >= 0 && x < width && y >= 0 && y < height) {
		if (grid[x][y] == PATH) {
			if (++count > targetCount) return false;
		}
		x += dir.first; y += dir.second;
	}
	return count == targetCount;
}
