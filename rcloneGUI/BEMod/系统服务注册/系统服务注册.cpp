/**@ModuleTitle: 系统服务注册
*  @version:     1.0
*  @platform:    win32(x86|x64)
*  @compiler:    source
*  @author:
*  @datetime:    
*  @description: Windows 系统服务管理与控制类模块 (白易规范)
*/
#include "stdafx.h"

typedef void(*服务回调_fn)();

namespace {
SERVICE_STATUS_HANDLE g_SvcStatusHandle = NULL;
HANDLE g_hSvcStopEvent = NULL;
服务回调_fn g_回调_指针[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

/**状态反馈
 * 向服务控制管理器 (SCM) 提交当前服务状态
 * @param dwCurrentState 服务状态 (如 SERVICE_RUNNING, SERVICE_STOPPED 等)
 * @param dwWin32ExitCode 错误代码 / 退出码，默认为 0
 * @param dwWaitHint 检查点等待时间 (毫秒)，默认为 0
 */
void 状态反馈(DWORD dwCurrentState, DWORD dwWin32ExitCode = 0, DWORD dwWaitHint = 0)
{
	if (!g_SvcStatusHandle) return;
	SERVICE_STATUS SvcStatus = { 0 };
	SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
	SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_PARAMCHANGE;
	SvcStatus.dwCurrentState = dwCurrentState;
	SvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	SvcStatus.dwWaitHint = dwWaitHint;
	SetServiceStatus(g_SvcStatusHandle, &SvcStatus);
}

/**SvcCtrlHandler
 * 服务控制处理回调函数，由 SCM 调用以改变控制该服务的状态
 * @param dwCtrl 执行指令代码 (如 SERVICE_CONTROL_STOP 等)
 */
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	switch (dwCtrl) {
	case SERVICE_CONTROL_STOP: // 停止
	case SERVICE_CONTROL_SHUTDOWN:
		状态反馈(SERVICE_STOP_PENDING, 0, 3000);
		if (g_hSvcStopEvent) {
			SetEvent(g_hSvcStopEvent);
		}
		if (g_回调_指针[1]) {
			g_回调_指针[1]();
		}
		break;
	case SERVICE_CONTROL_PAUSE: // 暂停
		if (g_回调_指针[2]) {
			g_回调_指针[2]();
		}
		状态反馈(SERVICE_PAUSED, 0, 0);
		break;
	case SERVICE_CONTROL_CONTINUE: // 恢复
		if (g_回调_指针[3]) {
			g_回调_指针[3]();
		}
		状态反馈(SERVICE_RUNNING, 0, 0);
		break;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;
	}
}

/**ServiceMain
 * 服务主入口函数，由操作系统调用并执行服务的初始化与运行
 * @param dwArgc 传递给服务的值的个数
 * @param lpszArgv 字符数组指针，第一项为当前服务名称
 */
VOID WINAPI ServiceMain(DWORD dwArgc, LPWSTR* lpszArgv)
{
	if (dwArgc > 0 && lpszArgv != nullptr && lpszArgv[0] != nullptr) {
		g_SvcStatusHandle = RegisterServiceCtrlHandlerW(lpszArgv[0], SvcCtrlHandler);
	}
	if (!g_SvcStatusHandle) {
		return;
	}

	g_hSvcStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	状态反馈(SERVICE_START_PENDING, 0, 3000);

	if (g_回调_指针[4]) {
		g_回调_指针[4]();
	}
	状态反馈(SERVICE_RUNNING, 0, 0);

	// 阻塞等待系统服务停止信号，使服务在后台持续运行
	if (g_hSvcStopEvent) {
		WaitForSingleObject(g_hSvcStopEvent, INFINITE);
		CloseHandle(g_hSvcStopEvent);
		g_hSvcStopEvent = NULL;
	}

	状态反馈(SERVICE_STOPPED, 0, 0);
}
}

class EXP 系统服务
{
public:
	StrW m_服务名称;

	/**Instance
	 * 获取系统服务类全局单例实例
	 * @return 返回系统服务类单例引用
	 */
	static 系统服务& Instance()
	{
		static 系统服务 s_instance;
		return s_instance;
	}

	/**初始化
	 * 初始化系统服务配置与回调。若当前为系统服务启动，则自动进入分发循环
	 * @param 服务名称 服务名称
	 * @param 回调_服务启动时 服务启动时执行的回调函数
	 * @param 回调_停止 服务停止时执行的回调函数
	 * @param 回调_暂停 服务暂停时执行的回调函数
	 * @param 回调_恢复 服务恢复时执行的回调函数
	 * @return 若为服务启动返回真，否则返回假
	 */
	bool 初始化(c_StrX 服务名称, 服务回调_fn 回调_服务启动时 = nullptr,
		服务回调_fn 回调_停止 = nullptr, 服务回调_fn 回调_暂停 = nullptr,
		服务回调_fn 回调_恢复 = nullptr)
	{
		m_服务名称 = 服务名称;
		bool isSvc = 当前进程是否为服务启动();
		if (isSvc) {
			g_回调_指针[4] = 回调_服务启动时;
			g_回调_指针[1] = 回调_停止;
			g_回调_指针[2] = 回调_暂停;
			g_回调_指针[3] = 回调_恢复;

			SERVICE_TABLE_ENTRYW st[2] = {
				{ (LPWSTR)(const wchar_t*)m_服务名称, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
				{ NULL, NULL }
			};

			StartServiceCtrlDispatcherW(st);
		}
		return isSvc;
	}

	// ====================================================
	// 静态方法（Static Methods）
	// ====================================================

	/**当前进程是否为服务启动
	 * 判断当前进程是否由 Windows 服务管理器 (services.exe) 启动
	 * @return 是返回真，否则返回假
	 */
	static bool 当前进程是否为服务启动()
	{
		// 检查父进程是否为 services.exe (Session 0 系统服务)
		DWORD currentPid = GetCurrentProcessId();
		DWORD parentPid = 0;
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE) {
			return false;
		}

		PROCESSENTRY32W pe = { sizeof(pe) };
		if (Process32FirstW(hSnap, &pe)) {
			do {
				if (pe.th32ProcessID == currentPid) {
					parentPid = pe.th32ParentProcessID;
					break;
				}
			} while (Process32NextW(hSnap, &pe));
		}

		bool isService = false;
		if (parentPid != 0 && Process32FirstW(hSnap, &pe)) {
			do {
				if (pe.th32ProcessID == parentPid) {
					if (_wcsicmp(pe.szExeFile, L"services.exe") == 0) {
						isService = true;
					}
					break;
				}
			} while (Process32NextW(hSnap, &pe));
		}

		CloseHandle(hSnap);
		return isService;
	}

	/**是否存在
	 * 检查指定的系统服务是否存在
	 * @param 服务名称 服务名称
	 * @return 存在返回真，不存在返回假
	 */
	static bool 是否存在(c_StrX 服务名称)
	{
		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
		if (!hSCManager) {
			hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!hSCManager) return false;
		}
		SC_HANDLE hService = OpenServiceW(hSCManager, 服务名称, SERVICE_QUERY_STATUS);
		if (!hService) {
			CloseServiceHandle(hSCManager);
			return false;
		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return true;
	}

	/**取系统服务状态
	 * 查询指定系统服务的当前运行状态
	 * @param 服务名称 服务名称
	 * @return 0: 未安装, 1: 已启动 (运行中), 2: 未启动 (已停止)
	 */
	static int 取系统服务状态(c_StrX 服务名称)
	{
		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
		if (!hSCManager) {
			hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!hSCManager) return 0;
		}
		SC_HANDLE hService = OpenServiceW(hSCManager, 服务名称, SERVICE_QUERY_STATUS);
		if (!hService) {
			CloseServiceHandle(hSCManager);
			return 0; // 未安装
		}
		SERVICE_STATUS status = { 0 };
		int result = 2; // 默认已安装但未启动
		if (QueryServiceStatus(hService, &status)) {
			if (status.dwCurrentState == SERVICE_RUNNING || status.dwCurrentState == SERVICE_START_PENDING) {
				result = 1; // 已启动 (运行中)
			} else {
				result = 2; // 未启动 (已停止)
			}
		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return result;
	}

	/**结束所有进程
	 * 结束系统中所有指定名称的进程
	 * @param 进程名 进程文件名 (如 "rclone.core.exe")
	 */
	static void 结束所有进程(c_StrX 进程名)
	{
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE) return;

		StrW wProcName = (StrW)进程名;
		PROCESSENTRY32W pe = { sizeof(pe) };
		if (Process32FirstW(hSnap, &pe)) {
			do {
				if (_wcsicmp(pe.szExeFile, (const wchar_t*)wProcName) == 0) {
					HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
					if (hProc) {
						TerminateProcess(hProc, 0);
						CloseHandle(hProc);
					}
				}
			} while (Process32NextW(hSnap, &pe));
		}
		CloseHandle(hSnap);
	}

	/**启动
	 * 启动某个指定服务，成功返回1，否则返回错误代码
	 * @param 服务名称 服务名
	 * @param 是否等待返回超时 等待超时毫秒数，0为不等待，-1为无限等待，默认空(0)
	 * @param 是否非阻塞等待 是否非阻塞等待，默认为真
	 * @return 成功返回1，失败返回错误代码
	 */
	static int 启动(c_StrX 服务名称, 可空<int> 是否等待返回超时 = 空, bool 是否非阻塞等待 = true)
	{
		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!hSCManager) {
			hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
			if (!hSCManager) return (int)GetLastError();
		}
		SC_HANDLE hService = OpenServiceW(hSCManager, 服务名称, SC_MANAGER_ALL_ACCESS);
		if (!hService) {
			hService = OpenServiceW(hSCManager, 服务名称, SERVICE_START | SERVICE_QUERY_STATUS);
			if (!hService) {
				DWORD 错误代码 = GetLastError();
				CloseServiceHandle(hSCManager);
				return (int)错误代码;
			}
		}

		if (!StartServiceW(hService, 0, NULL)) {
			DWORD 错误代码 = GetLastError();
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return (int)错误代码;
		}

		int 超时 = 是否等待返回超时.OR(0);
		if (超时 != 0) {
			int k = 0;
			SERVICE_STATUS 服务状态 = { 0 };
			while (true) {
				if (是否非阻塞等待) {
					MSG msg;
					while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
					Sleep(10);
				} else {
					Sleep(10);
				}

				if (QueryServiceStatus(hService, &服务状态)) {
					if (服务状态.dwCurrentState != SERVICE_START_PENDING) {
						break;
					}
				} else {
					break;
				}

				if (超时 != -1) {
					k++;
					if (k * 10 >= 超时) {
						break;
					}
				}
			}
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 1;
	}

	/**安装
	 * 安装系统服务，可以设置执行文件为系统服务。安装成功返回1，失败返回错误代码
	 * @param 服务名称 要安装该服务的名称
	 * @param 服务程序文件 服务执行文件路径（若路径包含空格建议使用双引号）
	 * @param 启动类型 服务启动选项，默认为3 (SERVICE_DEMAND_START/手动)，2=自动，3=手动，4=禁用
	 * @param 显示名称 用户界面程序用来识别服务的显示名称，为空则使用服务名称
	 * @param 服务描述 对于服务功能的描述
	 * @param 服务可否与桌面交互 是否允许与桌面交互
	 * @param 登录用户名 如果为空则使用SYSTEM用户登录
	 * @param 登录密码 登录密码
	 * @return 安装成功返回1，参数错误返回-1，失败返回错误代码
	 */
	static int 安装(c_StrX 服务名称, c_StrX 服务程序文件, int 启动类型 = SERVICE_DEMAND_START,
		c_StrX 显示名称 = "", c_StrX 服务描述 = "", bool 服务可否与桌面交互 = false,
		c_StrX 登录用户名 = "", c_StrX 登录密码 = "")
	{
		if (!服务名称 || !服务程序文件) {
			return -1;
		}
		StrW dispName = !显示名称 ? (const wchar_t*)服务名称 : (const wchar_t*)显示名称;

		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!hSCManager) {
			return (int)GetLastError();
		}
		if (启动类型 == 0) {
			启动类型 = SERVICE_DEMAND_START; // 3
		}

		DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS | (服务可否与桌面交互 ? SERVICE_INTERACTIVE_PROCESS : 0);
		LPCWSTR lpUser = !登录用户名 ? NULL : (const wchar_t*)登录用户名;
		LPCWSTR lpPass = !登录密码 ? NULL : (const wchar_t*)登录密码;

		SC_HANDLE hService = CreateServiceW(
			hSCManager,
			服务名称,
			dispName,
			SC_MANAGER_ALL_ACCESS,
			dwServiceType,
			启动类型,
			SERVICE_ERROR_CRITICAL,
			服务程序文件,
			NULL, NULL, NULL,
			lpUser, lpPass
		);

		if (!hService) {
			DWORD 错误代码 = GetLastError();
			CloseServiceHandle(hSCManager);
			return (int)错误代码;
		}

		if (服务描述) {
			SERVICE_DESCRIPTIONW sd;
			sd.lpDescription = (LPWSTR)(const wchar_t*)服务描述;
			ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return 1;
	}

	/**停止
	 * 停止某个指定服务，成功返回1，失败返回错误代码
	 * @param 服务名称 服务名
	 * @param 是否等待返回超时 毫秒数，0为不等待，-1为无限等待，默认空(0)
	 * @param 是否非阻塞等待 是否非阻塞等待，默认为真
	 * @return 成功返回1，失败返回错误代码
	 */
	static int 停止(c_StrX 服务名称, 可空<int> 是否等待返回超时 = 空, bool 是否非阻塞等待 = true)
	{
		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!hSCManager) {
			hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
			if (!hSCManager) return (int)GetLastError();
		}
		SC_HANDLE hService = OpenServiceW(hSCManager, 服务名称, SC_MANAGER_ALL_ACCESS);
		if (!hService) {
			hService = OpenServiceW(hSCManager, 服务名称, SERVICE_STOP | SERVICE_QUERY_STATUS);
			if (!hService) {
				DWORD 错误代码 = GetLastError();
				CloseServiceHandle(hSCManager);
				return (int)错误代码;
			}
		}

		SERVICE_STATUS 服务状态 = { 0 };
		if (ControlService(hService, SERVICE_CONTROL_STOP, &服务状态)) {
			int 超时 = 是否等待返回超时.OR(0);
			if (超时 != 0) {
				int k = 0;
				while (true) {
					if (是否非阻塞等待) {
						延迟(100);
					} else {
						Sleep(10);
					}

					if (QueryServiceStatus(hService, &服务状态)) {
						if (服务状态.dwCurrentState != SERVICE_STOP_PENDING) {
							break;
						}
					} else {
						break;
					}

					if (超时 != -1) {
						k++;
						if (k * 10 >= 超时) {
							break;
						}
					}
				}
			}

			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return 1;
		}

		DWORD 错误代码 = GetLastError();
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return (int)错误代码;
	}

	/**删除
	 * 删除指定的系统服务，删除前会先尝试停止服务
	 * @param 服务名称 要删除的服务名
	 * @param 等待停止超时 停止等待超时毫秒数，默认为-1 (无限等待)
	 * @param 是否非阻塞等待 是否非阻塞等待，默认为真
	 * @return 成功返回1，服务不存在返回0，失败返回错误代码
	 */
	static int 删除(c_StrX 服务名称, int 等待停止超时 = -1, bool 是否非阻塞等待 = true)
	{
		if (!是否存在(服务名称)) {
			return 0;
		}
		停止(服务名称, 等待停止超时, 是否非阻塞等待);

		SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!hSCManager) {
			hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
			if (!hSCManager) return (int)GetLastError();
		}
		SC_HANDLE hService = OpenServiceW(hSCManager, 服务名称, SC_MANAGER_ALL_ACCESS);
		if (!hService) {
			hService = OpenServiceW(hSCManager, 服务名称, DELETE);
			if (!hService) {
				DWORD 错误代码 = GetLastError();
				CloseServiceHandle(hSCManager);
				return (int)错误代码;
			}
		}

		if (DeleteService(hService)) {
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return 1;
		}

		DWORD 错误代码 = GetLastError();
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return (int)错误代码;
	}

	/**通知系统服务启动完毕
	 * 在启动回调中使用，向 SCM 反馈服务已运行
	 */
	static void 通知系统服务启动完毕()
	{
		状态反馈(SERVICE_RUNNING, 0, 0);
	}

	// ====================================================
	// 实例方法（Instance Methods）
	// ====================================================

	/**启动
	 * 启动当前配置的服务，成功返回1，否则返回错误代码
	 * @param 是否等待返回超时 -1表示无限等待，默认空(0)
	 * @param 是否非阻塞等待 可空，默认为真
	 * @return 成功返回1，失败返回错误代码
	 */
	int 启动(int 是否等待返回超时 = 0, bool 是否非阻塞等待 = true)
	{
		return 启动(m_服务名称, 是否等待返回超时, 是否非阻塞等待);
	}

	/**安装
	 * 安装系统服务，可以设置执行文件为系统服务。安装成功返回1，失败返回错误代码
	 * @param 服务程序文件 服务执行文件路径
	 * @param 启动类型 默认为3 (SERVICE_DEMAND_START/手动)，2=自动，3=手动，4=禁用
	 * @param 显示名称 用户界面程序用来识别服务的显示名称
	 * @param 服务描述 对于服务功能的描述
	 * @param 服务可否与桌面交互 是否允许与桌面交互
	 * @param 登录用户名 如果为空则使用 SYSTEM 用户登录
	 * @param 登录密码 登录密码
	 * @return 成功返回1，失败返回错误代码
	 */
	int 安装(c_StrX 服务程序文件, int 启动类型 = SERVICE_DEMAND_START,
		c_StrX 显示名称 = "", c_StrX 服务描述 = "", bool 服务可否与桌面交互 = false,
		c_StrX 登录用户名 = "", c_StrX 登录密码 = "")
	{
		return 安装(m_服务名称, 服务程序文件, 启动类型, 显示名称, 服务描述, 服务可否与桌面交互, 登录用户名, 登录密码);
	}

	/**删除
	 * 删除系统中已有服务，成功返回1
	 * @param 等待停止超时 默认为-1
	 * @param 是否非阻塞等待
	 * @return 成功返回1，服务不存在返回0，失败返回错误代码
	 */
	int 删除(int 等待停止超时 = -1, bool 是否非阻塞等待 = true)
	{
		return 删除(m_服务名称, 等待停止超时, 是否非阻塞等待);
	}

	/**停止
	 * 停止当前配置的服务，成功返回1，失败返回错误代码
	 * @param 等待停止超时 -1表示无限等待，默认空(0)
	 * @param 是否非阻塞等待
	 * @return 成功返回1，失败返回错误代码
	 */
	int 停止(可空<int> 等待停止超时 = 空, bool 是否非阻塞等待 = true)
	{
		return 停止(m_服务名称, 等待停止超时, 是否非阻塞等待);
	}

	/**是否存在
	 * 检查当前配置的服务是否存在
	 * @return 存在返回真，不存在返回假
	 */
	bool 是否存在()
	{
		return 是否存在(m_服务名称);
	}

	/**取系统服务状态
	 * 查询当前配置的服务运行状态
	 * @return 0: 未安装, 1: 已启动 (运行中), 2: 未启动 (已停止)
	 */
	int 取系统服务状态()
	{
		return 取系统服务状态(m_服务名称);
	}
};

class EXP 自动开机
{
public:
	/**设置
	 * 设置程序开机自启动项
	 * @param 显示程序名 显示程序名（注册表键名）
	 * @param 路径 执行文件全路径
	 * @param 命令行 启动命令行参数，可空
	 * @param 本地机器 是否写入本地机器(HKLM)还是当前用户(HKCU)，如果为真需要管理员权限，默认假(HKCU)
	 * @return 成功返回真，失败返回假
	 */
	static bool 设置(c_StrX 显示程序名, c_StrX 路径, c_StrX 命令行 = "", bool 本地机器 = false)
	{
		StrW root = 本地机器 ? L"HKEY_LOCAL_MACHINE" : L"HKEY_CURRENT_USER";
		StrW regPath = root + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\" + (StrW)显示程序名;
		StrW val = L"\"" + (StrW)路径 + L"\"";
		StrW cmd = (StrW)命令行;
		if (cmd.len() > 0) {
			val += L" " + cmd;
		}
		return 写注册表值(regPath, val);
	}

	/**删除
	 * 删除程序开机自启动项
	 * @param 显示程序名 显示程序名（注册表键名）
	 * @param 本地机器 是否从本地机器(HKLM)还是当前用户(HKCU)中删除，如果为真需要管理员权限，默认假(HKCU)
	 * @return 成功返回真，失败返回假
	 */
	static bool 删除(c_StrX 显示程序名, bool 本地机器 = false)
	{
		StrW root = 本地机器 ? L"HKEY_LOCAL_MACHINE" : L"HKEY_CURRENT_USER";
		StrW regPath = root + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\" + (StrW)显示程序名;
		return 删除注册表(regPath);
	}

	/**是否存在
	 * 检测程序开机自启动项是否存在
	 * @param 显示程序名 显示程序名（注册表键名）
	 * @param 本地机器 是否在本地机器(HKLM)还是当前用户(HKCU)中检测，默认假(HKCU)
	 * @return 存在返回真，不存在返回假
	 */
	static bool 是否存在(c_StrX 显示程序名, bool 本地机器 = false)
	{
		StrW root = 本地机器 ? L"HKEY_LOCAL_MACHINE" : L"HKEY_CURRENT_USER";
		StrW regPath = root + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\" + (StrW)显示程序名;
		return 注册表是否存在(regPath);
	}
};