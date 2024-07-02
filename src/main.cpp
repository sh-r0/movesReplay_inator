#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include <bitset>
#include <thread>
#include <atomic>
#include <format>

#ifdef _WIN32
	#include <Windows.h>
	#include <WinUser.h>
#else 
	// TODO: include linux 
#endif 

//mouse move, mouse down, mouse up, kbd down, kbd up
enum EVENT_TYPE : uint8_t {
	EVENT_TYPE_NULL = 0,

	EVENT_TYPE_MS_MOVE, 
	EVENT_TYPE_MS_RDOWN,
	EVENT_TYPE_MS_RUP,
	EVENT_TYPE_MS_LDOWN,
	EVENT_TYPE_MS_LUP, 

	EVENT_TYPE_KBD_DOWN = 32, 
	EVENT_TYPE_KBD_UP,
};

//KBDLLHOOKSTRUCT
struct kbdEvent_t {
	DWORD   vkCode;
	DWORD   scanCode;
	DWORD   flags;
	DWORD   time;

	kbdEvent_t(const KBDLLHOOKSTRUCT& _kbdHkStruct) {
		vkCode = _kbdHkStruct.vkCode;
		scanCode = _kbdHkStruct.scanCode;
		flags = _kbdHkStruct.flags;
		time = _kbdHkStruct.time;
		return;
	}
};

//MSLLHOOKSTRUCT
struct msEvent_t {
	POINT		pt;
	DWORD   mouseData;
	DWORD   flags;
	DWORD   time;

	msEvent_t(const MSLLHOOKSTRUCT& _msHkStruct) {
		pt = _msHkStruct.pt;
		mouseData = _msHkStruct.mouseData;
		flags = _msHkStruct.flags;
		time = _msHkStruct.time;
		return;
	}
};

struct event_t {
	union EU {
		kbdEvent_t kbdEvent;
		msEvent_t msEvent;
	};

	EU eventUnion;	
	uint32_t deltaTime;	// ms since last event 
	uint8_t type;				// EVENT_TYPE
};

constexpr uint8_t toggleKey_c = VK_F12;
constexpr uint8_t playKey_c = VK_F11;

std::chrono::steady_clock::time_point lastEventTime_g;
std::vector<event_t>* eventsPtr_g;
std::bitset<256> kbdState_g;
std::pair<uint16_t, uint16_t> cursorPos_g;

bool isLogging_g = false; 
bool isPlaying_g = false;

inline uint32_t getDeltaTime(void) {
	 std::chrono::steady_clock::time_point newEventTime = std::chrono::steady_clock::now();
	 uint32_t dt = std::chrono::duration_cast<std::chrono::milliseconds>(newEventTime - lastEventTime_g).count();
	 lastEventTime_g = newEventTime;
	 return dt;
 }

void replayMoves(void);

LRESULT CALLBACK LLKeyboardProc(int32_t _code, WPARAM _wParam, LPARAM _lParam) { 
	if (_code < 0 || isPlaying_g) return CallNextHookEx(0,_code,_wParam,_lParam);

	KBDLLHOOKSTRUCT& kbdHookStruct = *(KBDLLHOOKSTRUCT*)_lParam;
	if (kbdHookStruct.vkCode == playKey_c && 
		(_wParam == WM_KEYDOWN || _wParam == WM_SYSKEYDOWN) &&
		!isLogging_g) {
		replayMoves();
		return CallNextHookEx(0, _code, _wParam, _lParam);
	}

	if (kbdHookStruct.vkCode != toggleKey_c && !isLogging_g)
		return CallNextHookEx(0, _code, _wParam, _lParam);

	std::chrono::steady_clock::time_point newEventTime = std::chrono::steady_clock::now();
	uint32_t dt = std::chrono::duration_cast<std::chrono::milliseconds>(newEventTime - lastEventTime_g).count();
	
	event_t tmp{ 
		.eventUnion = { .kbdEvent = kbdEvent_t(kbdHookStruct) }, 
		.deltaTime = dt, 
		.type = 0 
	};
		
	switch(_wParam) {
		case WM_SYSKEYUP: 
		case WM_KEYUP: {
			if (kbdHookStruct.vkCode == toggleKey_c || kbdHookStruct.vkCode == playKey_c) return CallNextHookEx(0, _code, _wParam, _lParam);
			
			lastEventTime_g = newEventTime;
			kbdState_g[kbdHookStruct.vkCode] = 0;
			tmp.type = EVENT_TYPE_KBD_UP;
			eventsPtr_g->push_back(tmp);
		} break;
		case WM_SYSKEYDOWN: 
		case WM_KEYDOWN: {
			if(kbdState_g[kbdHookStruct.vkCode] != 0) return CallNextHookEx(0, _code, _wParam, _lParam);
			if (kbdHookStruct.vkCode == toggleKey_c) {
				if (isLogging_g) {
					std::cout << "ended logging!\n";
				} else std::cout << "started logging!\n";
				isLogging_g = !isLogging_g;
				return CallNextHookEx(0, _code, _wParam, _lParam);
			}	

			lastEventTime_g = newEventTime;
			kbdState_g[kbdHookStruct.vkCode] = 1;
			tmp.type = EVENT_TYPE_KBD_DOWN;
			eventsPtr_g->push_back(tmp);
		} break;
	}

	//std::cout << std::format("delay:{} type:{} letter:{}\n", eventsPtr_g->back().deltaTime, eventsPtr_g->back().type, (char)(eventsPtr_g->back().eventUnion.kbdEvent.vkCode));
	return CallNextHookEx(0, _code,_wParam,_lParam);
}

LRESULT CALLBACK LLMouseProc(int32_t _code, WPARAM _wParam, LPARAM _lParam) {
	while (isPlaying_g) Sleep(1);
	if (_code < 0 || !isLogging_g) return CallNextHookEx(0, _code, _wParam, _lParam);

	MSLLHOOKSTRUCT& msHookStruct = *(MSLLHOOKSTRUCT*)_lParam;
	std::chrono::steady_clock::time_point newEventTime = std::chrono::steady_clock::now();
	uint32_t dt = std::chrono::duration_cast<std::chrono::milliseconds>(newEventTime - lastEventTime_g).count();
	lastEventTime_g = newEventTime;

	//std::cout << std::format("dt = {}\n", dt);
	event_t tmp { 
		.eventUnion = { .msEvent = msEvent_t(msHookStruct) }, 
		.deltaTime = dt, 
		.type = 0 
	};

	switch(_wParam) {
		case WM_LBUTTONDOWN: {
			tmp.type = EVENT_TYPE_MS_LDOWN;
			eventsPtr_g->push_back(tmp);
		} break;
		case WM_LBUTTONUP: {
			tmp.type = EVENT_TYPE_MS_LUP;
			eventsPtr_g->push_back(tmp);
		} break;
		case WM_RBUTTONDOWN: {
			tmp.type = EVENT_TYPE_MS_RDOWN;
			eventsPtr_g->push_back(tmp);
		} break;
		case WM_RBUTTONUP: {
			tmp.type = EVENT_TYPE_MS_RUP;
			eventsPtr_g->push_back(tmp);
		} break;
		case WM_MOUSEMOVE: {
			tmp.type = EVENT_TYPE_MS_MOVE;
			if (!(eventsPtr_g->empty()) && eventsPtr_g->back().type == EVENT_TYPE_MS_MOVE) {
				eventsPtr_g->back().eventUnion.msEvent = tmp.eventUnion.msEvent;
				eventsPtr_g->back().deltaTime += dt;
			} else {
				eventsPtr_g->push_back(tmp);
			}
		} break;
		case WM_MOUSEWHEEL: {
			//TODO
		} break;
	}

	//std::cout << std::format("delay:{} type:{}\n", eventsPtr_g->back().deltaTime, eventsPtr_g->back().type);
	return CallNextHookEx(0, _code, _wParam, _lParam);
}

void sendMouseInput(int32_t _flags) {
	MOUSEINPUT mInput = {};
	mInput.dx = 0;
	mInput.dy = 0;
	mInput.mouseData = XBUTTON1;
	mInput.dwFlags = _flags;
	mInput.dwExtraInfo = 0;

	INPUT input = {};
	input.type = 0;
	input.mi = mInput;

	SendInput(1, &input, sizeof(input));
	return;
}

void sendKeyboardInput(int16_t _wVk, bool _up) {
	KEYBDINPUT kInput = {};
	kInput.wVk = _wVk;
	kInput.dwExtraInfo = 0;
	kInput.dwFlags = 0;
	kInput.wScan = 0;
	if(_up)
		kInput.wScan = KEYEVENTF_KEYUP;
	kInput.time = 0;

	INPUT input = {};
	input.type = INPUT_KEYBOARD;
	input.ki = kInput;
	SendInput(1, &input, sizeof(input));

	return;
}

void replayMoves(void) {
	std::cout << "started playing!";
	for (const auto& a : *eventsPtr_g) {
		Sleep(a.deltaTime + 3);
		switch (a.type) {
			case EVENT_TYPE_MS_MOVE: {
				POINT p = a.eventUnion.msEvent.pt;
				SetCursorPos(p.x, p.y);
			} break;
			case EVENT_TYPE_MS_RDOWN: {
				sendMouseInput(MOUSEEVENTF_RIGHTDOWN);
			} break;
			case EVENT_TYPE_MS_RUP: {
				sendMouseInput(MOUSEEVENTF_RIGHTUP);
			} break;
			case EVENT_TYPE_MS_LDOWN: {
				sendMouseInput(MOUSEEVENTF_LEFTDOWN);
			} break;
			case EVENT_TYPE_MS_LUP: {
				sendMouseInput(MOUSEEVENTF_LEFTUP);
			} break;
			case EVENT_TYPE_KBD_DOWN: {
				sendKeyboardInput(a.eventUnion.kbdEvent.vkCode, false);
			} break;
			case EVENT_TYPE_KBD_UP: {
				//sendKeyboardInput(a.eventUnion.kbdEvent.vkCode, true);
			} break;	
		}
	}
	std::cout << "ended playing!";
	isPlaying_g = false;
	return;
}

int32_t main() {
	HHOOK kbdHook = SetWindowsHookExA(WH_KEYBOARD_LL, LLKeyboardProc, 0, 0);
	if (kbdHook == 0) {
		fprintf(stderr, "Failed to set keyboard hook!");
		return -1;
	}
	HHOOK msHook = SetWindowsHookExA(WH_MOUSE_LL, LLMouseProc, 0, 0);
	if (msHook == 0) {
		fprintf(stderr, "Failed to set mouse hook!");
		return -1;
	}
	std::vector<event_t> events{};
	eventsPtr_g = &events;
	cursorPos_g = { 0,0 };
	lastEventTime_g = std::chrono::steady_clock::now();

	LPMSG Msg{};
	while (GetMessage(Msg, NULL, 0, 0) > 0) {
		TranslateMessage(Msg);
		DispatchMessage(Msg);
	}
	return 0;
}