#include "多线程.h"
#include <thread>
#include <mutex>
#include <utility>

//多线程方法将在BE未来版本中引入，当前仅用作实验性实现

static_assert(sizeof(std::mutex) <= sizeof(互斥锁), "互斥锁 SBO 存储空间不足");
static_assert(alignof(std::mutex) <= alignof(互斥锁), "互斥锁 对齐要求不足");
static_assert(sizeof(std::thread) <= 16, "线程 SBO 存储空间不足");

// ================= 互斥锁实现 =================

互斥锁::互斥锁()
{
	new (_storage) std::mutex();
}

互斥锁::~互斥锁()
{
	reinterpret_cast<std::mutex*>(_storage)->~mutex();
}

void 互斥锁::加锁()
{
	reinterpret_cast<std::mutex*>(_storage)->lock();
}

bool 互斥锁::尝试加锁()
{
	return reinterpret_cast<std::mutex*>(_storage)->try_lock();
}

void 互斥锁::解锁()
{
	reinterpret_cast<std::mutex*>(_storage)->unlock();
}

// ================= 线程实现 =================

线程::线程()
{
	new (_storage) std::thread();
}

线程::线程(线程函数 fn)
{
	new (_storage) std::thread([fn = be::move(fn)]() mutable {
		if (fn) fn();
	});
}

线程::~线程()
{
	auto* t = reinterpret_cast<std::thread*>(_storage);
	if (t->joinable()) {
		t->detach();
	}
	t->~thread();
}

线程::线程(线程&& other) noexcept
{
	auto* ot = reinterpret_cast<std::thread*>(other._storage);
	new (_storage) std::thread(std::move(*ot));
}

线程& 线程::operator=(线程&& other) noexcept
{
	if (this != &other) {
		auto* t = reinterpret_cast<std::thread*>(_storage);
		auto* ot = reinterpret_cast<std::thread*>(other._storage);
		if (t->joinable()) {
			t->detach();
		}
		*t = std::move(*ot);
	}
	return *this;
}

void 线程::分离()
{
	reinterpret_cast<std::thread*>(_storage)->detach();
}

void 线程::等待()
{
	reinterpret_cast<std::thread*>(_storage)->join();
}

bool 线程::可等待() const
{
	return reinterpret_cast<const std::thread*>(_storage)->joinable();
}

void 线程::启动(线程函数 fn)
{
	线程 t(be::move(fn));
	t.分离();
}
