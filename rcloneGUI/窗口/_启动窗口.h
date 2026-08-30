#pragma once
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
		UINT 事件_资源加载(LPSCN_LOAD_DATA pld) override;
	} st;

	void 事件_创建完毕();

	void 载入(窗口* 父窗 = 0, bool 模态 = 0);
	void 完毕(bool 模态);

	static SciterObj DriveConfigToSciterObj(const DriveConfig& d);

	static DriveConfig SciterObjToDriveConfig(const SciterObj& arg);
private:
	static Bytes ExtractAppIconAsPng();
}; extern __启动窗口 _启动窗口;
