#include "../_启动窗口.h"

void __启动窗口::载入(窗口* 父窗, bool 模态)
{
	if (窗口句柄)return;

	窗口::参数 cs{ 0, 0, 559, 530 };
	cs.最大化按钮 = false; // 不需要最大化按钮
	cs.最小化按钮 = true;  // 提供任务栏最小化
	cs.边框 = 窗口边框::普通固定边框;
	窗口::创建(cs);
	
	this->完毕(模态);
}

void __启动窗口::完毕(bool 模态)
{
	SciterUI::参数 cs;
#ifdef _DEBUG
	cs.文件_html = "index.html";
#else
	cs.内存_zip = R::htmZip;
#endif

	st.绑定(cs, this);
	窗口::完毕(模态);
}
