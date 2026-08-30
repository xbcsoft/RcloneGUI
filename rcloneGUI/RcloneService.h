#pragma once
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

struct RcloneService
{
public:
	static RcloneService& Instance();

	RcloneService();

	~RcloneService();

	StrA 取运行目录();

	StrA 取Rclone路径();

	StrA 取配置文件路径();

	StrA 取设置文件路径();

	StrA 取语言目录();

	StrA 取Rclone版本();

	Arraybe<StrA> 取可用盘符列表();

	Arraybe<DriveConfig> 取所有网盘配置();

	bool 保存网盘配置(const DriveConfig& config, StrA& outErr);

	bool 删除网盘配置(c_StrA name, StrA& outErr);

	bool 检测WinFsp已安装();
	bool 安装WinFsp(HWND hParent = NULL);

	bool 挂载驱动器(c_StrA name, StrA& errMsg);

	bool 卸载驱动器(c_StrA name, StrA& outErr);

	bool 打开驱动器(c_StrA letter);

	bool 取驱动器空间(c_StrA letter, double& totalGB, double& usedGB, double& freeGB, int& usedPercent);

	StrW 取已挂载驱动器真实盘符(c_StrW driveName);

	// 0: 未开启自启, 1: 开机自启(带GUI), 2: 开机自启(无GUI/service)
	int 取自启动模式();

	void 设置自启动模式(int mode);

	AppSettings 取设置();

	bool 保存设置(const AppSettings& settings);

	bool 直接安装系统服务();

	bool 直接卸载系统服务();

	bool 安装系统服务(StrA& errMsg);

	bool 卸载系统服务(StrA& errMsg);

	bool 检测系统服务已安装();

	int 取系统服务状态();

	static StrA HttpPostRc(int port, const StrA& user, const StrA& pass, const StrA& method, const StrA& body);

	void 自动挂载所有重连网盘();

	StrA 浏览私钥文件(HWND hParent = NULL);

	typedef be::function<void(c_StrA /*name*/, c_StrA /*status*/, c_StrA /*errMsg*/), 64> StatusCallback;

	void 注册状态回调(StatusCallback cb);

private:
	static bool IsPortListening(int port);

	static bool StartRcloneDaemon(const StrA& exePath, const StrA& configPath, int port, const StrA& user, const StrA& pass, const StrA& appDir);

	static Arraybe<StrA> SplitString(const StrA& str, char delim);

	void 保存驱动器排序();

public:
	void 置驱动器排序(const Arraybe<StrA>& orderList);

	void 加载本地驱动器配置();

	void 加载设置();

	void 触发状态回调(c_StrA name, c_StrA status, c_StrA errMsg);

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
