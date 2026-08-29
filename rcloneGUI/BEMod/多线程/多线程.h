#pragma once
/**@ModuleTitle: 多线程
*  @version:     1.0
*  @platform:    win32(x86|x64) / linux
*  @compiler:    source
*  @description: 零STL头文件依赖的跨平台轻量多线程与互斥锁封装
*/
#include <BECore/BECore.h>

class 互斥锁
{
public:
	互斥锁();
	~互斥锁();

	互斥锁(const 互斥锁&) = delete;
	互斥锁& operator=(const 互斥锁&) = delete;

	void 加锁();
	bool 尝试加锁();
	void 解锁();

private:
	alignas(void*) char _storage[80];
};

template <typename MutexType = 互斥锁>
class 自动锁
{
public:
	explicit 自动锁(MutexType& mtx) : m_mtx(mtx)
	{
		m_mtx.加锁();
	}

	~自动锁()
	{
		m_mtx.解锁();
	}

	自动锁(const 自动锁&) = delete;
	自动锁& operator=(const 自动锁&) = delete;

private:
	MutexType& m_mtx;
};

class 线程
{
public:
	using 线程函数 = be::function<void(), 128>;

	线程();
	explicit 线程(线程函数 fn);
	~线程();

	线程(线程&& other) noexcept;
	线程& operator=(线程&& other) noexcept;

	线程(const 线程&) = delete;
	线程& operator=(const 线程&) = delete;

	void 分离();
	void 等待();

	/**判断该 std::thread 对象是否关联着一个真实的、可操作的线程。
		PS：只有创建后、且未 join/detach 的线程对象才是 true。
	 * @return
	 */
	bool 可等待() const;

	static void 启动(线程函数 fn);

private:
	alignas(void*) char _storage[16];
};
