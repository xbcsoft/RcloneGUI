#include "resource.h"
#include <BEWin32UI/runtime.h>
#include <SciterUI/SciterUI.h>
#include "RcloneService.h"
RcloneService& rclonesvc = RcloneService::Instance();

int main(int nCShow = SW_SHOWNORMAL, char** argVec = nullptr)
{
	Arraybe<StrA> cmdArgs; 取命令行(cmdArgs);
	// 检查命令行参数
	if (cmdArgs.count > 0) {
		if (cmdArgs[0] == "service") {
			nCShow = SW_HIDE;
		} else if (cmdArgs[0] == "install") {
			bool ok = rclonesvc.直接安装系统服务();
			return ok ? 0 : 1;
		} else if (cmdArgs[0] == "uninstall") {
			bool ok = rclonesvc.直接卸载系统服务();
			return ok ? 0 : 1;
		}
	}

	bool isServive = 系统服务::Instance().初始化("rcloneGUIService", []() {
		// 服务启动回调：初始化服务并在后台拉起 rclone.core.exe 守护进程
		系统服务::通知系统服务启动完毕();
		// 在后台线程自动挂载所有勾选了“登录时重新连接”的网盘
		线程::启动([]() {
			rclonesvc.自动挂载所有重连网盘();
		});
	}, []() {
		// 服务停止回调：关掉所有 rclone.core.exe
		系统服务::结束所有进程("rclone.core.exe");
	});
	if (isServive) { return 0; }


	dbg_log("=== APP STARTED ===");
	全局初始化配置(GetModuleHandle(0), true);
	SciterUI::全局初始化();

	_启动窗口.初显(nCShow).载入();
	return Win32消息循环();
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPTSTR, int nCShow)
{
	return main(nCShow);
}