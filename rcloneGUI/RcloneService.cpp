#include <BEMod.h>
#include "common.hpp"

struct DriveConfig
{
	StrA name;
	StrA protocol;       // WebDAV / SFTP / FTP
	StrA letter;         // e.g. "Z:"
	StrA host;
	StrA port;
	StrA path;
	StrA username;
	StrA password;
	bool isAnonymous = false;
	bool isSSL = true;
	bool isReconnect = true;
	bool isLocalDisk = false;
	bool isReadOnly = false;
	bool isFileLock = false;
	StrA privateKey;
	StrA charset;
	bool isPassive = true;
	bool isExplicit = true;
	StrA status = "disconnected"; // disconnected, connecting, connected, error
	DWORD pid = 0;
	StrA errorMsg;
};

struct AppSettings
{
	bool autoStart = false; // 开机自启(带GUI)
	bool autoStartNoGUI = false; // 开机自启(无GUI/run)
	bool openExplorerOnConnect = false;
	StrA language = "zh";
	StrA cachePath = "C:\\ProgramData\\rcloneGUI\\Cache";
	int rcPort = 5572;
	StrA rcUser = "admin";
	StrA rcPass = "admin123";
};

struct EXP RcloneService
{
public:
	static RcloneService& Instance()
	{
		static RcloneService s_instance;
		return s_instance;
	}

	RcloneService()
	{
		//_appDir = 选择(DBG, (StrA&)取当前目录(), 路径_去文件名(取模块路径()));
		_appDir = 路径_去文件名(取模块路径());
		_rcloneExePath = _appDir + "\\rclone.core.exe";
		_configFilePath = _appDir + "\\rclone.conf";
		_settingsFilePath = _appDir + "\\settings.ini";

		加载设置();
		加载本地驱动器配置();

		// 检查指定端口是否已处于监听状态 (Winsock TCP Check)...
		dbg_log("[RcloneService] 检测 RC 端口 %d", _settings.rcPort);
		if (!IsPortListening(_settings.rcPort)) {
			dbg_log("[RcloneService] 端口 %d 未监听，正在后台拉起 rclone.core.exe 守护进程...", _settings.rcPort);
			bool bStarted = StartRcloneDaemon(_rcloneExePath, _configFilePath, _settings.rcPort, _settings.rcUser, _settings.rcPass, _appDir);
			if (bStarted) {
				dbg_log("[RcloneService] 守护进程拉起成功！端口=%d, 用户名=%s", _settings.rcPort, (const char*)_settings.rcUser);
			} else {
				dbg_log("[RcloneService] 守护进程拉起失败！");
			}
		} else {
			dbg_log("[RcloneService] 端口 %d 已处于监听状态，无需重复启动守护进程。", _settings.rcPort);
		}
	}

	~RcloneService()
	{
		自动锁 lock(_mutex);
		for (auto& node : _activeMounts) {
			if (node.value.hProcess) {
				TerminateProcess(node.value.hProcess, 0);
				CloseHandle(node.value.hProcess);
				if (node.value.hThread) CloseHandle(node.value.hThread);
			}
		}
		_activeMounts.clear();
	}

	StrA 取运行目录()
	{
		return _appDir;
	}

	StrA 取Rclone路径()
	{
		return _rcloneExePath;
	}

	StrA 取配置文件路径()
	{
		return _configFilePath;
	}

	StrA 取设置文件路径()
	{
		return _settingsFilePath;
	}

	StrA 取语言目录()
	{
		return _appDir + "\\lang";
	}

	StrA 取Rclone版本()
	{
		return "rclone v1.70.3";
	}

	Arraybe<StrA> 取可用盘符列表()
	{
		Arraybe<StrA> list;
		list.push("Auto");
		DWORD dwDrives = GetLogicalDrives();
		for (char c = 'Z'; c >= 'D'; --c) {
			int shift = c - 'A';
			if (!(dwDrives & (1 << shift))) {
				char szLetter[3] = { c, ':', '\0' };
				list.push(szLetter);
			}
		}
		return list;
	}

	Arraybe<DriveConfig> 取所有网盘配置()
	{
		自动锁 lock(_mutex);
		dbg_log("[RcloneService] 取所有网盘配置, 当前 count=%d", _drives.count);
		return _drives;
	}

	bool 保存网盘配置(const DriveConfig& config, StrA& outErr)
	{
		if (config.name.len() == 0) {
			outErr = "驱动器名称不能为空";
			return false;
		}

		DriveConfig finalConfig = config;

		{
			自动锁 lock(_mutex);
			bool found = false;
			for (int i = 0; i < _drives.count; ++i) {
				if (_drives[i].name == config.name) {
					DriveConfig updated = config;
					updated.status = _drives[i].status;
					updated.pid = _drives[i].pid;
					_drives[i] = updated;
					found = true;
					break;
				}
			}
			if (!found) {
				StrA baseName = config.name;
				StrA uniqueName = baseName;
				int counter = 2;
				bool exists = true;
				while (exists) {
					exists = false;
					for (int k = 0; k < _drives.count; ++k) {
						if (_drives[k].name == uniqueName) {
							exists = true;
							break;
						}
					}
					if (exists) {
						uniqueName = baseName + " " + 到文本(counter);
						counter++;
					}
				}
				finalConfig.name = uniqueName;
				_drives.push(finalConfig);
			}
		}

		StrA sec = finalConfig.name;
		StrA protoType = (finalConfig.protocol == "SFTP") ? "sftp" : ((finalConfig.protocol == "FTP") ? "ftp" : "webdav");

		写配置项_U8(_configFilePath, sec, "type", protoType);
		写配置项_U8(_configFilePath, sec, "host", finalConfig.host);
		写配置项_U8(_configFilePath, sec, "port", finalConfig.port);
		写配置项_U8(_configFilePath, sec, "letter", finalConfig.letter);
		写配置项_U8(_configFilePath, sec, "user", finalConfig.username);
		写配置项_U8(_configFilePath, sec, "pass", finalConfig.password);
		写配置项_U8(_configFilePath, sec, "isReconnect", finalConfig.isReconnect ? "true" : "false");
		写配置项_U8(_configFilePath, sec, "isLocalDisk", finalConfig.isLocalDisk ? "true" : "false");
		写配置项_U8(_configFilePath, sec, "isReadOnly", finalConfig.isReadOnly ? "true" : "false");
		写配置项_U8(_configFilePath, sec, "isFileLock", finalConfig.isFileLock ? "true" : "false");

		if (finalConfig.path.len() > 0 && finalConfig.path != "undefined") {
			写配置项_U8(_configFilePath, sec, "path", finalConfig.path);
		}
		if (finalConfig.privateKey.len() > 0 && finalConfig.privateKey != "undefined") {
			写配置项_U8(_configFilePath, sec, "key_file", finalConfig.privateKey);
		}
		if (finalConfig.charset.len() > 0 && finalConfig.charset != "undefined") {
			写配置项_U8(_configFilePath, sec, "charset", finalConfig.charset);
		}

		if (finalConfig.protocol == "WebDAV") {
			StrA url = (finalConfig.isSSL ? "https://" : "http://") + finalConfig.host;
			if (finalConfig.port.len() > 0) url += ":" + finalConfig.port;
			if (finalConfig.path.len() > 0 && finalConfig.path != "undefined") {
				if (finalConfig.path[0] == '/') url += finalConfig.path;
				else url += "/" + finalConfig.path;
			}
			写配置项_U8(_configFilePath, sec, "url", url);
			写配置项_U8(_configFilePath, sec, "vendor", "other");
			if (finalConfig.isSSL) {
				写配置项_U8(_configFilePath, sec, "no_check_certificate", "true");
			}
		}
		else if (finalConfig.protocol == "FTP") {
			if (finalConfig.isSSL) {
				写配置项_U8(_configFilePath, sec, "tls", "true");
				写配置项_U8(_configFilePath, sec, "explicit_tls", "false");
				写配置项_U8(_configFilePath, sec, "no_check_certificate", "true");
			} else if (finalConfig.isExplicit) {
				写配置项_U8(_configFilePath, sec, "explicit_tls", "true");
				写配置项_U8(_configFilePath, sec, "tls", "false");
				写配置项_U8(_configFilePath, sec, "no_check_certificate", "true");
			} else {
				写配置项_U8(_configFilePath, sec, "tls", "false");
				写配置项_U8(_configFilePath, sec, "explicit_tls", "false");
			}
			if (finalConfig.isPassive) {
				写配置项_U8(_configFilePath, sec, "pass_mode", "passive");
			}
			写配置项_U8(_configFilePath, sec, "disable_mlsd", "true");
		}

		dbg_log("[保存网盘配置] 已直接写入 INI rclone.conf: sec='%s', letter='%s'", (const char*)sec, (const char*)finalConfig.letter);
		{
			自动锁 lock(_mutex);
			保存驱动器排序();
		}
		return true;
	}

	bool 删除网盘配置(c_StrA name, StrA& outErr)
	{
		卸载驱动器(name, outErr);

		写配置项_U8(_configFilePath, name, nil, nil);

		{
			自动锁 lock(_mutex);
			for (int i = 0; i < _drives.count; ++i) {
				if (_drives[i].name == name) {
					_drives.del(i);
					break;
				}
			}
			保存驱动器排序();
		}
		return true;
	}

	bool 检测WinFsp已安装()
	{
		// 1. 检查注册表
		HKEY hKey = NULL;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WinFsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			RegCloseKey(hKey);
			return true;
		}
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\WinFsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			RegCloseKey(hKey);
			return true;
		}
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\winfsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			RegCloseKey(hKey);
			return true;
		}

		// 2. 检查安装目录
		char szProgFiles[MAX_PATH] = { 0 };
		if (GetEnvironmentVariableA("ProgramFiles", szProgFiles, MAX_PATH)) {
			StrA p = StrA(szProgFiles) + "\\WinFsp\\bin\\winfsp-x64.dll";
			if (文件是否存在(p)) return true;
		}
		if (GetEnvironmentVariableA("ProgramFiles(x86)", szProgFiles, MAX_PATH)) {
			StrA p = StrA(szProgFiles) + "\\WinFsp\\bin\\winfsp-x86.dll";
			if (文件是否存在(p)) return true;
		}
		return false;
	}
	bool 安装WinFsp(HWND hParent = NULL)
	{
		StrA msiPath = _appDir + "\\winfsp-2.1.25156.msi";
		if (!文件是否存在(msiPath)) {
			msiPath = _appDir + "\\..\\winfsp-2.1.25156.msi";
			if (!文件是否存在(msiPath)) {
				msiPath = "winfsp-2.1.25156.msi";
			}
		}
		HINSTANCE hInst = ShellExecuteA(hParent, "open", "msiexec.exe", ("/i \"" + msiPath + "\""), NULL, SW_SHOWNORMAL);
		return (INT_PTR)hInst > 32;
	}

	bool 挂载驱动器(c_StrA name, StrA& errMsg)
	{
		return true;
	}

	bool 卸载驱动器(c_StrA name, StrA& outErr)
	{
		触发状态回调(name, "disconnected", "");
		return true;
	}

	bool 打开驱动器(c_StrA letter)
	{
		if (letter.len() == 0) return false;
		StrA path = letter;
		if (path[path.len() - 1] != '\\') path += "\\";
		HINSTANCE hInst = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
		return (INT_PTR)hInst > 32;
	}

	bool 取驱动器空间(c_StrA letter, double& totalGB, double& usedGB, double& freeGB, int& usedPercent)
	{
		totalGB = usedGB = freeGB = 0;
		usedPercent = 0;
		if (letter.len() == 0) return false;

		StrA rootPath = letter;
		if (rootPath[rootPath.len() - 1] != '\\') rootPath += "\\";

		ULARGE_INTEGER freeBytesAvailableToCaller = { 0 };
		ULARGE_INTEGER totalNumberOfBytes = { 0 };
		ULARGE_INTEGER totalNumberOfFreeBytes = { 0 };

		if (GetDiskFreeSpaceExA(rootPath, &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
			totalGB = (double)totalNumberOfBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
			freeGB = (double)totalNumberOfFreeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
			usedGB = totalGB - freeGB;
			if (totalGB > 0) {
				usedPercent = (int)((usedGB / totalGB) * 100.0);
			}
			return true;
		}
		return false;
	}

	StrW 取已挂载驱动器真实盘符(c_StrW driveName)
	{
		return common::取已挂载驱动器真实盘符(driveName);
	}

	// 0: 未开启自启, 1: 开机自启(带GUI), 2: 开机自启(无GUI/service)
	int 取自启动模式()
	{
		StrW val = 读注册表文本(L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\rcloneGUI");
		if (val.len() == 0) return 0;
		if (倒找文本(val, L"service") >= 0) {
			return 2; // 无GUI模式
		}
		return 1; // 带GUI模式
	}

	void 设置自启动模式(int mode)
	{
		if (mode == 1) {
			自动开机::设置(L"rcloneGUI", 取模块路径(), L"");
		} else if (mode == 2) {
			自动开机::设置(L"rcloneGUI", 取模块路径(), L"service");
		} else {
			自动开机::删除(L"rcloneGUI", false);
			自动开机::删除(L"rcloneGUI", true);
		}
	}

	AppSettings 取设置()
	{
		自动锁 lock(_mutex);
		int mode = 取自启动模式();
		_settings.autoStart = (mode == 1);
		_settings.autoStartNoGUI = (mode == 2);
		return _settings;
	}

	bool 保存设置(const AppSettings& settings)
	{
		自动锁 lock(_mutex);
		_settings = settings;
		_settingsFilePath = _appDir + "\\settings.ini";

		写配置项_U8(_settingsFilePath, "Settings", "openExplorerOnConnect", settings.openExplorerOnConnect ? "true" : "false");
		写配置项_U8(_settingsFilePath, "Settings", "language", settings.language);
		写配置项_U8(_settingsFilePath, "Settings", "cachePath", settings.cachePath);
		写配置项_U8(_settingsFilePath, "Settings", "rcPort", 到文本(settings.rcPort));
		写配置项_U8(_settingsFilePath, "Settings", "rcUser", settings.rcUser);
		写配置项_U8(_settingsFilePath, "Settings", "rcPass", settings.rcPass);

		if (settings.autoStartNoGUI) {
			设置自启动模式(2);
		} else if (settings.autoStart) {
			设置自启动模式(1);
		} else {
			设置自启动模式(0);
		}

		int mode = 取自启动模式();
		_settings.autoStart = (mode == 1);
		_settings.autoStartNoGUI = (mode == 2);
		return true;
	}

	bool 直接安装系统服务()
	{
		StrA errMsg;
		dbg_log("[RcloneService] [直接安装系统服务] 正在执行...");
		// 1. 先关闭所有可能正在运行的 rclone.core.exe 进程
		系统服务::结束所有进程(L"rclone.core.exe");

		// 2. 获取当前程序执行路径并构造服务参数
		StrW binPath = L"\"" + (StrW)取模块路径() + L"\" service";

		// 3. 注册安装系统服务 (2 = SERVICE_AUTO_START 自启动)
		int ret = 系统服务::安装(
			L"rcloneGUIService",
			binPath,
			2,
			L"rcloneGUI 服务",
			L"rcloneGUI 后台挂载守护服务"
		);

		if (ret != 1 && ret != ERROR_SERVICE_EXISTS) {
			errMsg = "安装失败 (错误码:" + 到文本(ret) + ")";
			dbg_log("[RcloneService] 系统服务安装失败: %s", (const char*)errMsg);
			return false;
		}

		// 4. 启动服务
		int startRet = 系统服务::启动(L"rcloneGUIService", 5000, true);
		dbg_log("[RcloneService] 系统服务启动状态: %d", startRet);
		return true;
	}

	bool 直接卸载系统服务()
	{
		StrA errMsg;
		dbg_log("[RcloneService] [直接卸载系统服务] 正在执行...");
		// 1. 关掉所有 rclone.core.exe 进程
		系统服务::结束所有进程(L"rclone.core.exe");

		// 2. 删除服务 (系统服务::删除 会先尝试停止服务，再删除)
		int ret = 系统服务::删除(L"rcloneGUIService", 5000, true);

		// 3. 再次确保杀掉所有残留的 rclone.core.exe
		系统服务::结束所有进程(L"rclone.core.exe");

		if (ret != 1 && ret != 0) {
			errMsg = "卸载失败 (错误码:" + 到文本(ret) + ")";
			dbg_log("[RcloneService] 系统服务卸载失败: %s", (const char*)errMsg);
			return false;
		}

		dbg_log("[RcloneService] 系统服务卸载成功");
		return true;
	}

	bool 安装系统服务(StrA& errMsg)
	{
		dbg_log("[RcloneService] 准备以管理员权限安装系统服务...");
		// 1. 先关闭所有正在运行的 rclone.core.exe
		系统服务::结束所有进程(L"rclone.core.exe");

		// 2. 以管理员权限运行当前程序执行 install 命令行 (隐藏窗口并等待完成)
		bool runOk = 运行(取模块路径(), "install", true, 1, true);
		if (!runOk && !检测系统服务已安装()) {
			errMsg = "请求管理员权限被取消或执行失败";
			dbg_log("[RcloneService] 运行管理员安装进程失败");
			return false;
		}

		if (!检测系统服务已安装()) {
			errMsg = "系统服务安装未成功";
			return false;
		}

		return true;
	}

	bool 卸载系统服务(StrA& errMsg)
	{
		dbg_log("[RcloneService] 准备以管理员权限卸载系统服务...");
		// 1. 先关闭所有正在运行的 rclone.core.exe
		系统服务::结束所有进程(L"rclone.core.exe");

		// 2. 以管理员权限运行当前程序执行 uninstall 命令行 (隐藏窗口并等待完成)
		bool runOk = 运行(取模块路径(), "uninstall", true, 1, true);
		if (!runOk && 检测系统服务已安装()) {
			errMsg = "请求管理员权限被取消或执行失败";
			dbg_log("[RcloneService] 运行管理员卸载进程失败");
			return false;
		}

		if (检测系统服务已安装()) {
			errMsg = "系统服务卸载未成功";
			return false;
		}

		return true;
	}

	bool 检测系统服务已安装()
	{
		return 系统服务::是否存在(L"rcloneGUIService");
	}

	int 取系统服务状态()
	{
		return 系统服务::取系统服务状态(L"rcloneGUIService");
	}

	static StrA HttpPostRc(int port, const StrA& user, const StrA& pass, const StrA& method, const StrA& body)
	{
		HTTP原始报客户端 client;
		StrA credentials = user + ":" + pass;
		StrA auth = BEencode::Base64(credentials.bytes);
		StrA header = "POST /" + method + " HTTP/1.1\r\n";
		header += "Host: 127.0.0.1:" + 到文本(port) + "\r\n";
		header += "Authorization: Basic " + auth + "\r\n";
		header += "Content-Type: application/json\r\n";
		header += "Connection: close\r\n\r\n";

		if (!client.HTTP连接(header, 15000)) {
			return "{\"error\":\"connection to rclone daemon refused\"}";
		}

		Bytes bodyBytes((const char*)body, body.len());
		if (!client.HTTP发送(header, bodyBytes, 15000)) {
			client.断开();
			return "{\"error\":\"send failed\"}";
		}

		StrA respHeader;
		int statusCode = 0;
		Bytes respBody = client.HTTP接收(&respHeader, &statusCode, 15000);
		client.断开();

		if (respBody.size > 0) {
			return StrA((const char*)respBody.buf, respBody.size);
		}
		if (statusCode == 200) {
			return "{}";
		}
		return "{\"error\":\"HTTP request failed or timeout\"}";
	}

	void 自动挂载所有重连网盘()
	{
		dbg_log("[RcloneService] [自动挂载] 开始执行后台网盘自动挂载...");
		int port = _settings.rcPort > 0 ? _settings.rcPort : 5572;
		StrA user = _settings.rcUser.len() > 0 ? _settings.rcUser : StrA("admin");
		StrA pass = _settings.rcPass.len() > 0 ? _settings.rcPass : StrA("admin123");

		// 等待 RC 端口就绪（最多等待 10 秒）
		for (int k = 0; k < 30; ++k) {
			if (IsPortListening(port)) break;
			Sleep(300);
		}

		自动锁 lock(_mutex);
		for (int i = 0; i < _drives.count; ++i) {
			const DriveConfig& d = _drives[i];
			if (!d.isReconnect) {
				dbg_log("[RcloneService] [自动挂载] 跳过未勾选自动重连网盘: %s", (const char*)d.name);
				continue;
			}

			dbg_log("[RcloneService] [自动挂载] 正在挂载: name=%s, letter=%s", (const char*)d.name, (const char*)d.letter);

			StrA cacheDir = _settings.cachePath.len() > 0 ? _settings.cachePath : StrA("C:\\ProgramData\\rcloneGUI\\Cache");
			cacheDir += "\\" + d.name;

			StrA escapedCacheDir = "";
			for (size_t c = 0; c < cacheDir.len(); ++c) {
				if (cacheDir[c] == '\\') {
					escapedCacheDir += "\\\\";
				} else {
					char ch[2] = { cacheDir[c], '\0' };
					escapedCacheDir += ch;
				}
			}

			StrA mountPt = (d.letter == "Auto") ? StrA("*") : d.letter;
			StrA body = "{\"fs\":\"" + d.name + ":\","
				"\"mountPoint\":\"" + mountPt + "\","
				"\"mountOpt\":{"
					"\"VolumeName\":\"" + d.name + "\","
					"\"NetworkMode\":" + (d.isLocalDisk ? "false" : "true") + ","
					"\"AttrTimeout\":1000000000"
				"},"
				"\"vfsOpt\":{"
					"\"CacheMode\":2,"
					"\"Links\":true,"
					"\"ReadOnly\":" + (d.isReadOnly ? "true" : "false") + ","
					"\"DirCacheTime\":3000000000,"
					"\"WriteBack\":0,"
					"\"ChunkSize\":33554432,"
					"\"ChunkSizeLimit\":536870912,"
					"\"ReadAhead\":67108864,"
					"\"CacheDir\":\"" + escapedCacheDir + "\""
				"}}";

			StrA resp = HttpPostRc(port, user, pass, "mount/mount", body);
			dbg_log("[RcloneService] [自动挂载] 挂载结果: name=%s, resp=%s", (const char*)d.name, (const char*)resp);
		}
	}

	StrA 浏览私钥文件(HWND hParent = NULL)
	{
		文件对话框 dlg("选择私钥文件", "所有文件(*.*)|*.*|私钥文件(*.pem;*.key;*.id_rsa)|*.pem;*.key;*.id_rsa", 0, "", true, hParent);
		return dlg.打开();
	}

	typedef be::function<void(c_StrA /*name*/, c_StrA /*status*/, c_StrA /*errMsg*/), 64> StatusCallback;

	void 注册状态回调(StatusCallback cb)
	{
		自动锁 lock(_mutex);
		_statusCallbacks.push(be::move(cb));
	}

private:
	static bool IsPortListening(int port)
	{
		TCP客户端 client;
		return client.连接("127.0.0.1", port, 100);
	}

	static bool StartRcloneDaemon(const StrA& exePath, const StrA& configPath, int port, const StrA& user, const StrA& pass, const StrA& appDir)
	{
		StrA args = "rcd --rc-addr 127.0.0.1:" + 到文本(port) +
			" --rc-user \"" + user + "\" --rc-pass \"" + pass + "\" --rc-no-auth --rc-allow-origin \"*\"" +
			" --contimeout 4s --timeout 8s --low-level-retries 1 --retries 1" +
			" --transfers 8 --checkers 16 --multi-thread-streams 8 --multi-thread-cutoff 10M --buffer-size 32M" +
			" --links --skip-links --config \"" + configPath + "\"";
		return 运行(exePath, args, false, 1);
	}

	static Arraybe<StrA> SplitString(const StrA& str, char delim)
	{
		Arraybe<StrA> result;
		const char* p = (const char*)str;
		if (!p || *p == '\0') return result;
		const char* start = p;
		while (*p) {
			if (*p == delim) {
				if (p > start) {
					result.push(StrA(start, p - start));
				}
				start = p + 1;
			}
			p++;
		}
		if (p > start) {
			result.push(StrA(start, p - start));
		}
		return result;
	}

	void 保存驱动器排序()
	{
		StrA orderStr = "";
		for (int i = 0; i < _drives.count; ++i) {
			if (i > 0) orderStr += ",";
			orderStr += _drives[i].name;
		}
		_settingsFilePath = _appDir + "\\settings.ini";
		写配置项_U8(_settingsFilePath, "Settings", "driveOrder", orderStr);
	}

public:
	void 加载本地驱动器配置()
	{
		dbg_log("[RcloneService] 从 INI 配置文件加载网盘列表: %s", (const char*)_configFilePath);
		自动锁 lock(_mutex);
		_drives.clear();

		Arraybe<StrA> sections = 取配置节名_U8(_configFilePath);
		dbg_log("[RcloneService] 取配置节名 数量=%d", sections.count);

		for (int i = 0; i < sections.count; ++i) {
			StrA sectionName = sections[i];
			if (sectionName.len() == 0) continue;

			DriveConfig d;
			d.name = sectionName;

			StrA typeStr = 读配置项_U8(_configFilePath, sectionName, "type", "");
			if (typeStr == "sftp") d.protocol = "SFTP";
			else if (typeStr == "ftp") d.protocol = "FTP";
			else if (typeStr == "webdav") d.protocol = "WebDAV";
			else d.protocol = typeStr;

			d.host = 读配置项_U8(_configFilePath, sectionName, "host", "");
			d.port = 读配置项_U8(_configFilePath, sectionName, "port", "");
			d.path = 读配置项_U8(_configFilePath, sectionName, "path", "");
			d.username = 读配置项_U8(_configFilePath, sectionName, "user", "");
			d.password = 读配置项_U8(_configFilePath, sectionName, "pass", "");
			d.letter = 读配置项_U8(_configFilePath, sectionName, "letter", "Auto");
			if (d.letter == "*") d.letter = "Auto";

			StrA recStr = 读配置项_U8(_configFilePath, sectionName, "isReconnect", "false");
			d.isReconnect = (recStr == "true" || recStr == "1");

			StrA localStr = 读配置项_U8(_configFilePath, sectionName, "isLocalDisk", "false");
			d.isLocalDisk = (localStr == "true" || localStr == "1");

			StrA roStr = 读配置项_U8(_configFilePath, sectionName, "isReadOnly", "false");
			d.isReadOnly = (roStr == "true" || roStr == "1");

			StrA lockStr = 读配置项_U8(_configFilePath, sectionName, "isFileLock", "false");
			d.isFileLock = (lockStr == "true" || lockStr == "1");

			StrA sslStr = 读配置项_U8(_configFilePath, sectionName, "tls", "false");
			d.isSSL = (sslStr == "true" || sslStr == "1");

			StrA explStr = 读配置项_U8(_configFilePath, sectionName, "explicit_tls", "false");
			d.isExplicit = (explStr == "true" || explStr == "1");

			StrA passvStr = 读配置项_U8(_configFilePath, sectionName, "pass_mode", "passive");
			d.isPassive = (passvStr == "passive" || passvStr == "true" || passvStr == "1");

			d.privateKey = 读配置项_U8(_configFilePath, sectionName, "key_file", "");
			if (d.privateKey == "undefined") d.privateKey = "";
			d.charset = 读配置项_U8(_configFilePath, sectionName, "charset", "utf-8");
			if (d.path == "undefined") d.path = "";

			d.status = "disconnected";
			d.pid = 0;
			d.errorMsg = "";

			_drives.push(d);
			dbg_log("[RcloneService] 读配置项读取网盘成功: name='%s', protocol='%s', letter='%s', host='%s', port='%s'", (const char*)d.name, (const char*)d.protocol, (const char*)d.letter, (const char*)d.host, (const char*)d.port);
		}

		_settingsFilePath = _appDir + "\\settings.ini";
		StrA orderStr = 读配置项_U8(_settingsFilePath, "Settings", "driveOrder", "");
		if (orderStr.len() > 0) {
			Arraybe<StrA> orderList = SplitString(orderStr, ',');
			Arraybe<DriveConfig> sortedDrives;
			for (int k = 0; k < orderList.count; ++k) {
				StrA orderedName = orderList[k];
				for (int i = 0; i < _drives.count; ++i) {
					if (_drives[i].name == orderedName) {
						sortedDrives.push(_drives[i]);
						_drives.del(i);
						break;
					}
				}
			}
			for (int i = 0; i < _drives.count; ++i) {
				sortedDrives.push(_drives[i]);
			}
			_drives = sortedDrives;
			保存驱动器排序();
		} else {
			保存驱动器排序();
		}

		dbg_log("[RcloneService] 共加载 %d 个驱动器 (按固定排序)", _drives.count);
	}

	void 加载设置()
	{
		_settingsFilePath = _appDir + "\\settings.ini";
		int mode = 取自启动模式();
		_settings.autoStart = (mode == 1);
		_settings.autoStartNoGUI = (mode == 2);

		StrA openExpStr = 读配置项_U8(_settingsFilePath, "Settings", "openExplorerOnConnect", "false");
		_settings.openExplorerOnConnect = (openExpStr == "true" || openExpStr == "1");

		_settings.language = 读配置项_U8(_settingsFilePath, "Settings", "language", "zh");
		_settings.cachePath = 读配置项_U8(_settingsFilePath, "Settings", "cachePath", "");

		StrA portStr = 读配置项_U8(_settingsFilePath, "Settings", "rcPort", "5572");
		_settings.rcPort = atoi((const char*)portStr);
		if (_settings.rcPort <= 0) _settings.rcPort = 5572;

		_settings.rcUser = 读配置项_U8(_settingsFilePath, "Settings", "rcUser", "admin");
		_settings.rcPass = 读配置项_U8(_settingsFilePath, "Settings", "rcPass", "admin123");
	}

	void 触发状态回调(c_StrA name, c_StrA status, c_StrA errMsg)
	{
		自动锁 lock(_mutex);
		for (int i = 0; i < _statusCallbacks.count; ++i) {
			if (_statusCallbacks[i]) _statusCallbacks[i](name, status, errMsg);
		}
	}

	struct MountProcessInfo
	{
		HANDLE hProcess = NULL;
		HANDLE hThread = NULL;
		DWORD pid = 0;
		int rcPort = 5572;
		StrA driveName;
		StrA letter;
	};

	互斥锁 _mutex;
	Arraybe<DriveConfig> _drives;
	HashTbe<StrA, MountProcessInfo> _activeMounts;
	AppSettings _settings;
	Arraybe<StatusCallback> _statusCallbacks;
	StrA _appDir;
	StrA _rcloneExePath;
	StrA _configFilePath;
	StrA _settingsFilePath;
};
