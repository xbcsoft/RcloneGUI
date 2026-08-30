#pragma once
#include "stdafx.h"
#include <winver.h>
#pragma comment(lib, "version.lib")

namespace common {
static int CALLBACK _BrowseFolderCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED && lpData) {
		const wchar_t* dir = (const wchar_t*)lpData;
		if (dir && *dir) {
			SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, (LPARAM)dir);
		}
	}
	return 0;
}

//此方法将在BE0.8+中再引入，当前仅用作实验性实现
inline StrX 浏览文件夹(c_StrX 标题, c_StrX 初始目录, bool 地址栏, bool 新样式, bool 显示文件, NilOpt<HWND> 父窗口)
{
	HWND hOwner = 父窗口.OR(GetActiveWindow());
	charW szPath[MAX_PATH] = { 0 };
	BROWSEINFOW bi = { 0 };
	bi.hwndOwner = hOwner;
	StrW wTitle = 标题 ? (StrW)标题 : StrW(L"请选择一个文件夹");
	bi.lpszTitle = wTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	if (地址栏) bi.ulFlags |= BIF_EDITBOX;
	if (新样式) bi.ulFlags |= BIF_NEWDIALOGSTYLE;
	if (显示文件) bi.ulFlags |= BIF_BROWSEINCLUDEFILES;

	StrW wInitDir = 初始目录;
	if (wInitDir.len() > 0) {
		bi.lpfn = _BrowseFolderCallbackProc;
		bi.lParam = (LPARAM)(const wchar_t*)wInitDir;
	}

	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
	if (pidl != 0) {
		SHGetPathFromIDListW(pidl, szPath);
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc))) {
			imalloc->Free(pidl);
			imalloc->Release();
		}
		return StrX(szPath);
	}
	return "";
}

static bool _匹配挂载路径名(const wchar_t* path, const wchar_t* name)
{
	if (!path || !name || !*path || !*name) return false;
	size_t nameLen = wcslen(name);
	if (wcslen(path) < nameLen) return false;

	if (_wcsicmp(path, name) == 0) return true;

	const wchar_t* p = path;
	while ((p = wcsstr(p, name)) != NULL) {
		bool prefixOk = (p == path || *(p - 1) == L'\\' || *(p - 1) == L'/');
		const wchar_t* end = p + nameLen;
		bool suffixOk = (*end == 0 || *end == L'\\' || *end == L'/');
		if (prefixOk && suffixOk) {
			return true;
		}
		p++;
	}
	return false;
}

inline StrW 取已挂载驱动器真实盘符(c_StrW driveName)
{
	if (!driveName) return L"";

	WCHAR szDrive[4] = L"A:";
	for (WCHAR c = L'Z'; c >= L'A'; --c) {
		szDrive[0] = c;

		// 2. QueryDosDeviceW 卷/符号链接匹配 (例如 \Device\Volume{...}\server\SFTP)
		WCHAR szTarget[512] = { 0 };
		if (QueryDosDeviceW(szDrive, szTarget, 512) > 0) {
			if (_匹配挂载路径名(szTarget, (const wchar_t*)driveName) &&
				(wcsstr(szTarget, L"Volume") || wcsstr(szTarget, L"WinFsp") || wcsstr(szTarget, L"rclone") || wcsstr(szTarget, L"Mup"))) {
				return StrW(szDrive);
			}
		}

		// 3. GetVolumeInformationW 卷标与文件系统名匹配
		WCHAR szVol[512] = { 0 };
		WCHAR szFS[512] = { 0 };
		WCHAR szRoot[5] = { c, L':', L'\\', 0 };
		if (GetVolumeInformationW(szRoot, szVol, 512, NULL, NULL, NULL, szFS, 512)) {
			if ((wcsstr(szFS, L"rclone") || wcsstr(szFS, L"FUSE") || wcsstr(szFS, L"WinFsp")) &&
				(_wcsicmp(szVol, (const wchar_t*)driveName) == 0 || _匹配挂载路径名(szTarget, (const wchar_t*)driveName))) {
				return StrW(szDrive);
			}
		}
	}
	return L"";
}

inline StrW 取自身文件版本()
{
	WCHAR szExePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, szExePath, MAX_PATH);

	DWORD dwHandle = 0;
	DWORD dwSize = GetFileVersionInfoSizeW(szExePath, &dwHandle);
	if (dwSize == 0) return L"";

	Bytes buf(dwSize);
	if (!GetFileVersionInfoW(szExePath, dwHandle, dwSize, buf.buf)) {
		return L"";
	}

	VS_FIXEDFILEINFO* pFileInfo = NULL;
	UINT uLen = 0;
	if (VerQueryValueW(buf.buf, L"\\", (LPVOID*)&pFileInfo, &uLen) && pFileInfo && uLen >= sizeof(VS_FIXEDFILEINFO)) {
		WORD major = HIWORD(pFileInfo->dwFileVersionMS);
		WORD minor = LOWORD(pFileInfo->dwFileVersionMS);
		WORD build = HIWORD(pFileInfo->dwFileVersionLS);

		return sprintF<W>(L"%d.%d.%d", major, minor, build);
	}
	return L"";
}

// =========================================================================
// 8.3 短路径 + Win32 原生 A 版 API 纯 UTF-8 INI 读写实现
// 零手写解析器，100% 免疫中文路径与中文内容转码乱码
// =========================================================================

inline StrA _取短路径A(c_StrX 路径)
{
	StrW wPath = 取绝对路径(路径);
	// 如果文件尚不存在，先以读写方式打开以确保 Windows 文件系统为其生成 8.3 短文件名别名
	HANDLE h = CreateFileW(wPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		CloseHandle(h);
	}
	WCHAR wShort[MAX_PATH] = { 0 };
	if (GetShortPathNameW(wPath, wShort, MAX_PATH) > 0) {
		char szShort[MAX_PATH] = { 0 };
		WideCharToMultiByte(CP_ACP, 0, wShort, -1, szShort, MAX_PATH, NULL, NULL);
		return StrA(szShort);
	}
	char szAnsi[MAX_PATH] = { 0 };
	WideCharToMultiByte(CP_ACP, 0, (const wchar_t*)wPath, -1, szAnsi, MAX_PATH, NULL, NULL);
	return StrA(szAnsi);
}

/** 读配置项_U8 (纯 UTF-8 字节直通，支持任意语言文件名与节名/键值) */
inline StrA 读配置项_U8(c_StrA 配置文件名, c_StrA 节名称, c_StrA 配置项名称, c_StrA 默认文本 = "")
{
	StrA shortPath = _取短路径A(配置文件名);
	DWORD size = 2048;
	StrA buf(size);
	while (true) {
		DWORD ret = GetPrivateProfileStringA((const char*)节名称, (const char*)配置项名称, (const char*)默认文本, (char*)buf, size, (const char*)shortPath);
		if (ret < size - 1) {
			return StrA((const char*)buf, ret);
		}
		buf.reset(size *= 2);
	}
}

/** 写配置项_U8 (纯 UTF-8 字节直通，支持任意语言文件名与节名/键值) */
inline bool 写配置项_U8(c_StrA 配置文件名, c_StrA 节名称, NilOpt<c_StrA> 配置项名称 = nil, NilOpt<c_StrA> 欲写入值 = nil)
{
	StrA shortPath = _取短路径A(配置文件名);
	const char* lpKey = (配置项名称 != nil) ? (const char*)(const StrA&)配置项名称 : NULL;
	const char* lpString = (欲写入值 != nil) ? (const char*)(const StrA&)欲写入值 : NULL;
	return WritePrivateProfileStringA((const char*)节名称, lpKey, lpString, (const char*)shortPath) != 0;
}

/** 取配置节名_U8 (纯 UTF-8 字节直通，支持任意语言文件名与节名) */
inline Arraybe<StrA> 取配置节名_U8(c_StrA 配置文件名)
{
	Arraybe<StrA> res;
	StrA shortPath = _取短路径A(配置文件名);
	DWORD size = 8192;
	StrA buf(size);
	while (true) {
		DWORD ret = GetPrivateProfileSectionNamesA((char*)buf, size, (const char*)shortPath);
		if (ret < size - 2) {
			char* p = (char*)buf;
			while (p < (char*)buf + ret && *p) {
				res.push(StrA(p));
				p += strlen(p) + 1;
			}
			break;
		}
		size *= 2;
		buf.reset(size);
	}
	return res;
}

/** 取配置项名_U8 (纯 UTF-8 字节直通，支持任意语言文件名与键名) */
inline Arraybe<StrA> 取配置项名_U8(c_StrA 配置文件名, c_StrA 节名称)
{
	Arraybe<StrA> res;
	StrA shortPath = _取短路径A(配置文件名);
	DWORD size = 8192;
	StrA buf(size);
	while (true) {
		DWORD ret = GetPrivateProfileSectionA((const char*)节名称, (char*)buf, size, (const char*)shortPath);
		if (ret < size - 2) {
			char* p = (char*)buf;
			while (p < (char*)buf + ret && *p) {
				const char* eq = strchr(p, '=');
				if (eq) {
					res.push(StrA(p, eq - p));
				} else {
					res.push(StrA(p));
				}
				p += strlen(p) + 1;
			}
			break;
		}
		size *= 2;
		buf.reset(size);
	}
	return res;
}

} // namespace common

using common::读配置项_U8;
using common::写配置项_U8;
using common::取配置节名_U8;
using common::取配置项名_U8;