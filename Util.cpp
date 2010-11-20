#include "stdafx.h"
#include "util.h"
#include <fcntl.h>
#include <io.h>

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif

namespace Util
{
	void CreateConsole()
	{
		static bool consoleOpen = false;
		if(consoleOpen)
			return;

		// redirect unbuffered STDOUT to the console
		int hConHandle;
		long lStdHandle;
		FILE *fp;
		AllocConsole();
		lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
		*stdout = *fp;
		setvbuf( stdout, NULL, _IONBF, 0 );
		// redirect unbuffered STDIN to the console
		lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "r" );
		*stdin = *fp;
		setvbuf( stdin, NULL, _IONBF, 0 );
		// redirect unbuffered STDERR to the console
		lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
		hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
		fp = _fdopen( hConHandle, "w" );
		*stderr = *fp;
		setvbuf( stderr, NULL, _IONBF, 0 );
		consoleOpen = true;
	}

	void Print(const char* s)
	{
		Print(CString(s));
	}

	void Print(const wchar_t* s)
	{
		CString str;
		str.Format(L"[%4x] %s%s", GetCurrentThreadId(), s, wcschr(s, '\n') ? L"" : L"\n");
		OutputDebugString(str);
	}

	CString FormatSize(__int64 size)
	{
		static TCHAR prefix[] = _T("kMGT");
		CString s;
		if(size < 1024)
			s.Format(_T("%I64d B"), size);
		else {
			int idx = -1;
			double dsize = (double)size;
			while(dsize >= 1024.0 && idx+1 < sizeof(prefix)/sizeof(prefix[0])) {
				idx++;
				dsize /= 1024.0;
			}
			if(dsize < 10.0)
				s.Format(_T("%.2f %cB"), dsize, prefix[idx]);
			else if(dsize < 100.0)
				s.Format(_T("%.1f %cB"), dsize, prefix[idx]);
			else
				s.Format(_T("%.0f %cB"), dsize, prefix[idx]);
		}
		return s;
	}

	__int64 ParseSize(const CString& s)
	{
		double f1 = _tstof(s);
		if(s.Find('k') >= 0) f1 *= 1024.;
		else if(s.Find('M') >= 0) f1 *= 1024. * 1024.;
		else if(s.Find('G') >= 0) f1 *= 1024. * 1024. * 1024.;
		else if(s.Find('T') >= 0) f1 *= 1024. * 1024. * 1024. * 1024.;
		return (__int64)f1;
	}

	CString FormatSpeed(__int64 speed)
	{
		return FormatSize(speed) + _T("/s");
	}

	CString FormatTimeSpan(__int64 span) // span is in seconds
	{
		static const __int64 minute = 60;
		static const __int64 hour = minute * 60;
		static const __int64 day = hour * 24;
		static const __int64 week = day * 7;
		CString s;
		int weeks = (int)(span / week);
		int days = (span / day) % 7;
		int hours = (span / hour) % 24;
		int minutes = (span / minute) % 60;
		int seconds = span % 60;
		if(weeks > 0)
			s.Format(_T("%dw %dd"), weeks, days);
		else if(days > 0)
			s.Format(_T("%dd %dh"), days, hours);
		else if(hours > 0)
			s.Format(_T("%dh %dm"), hours, minutes);
		else
			s.Format(_T("%dm %ds"), minutes, seconds);
		return s;
	}

	__int64 ParseTimeSpan(const CString& s)
	{
		static const __int64 minute = 60;
		static const __int64 hour = minute * 60;
		static const __int64 day = hour * 24;
		static const __int64 week = day * 7;

		if(s == _T("\x221e"))
			return LLONG_MAX;

		TCHAR* end = NULL;
		__int64 v1 = _tcstoul(s, &end, 10);
		if(end && *end) {
			if(*end == 'm') v1 *= minute;
			else if(*end == 'h') v1 *= hour;
			else if(*end == 'd') v1 *= day;
			else if(*end == 'w') v1 *= week;

			end++;
			__int64 v2 = _tcstoul(end, &end, 10);
			if(*end == 'm') v2 *= minute;
			else if(*end == 'h') v2 *= hour;
			else if(*end == 'd') v2 *= day;
			else if(*end == 'w') v2 *= week;

			return v1 + v2;
		}

		return v1;
	}

	CString SanitizeFilename(const CString& fn)
	{
		CString out;
		static const TCHAR* reserved = _T("<>:/\\|?*");
		for(int i = 0; i < fn.GetLength(); i++) {
			TCHAR c = fn[i];
			if(c > 31 && !_tcschr(reserved, c))
				out += c;
		}
		return out;
	}

	void DeleteDirectory(const CString& _path)
	{
		CString path(_path);
		if(path.Right(1) != _T("\\")) path += '\\';
		WIN32_FIND_DATA fdata;
		HANDLE h = FindFirstFile(path + _T("*"), &fdata);
		if(h != INVALID_HANDLE_VALUE) {
			do {
				if(!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					CFile::Delete(path + fdata.cFileName);
			} while(FindNextFile(h, &fdata));
		}
		FindClose(h);
		RemoveDirectory(_path);
	}
};

// CToolBarImageList
//////////////////////////////////////////////////////////////////////////

CToolBarImageList::~CToolBarImageList()
{
	ilNormal.Destroy();
	ilDisabled.Destroy();
}

bool CToolBarImageList::Load(const CString& path, int cx, int cy)
{
	CImage image;
	if(!SUCCEEDED(image.Load(path)))
		return false;
	return Load(image, cx, cy);
}

bool CToolBarImageList::LoadFromResource(ATL::_U_STRINGorID bitmap, int cx, int cy)
{
	CImage image;
	image.LoadFromResource(ModuleHelper::GetResourceInstance(), bitmap.m_lpstr);
	return Load(image, cx, cy);
}

bool CToolBarImageList::Load(CImage &image, int cx, int cy)
{
	if(image.GetBPP() != 32)
		return false;

	ilNormal.Create(cx, cy, ILC_COLOR32, 0, 100);
	ilNormal.Add((HBITMAP)image, (HBITMAP)NULL);

	// generate disabled image by applying 50% alpha and 75% desaturation
	unsigned char* bits = (unsigned char*)image.GetBits();
	int pitch = image.GetPitch();
	for(int y = 0; y < image.GetHeight(); y++) {
		for(int x = 0; x < image.GetWidth(); x++) {
			unsigned char* pp = &bits[x*4];
			int grey = ((unsigned)pp[0] + (unsigned)pp[1] + (unsigned)pp[2]) / 3;
			int r = pp[0] + ((grey - pp[0]) * 3 >> 2);
			int g = pp[1] + ((grey - pp[1]) * 3 >> 2);
			int b = pp[2] + ((grey - pp[2]) * 3 >> 2);
			pp[0] = r;
			pp[1] = g;
			pp[2] = b;
			pp[3] >>= 1;
		}
		bits += pitch;
	}
	ilDisabled.Create(24, 24, ILC_COLOR32, 0, 100);
	ilDisabled.Add((HBITMAP)image, (HBITMAP)NULL);

	return true;
}

void CToolBarImageList::Set(HWND hwndToolBar)
{
	CToolBarCtrl toolBar(hwndToolBar);

	toolBar.SetImageList(ilNormal);
	toolBar.SetDisabledImageList(ilDisabled);
}