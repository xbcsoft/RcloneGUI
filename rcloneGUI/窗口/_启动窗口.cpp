#include <BEMod.h>
#include "../resource.h"
#include "../common.hpp"
#include "../RcloneService.h"

// Rebuilt cleanly v7
struct __启动窗口 : 窗口
{
	SciterDom dom;
	RcloneService& svc = RcloneService::Instance();

	struct _st : SciterUI {
		UINT 事件_资源加载(LPSCN_LOAD_DATA pld) override
		{
			StrW uri = pld->uri;
			if (文本_对比右边<W>(uri, L"/app-icon.png")) {
				static Bytes appIconPng = ExtractAppIconAsPng();
				//coutlogR(appIconPng, false, "png");
				if (appIconPng.size > 0) {
					return 资源返回(pld, appIconPng);
				}
			}

			ssize_t idx = 倒找文本(uri, L"/lang/");
			if (idx >= 0) {
				StrW fName = 取文本右边(uri, uri.len() - idx - 6);
				return 资源返回(pld, 读入文件(_启动窗口.svc.取语言目录() + "/" + WtoU8(fName)));
			}
			return LOAD_OK;
		}
	} st;

	void 事件_创建完毕();

	void 载入(窗口* 父窗 = 0, bool 模态 = 0);
	void 完毕(bool 模态);

	static SciterObj DriveConfigToSciterObj(const DriveConfig& d)
	{
		SciterObj obj;
		obj.置属性("name", (StrU8&)d.name);
		obj.置属性("protocol", (StrU8&)d.protocol);
		obj.置属性("letter", (StrU8&)d.letter);
		obj.置属性("host", (StrU8&)d.host);
		obj.置属性("port", (StrU8&)d.port);
		obj.置属性("path", (StrU8&)d.path);
		obj.置属性("username", (StrU8&)d.username);
		obj.置属性("password", (StrU8&)d.password);
		obj.置属性("isAnonymous", d.isAnonymous);
		obj.置属性("isSSL", d.isSSL);
		obj.置属性("isReconnect", d.isReconnect);
		obj.置属性("isLocalDisk", d.isLocalDisk);
		obj.置属性("isReadOnly", d.isReadOnly);
		obj.置属性("isFileLock", d.isFileLock);
		obj.置属性("privateKey", (StrU8&)d.privateKey);
		obj.置属性("charset", (StrU8&)d.charset);
		obj.置属性("isPassive", d.isPassive);
		obj.置属性("isExplicit", d.isExplicit);
		obj.置属性("status", (StrU8&)d.status);
		obj.置属性("pid", (int)d.pid);
		obj.置属性("errorMsg", (StrU8&)d.errorMsg);
		return obj;
	}

	static DriveConfig SciterObjToDriveConfig(const SciterObj& arg)
	{
		DriveConfig d;
		d.name = arg.取属性("name");
		d.protocol = arg.取属性("protocol");
		d.letter = arg.取属性("letter");
		d.host = arg.取属性("host");
		d.port = arg.取属性("port");
		d.path = arg.取属性("path");
		d.username = arg.取属性("username");
		d.password = arg.取属性("password");
		d.isAnonymous = arg.取属性("isAnonymous");
		d.isSSL = arg.取属性("isSSL");
		d.isReconnect = arg.取属性("isReconnect");
		d.isLocalDisk = arg.取属性("isLocalDisk");
		d.isReadOnly = arg.取属性("isReadOnly");
		d.isFileLock = arg.取属性("isFileLock");
		d.privateKey = arg.取属性("privateKey");
		d.charset = arg.取属性("charset");
		d.isPassive = arg.取属性("isPassive");
		d.isExplicit = arg.取属性("isExplicit");
		d.status = arg.取属性("status");
		if (!d.status) d.status = "disconnected";
		return d;
	}
private:
	static Bytes ExtractAppIconAsPng()
	{
		HMODULE hMod = GetModuleHandleW(NULL);
		Bytes pngBytes;
		EnumResourceNamesW(hMod, RT_ICON, [](HMODULE hModule, LPCWSTR lpType,
			LPWSTR lpName, LONG_PTR lParam) -> BOOL
		{
			HRSRC hr = FindResourceW(hModule, lpName, lpType);
			if (hr) {
				DWORD sz = SizeofResource(hModule, hr);
				HGLOBAL hg = LoadResource(hModule, hr);
				if (hg) {
					const BYTE* ptr = (const BYTE*)LockResource(hg);
					if (ptr && sz >= 8) {
						// 命中内嵌的 PNG 格式图标帧 (\x89PNG)
						if (ptr[0] == 0x89 && ptr[1] == 'P' && ptr[2] == 'N' && ptr[3] == 'G') {
							((Bytes*)lParam)->ref(ptr, sz);
							return FALSE; // 提取成功，停止枚举
						}
					}
				}
			}
			return TRUE;
		}, (LONG_PTR)&pngBytes);
		return pngBytes;
	}
} _启动窗口;

void __启动窗口::事件_创建完毕()
{
	dbg_log("[__启动窗口] 初始化Sciter接口 called");
	dom = st.取文档模型();

	// 1. 向前端派发驱动器状态变化通知
	svc.注册状态回调([this](c_StrA name, c_StrA status, c_StrA errMsg) {
		dom.调用JS函数("onNativeDriveStatusChange", { (StrU8&)name, (StrU8&)status, (StrU8&)errMsg });
	});

	// 0. 日志注入
	dom.注入JS函数("Native_Log", [this](SciterObj& arg) -> SciterObj {
		StrA msg = arg;
		dbg_log("[JS] %s", (const char*)msg);
		return true;
	});

	// 0.1 异步 HTTP RC 调用注入（后台独立 C++ 线程执行，套接字网络请求绝不阻塞 UI 渲染和动画）
	dom.注入JS函数("Native_CallRcloneRCAsync", [this](SciterObj& arg) -> SciterObj {
		int reqId = arg.取属性("id");
		StrA method = arg.取属性("method");
		StrA body = arg.取属性("body");
		AppSettings s = svc.取设置();
		int port = s.rcPort > 0 ? s.rcPort : 5572;
		StrA user = s.rcUser ? s.rcUser : StrA("admin");
		StrA pass = s.rcPass ? s.rcPass : StrA("admin123");

		线程::启动([this, reqId, port, user, pass, method, body]() {
			dbg_log("[Native_CallRcloneRCAsync] start reqId=%d, method=%s", reqId, (const char*)method);
			StrA resp = RcloneService::HttpPostRc(port, user, pass, method, body);
			dbg_log("[Native_CallRcloneRCAsync] done reqId=%d, resp=%s", reqId, (const char*)resp);
			dom.调用JS函数("onNativeRcloneRCResponse", { reqId, (StrU8&)resp });
		});

		return true;
	});

	// 0.2 同步 HTTP RC 调用注入（备用）
	dom.注入JS函数("Native_CallRcloneRC", [this](SciterObj& arg) -> SciterObj {
		StrA method = arg.取属性("method");
		StrA body = arg.取属性("body");
		AppSettings s = svc.取设置();
		int port = s.rcPort > 0 ? s.rcPort : 5572;
		StrA user = s.rcUser.len() > 0 ? s.rcUser : StrA("admin");
		StrA pass = s.rcPass.len() > 0 ? s.rcPass : StrA("admin123");
		dbg_log("[Native_CallRcloneRC] method=%s, body=%s", (const char*)method, (const char*)body);
		StrA resp = RcloneService::HttpPostRc(port, user, pass, method, body);
		dbg_log("[Native_CallRcloneRC] resp=%s", (const char*)resp);
		return (StrU8&)resp;
	});

	// 0.3 触发后台自动重连挂载
	dom.注入JS函数("Native_AutoMountReconnectDrives", [this](SciterObj& arg) -> SciterObj {
		线程::启动([this]() {
			svc.自动挂载所有重连网盘();
		});
		return true;
	});

	// 3.1 获取可用盘符列表
	dom.注入JS函数("Native_GetAvailableDriveLetters", [this](SciterObj& arg) -> SciterObj {
		auto list = svc.取可用盘符列表();
		SciterObj arr;
		for (int i = 0; i < list.count; ++i) {
			arr.加入成员((StrU8&)list[i]);
		}
		return arr;
	});

	// 3.2 获取所有驱动器列表
	dom.注入JS函数("Native_GetDrivesList", [this](SciterObj& arg) -> SciterObj {
		auto drives = svc.取所有网盘配置();
		SciterObj arr;
		for (int i = 0; i < drives.count; ++i) {
			arr.加入成员(DriveConfigToSciterObj(drives[i]));
		}
		return arr;
	});

	// 3.3 新建/保存驱动器
	dom.注入JS函数("Native_SaveDrive", [this](SciterObj& arg) -> SciterObj {
		DriveConfig cfg = SciterObjToDriveConfig(arg);
		dbg_log("[Native_SaveDrive] 解析后 cfg.name='%s', cfg.letter='%s', cfg.host='%s'", (const char*)cfg.name, (const char*)cfg.letter, (const char*)cfg.host);

		StrA err;
		bool ok = svc.保存网盘配置(cfg, err);
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		return res;
	});

	// 3.4 删除驱动器
	dom.注入JS函数("Native_DeleteDrive", [this](SciterObj& arg) -> SciterObj {
		StrA name = arg;
		StrA err;
		bool ok = svc.删除网盘配置(name, err);
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		return res;
	});

	// 3.5 挂载驱动器
	dom.注入JS函数("Native_MountDrive", [this](SciterObj& arg) -> SciterObj {
		StrA name = arg;
		StrA err;
		bool ok = svc.挂载驱动器(name, err);
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		return res;
	});

	// 3.6 卸载驱动器
	dom.注入JS函数("Native_UnmountDrive", [this](SciterObj& arg) -> SciterObj {
		StrA name = arg;
		StrA err;
		bool ok = svc.卸载驱动器(name, err);
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		return res;
	});

	// 3.7 在资源管理器中打开驱动器
	dom.注入JS函数("Native_OpenDrive", [this](SciterObj& arg) -> SciterObj {
		StrA letter = arg;
		bool ok = svc.打开驱动器(letter);
		return ok;
	});

	// 3.71 获取已挂载驱动器的系统真实分配盘符
	dom.注入JS函数("Native_GetMountedDriveLetter", [this](SciterObj& arg) -> SciterObj {
		StrW name = arg;
		StrW realLetter = svc.取已挂载驱动器真实盘符(name);
		return realLetter;
	});

	// 3.72 获取 GUI 自身文件版本
	dom.注入JS函数("Native_GetGuiVersion", [this](SciterObj& arg) -> SciterObj {
		StrW ver = common::取自身文件版本();
		return ver;
	});

	// 3.8 异步获取驱动器容量使用情况（后台线程异步执行，避免网络 IO 阻塞 UI）
	dom.注入JS函数("Native_GetDriveSpaceAsync", [this](SciterObj& arg) -> SciterObj {
		StrA letter = arg.取属性("letter");
		线程::启动([this, letter]() {
			double totalGB = 0, usedGB = 0, freeGB = 0;
			int usedPercent = 0;
			bool ok = svc.取驱动器空间(letter, totalGB, usedGB, freeGB, usedPercent);
			SciterObj res;
			res.置属性("success", ok);
			res.置属性("totalGB", totalGB);
			res.置属性("usedGB", usedGB);
			res.置属性("freeGB", freeGB);
			res.置属性("usedPercent", usedPercent);
			dom.调用JS函数("onNativeGetDriveSpaceResponse", { (StrU8&)letter, res });
		});
		return true;
	});

	// 3.8.1 同步获取驱动器容量使用情况（备用）
	dom.注入JS函数("Native_GetDriveSpace", [this](SciterObj& arg) -> SciterObj {
		StrA letter = arg;
		double totalGB = 0, usedGB = 0, freeGB = 0;
		int usedPercent = 0;
		bool ok = svc.取驱动器空间(letter, totalGB, usedGB, freeGB, usedPercent);
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("totalGB", totalGB);
		res.置属性("usedGB", usedGB);
		res.置属性("freeGB", freeGB);
		res.置属性("usedPercent", usedPercent);
		return res;
	});

	// 3.9 浏览私钥文件
	dom.注入JS函数("Native_BrowsePrivateKey", [this](SciterObj& arg) -> SciterObj {
		StrA path = svc.浏览私钥文件((HWND)窗口句柄);
		return (StrU8&)path;
	});

	// 3.10 浏览文件夹
	dom.注入JS函数("Native_BrowseFolder", [this](SciterObj& arg) -> SciterObj {
		StrA path = common::浏览文件夹("请选择缓存文件夹", "", true, true, false, (HWND)窗口句柄);
		return (StrU8&)path;
	});

	// 3.11 获取设置
	dom.注入JS函数("Native_GetSettings", [this](SciterObj& arg) -> SciterObj {
		AppSettings s = svc.取设置();
		SciterObj obj;
		obj.置属性("autoStart", s.autoStart);
		obj.置属性("autoStartNoGUI", s.autoStartNoGUI);
		obj.置属性("openExplorerOnConnect", s.openExplorerOnConnect);
		obj.置属性("language", (StrU8&)s.language);
		obj.置属性("cachePath", (StrU8&)s.cachePath);
		obj.置属性("rcPort", s.rcPort);
		obj.置属性("rcUser", (StrU8&)s.rcUser);
		obj.置属性("rcPass", (StrU8&)s.rcPass);
		return obj;
	});

	// 3.12 保存设置
	dom.注入JS函数("Native_SaveSettings", [this](SciterObj& arg) -> SciterObj {
		AppSettings s;
		s.autoStart = arg.取属性("autoStart");
		s.autoStartNoGUI = arg.取属性("autoStartNoGUI");
		s.openExplorerOnConnect = arg.取属性("openExplorerOnConnect");
		s.language = arg.取属性("language");
		if (s.language.len() == 0) s.language = "zh";
		s.cachePath = arg.取属性("cachePath");
		s.rcPort = arg.取属性("rcPort");
		if (s.rcPort <= 0) s.rcPort = 5572;
		s.rcUser = arg.取属性("rcUser");
		if (s.rcUser.len() == 0) s.rcUser = "admin";
		s.rcPass = arg.取属性("rcPass");
		if (s.rcPass.len() == 0) s.rcPass = "admin123";

		bool ok = svc.保存设置(s);
		return ok;
	});

	// 3.13 获取 Rclone 版本
	dom.注入JS函数("Native_GetRcloneVersion", [this](SciterObj& arg) -> SciterObj {
		StrA ver = svc.取Rclone版本();
		return (StrU8&)ver;
	});

	// 3.14 检测 WinFsp
	dom.注入JS函数("Native_CheckWinFsp", [this](SciterObj& arg) -> SciterObj {
		bool bInstalled = svc.检测WinFsp已安装();
		return bInstalled;
	});

	// 3.15 安装 WinFsp
	dom.注入JS函数("Native_InstallWinFsp", [this](SciterObj& arg) -> SciterObj {
		bool ok = svc.安装WinFsp((HWND)窗口句柄);
		return ok;
	});

	// 3.16 安装系统服务
	dom.注入JS函数("Native_InstallService", [this](SciterObj& arg) -> SciterObj {
		StrA err;
		bool ok = svc.安装系统服务(err);
		int status = svc.取系统服务状态();
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		res.置属性("isInstalled", status != 0);
		res.置属性("isRunning", status == 1);
		res.置属性("statusCode", status);
		if (ok) {
			MessageBoxW((HWND)窗口句柄, L"安装成功", L"系统服务", MB_OK | MB_ICONINFORMATION);
		} else {
			StrW wErr = err.len() > 0 ? (StrW)U8toW(err) : StrW(L"安装失败，可能无管理员权限");
			MessageBoxW((HWND)窗口句柄, (const wchar_t*)wErr, L"系统服务", MB_OK | MB_ICONERROR);
		}
		return res;
	});

	// 3.17 卸载系统服务
	dom.注入JS函数("Native_UninstallService", [this](SciterObj& arg) -> SciterObj {
		StrA err;
		bool ok = svc.卸载系统服务(err);
		int status = svc.取系统服务状态();
		SciterObj res;
		res.置属性("success", ok);
		res.置属性("error", (StrU8&)err);
		res.置属性("isInstalled", status != 0);
		res.置属性("isRunning", status == 1);
		res.置属性("statusCode", status);
		if (ok) {
			MessageBoxW((HWND)窗口句柄, L"卸载成功", L"系统服务", MB_OK | MB_ICONINFORMATION);
		} else {
			StrW wErr = err.len() > 0 ? (StrW)U8toW(err) : StrW(L"卸载失败，可能无管理员权限");
			MessageBoxW((HWND)窗口句柄, (const wchar_t*)wErr, L"系统服务", MB_OK | MB_ICONERROR);
		}
		return res;
	});

	// 3.18 检测系统服务状态
	dom.注入JS函数("Native_GetServiceStatus", [this](SciterObj& arg) -> SciterObj {
		int status = svc.取系统服务状态();
		SciterObj res;
		res.置属性("isInstalled", status != 0);
		res.置属性("isRunning", status == 1);
		res.置属性("statusCode", status);
		return res;
	});

	// 3.19 枚举语言包文件列表
	dom.注入JS函数("Native_GetLanguageFiles", [this](SciterObj& arg) -> SciterObj {
		StrA langDir = svc.取语言目录();
		Arraybe<StrW> files = 文件_枚举<false, true>(langDir, false);
		SciterObj res;
		for (int i = 0; i < files.count; ++i) {
			StrA fName = WtoU8(files[i]);
			if (fName.len() > 3 && 到小写(取文本右边(fName, 3)) == ".js") {
				if (到小写(fName) == "index.js") continue;
				res.加入成员((StrU8&)fName);
			}
		}
		return res;
	});

	// 4. 触发前端初始化数据加载
	dom.调用JS函数("initApp");
}
