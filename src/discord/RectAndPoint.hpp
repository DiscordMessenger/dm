#pragma once

#define W32RECT(rc) rc.left, rc.top, rc.right, rc.bottom

struct Rect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;

	int Height() const { return bottom - top; }
	int Width() const { return right - left; }

	Rect() {}
	Rect(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
};

struct Point {
	int x = 0;
	int y = 0;

	Point() {}
	Point(int _x, int _y):x(_x), y(_y) {}
};
