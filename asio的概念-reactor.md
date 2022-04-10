# reactor模型和proactor模型
reactor模型和procator模型，先简单介绍下这两种模型。
## reactor模型
reactor模型其实很很简单，原理就是采用的I/O多路复用，其中reactor对象负责监听I/O事件的产生和把事件分发出去，而事件处理器则负责相应的事件处理。比如acceptor处理器处理所有的accept事件，而接收/发送处理器处理所有的send/recv事件，你也可以把timer加进来，timer处理器处理所有的定时器事件。这里可以每个处理器一个线程，或者每个处理器一个进程。你的reactor也可以是运行在多个线程之上的，即每个epoll_wait就是一个reactor。从而变成高效的多reactor多进程/多线程方案。如下图所示：
![reactors](C:\Users\28184\Desktop\km\follow_asio_ex\reactors_handlers.jpg)
Reactor 是非阻塞同步网络模式，感知的是就绪可读写事件。在每次感知到有事件发生（比如可读就绪事件）后，就需要应用进程主动调用 read 方法来完成数据的读取，也就是要应用进程主动将 socket 接收缓存中的数据读到应用进程内存中，这个过程是同步的，读取完数据后应用进程才能处理数据。
## proactor模型
proactor是异步网络模式。普通的非阻塞I/O比如read函数，在数据未准备好的时候，返回的是E_WOULDBLOCK，当数据准备好的时候，调用read读取，将数据从内核态拷贝到用户态。这不是真正的异步，真正的异步，指的是从内核态拷贝到用户态这一步，也不用阻塞在这里等待，而是使用通知的方式，也就是说当内核态到用户态拷贝完成的时候，通知用户，数据真正的已经获取到了。这算是真正的异步与普通的非阻塞I/O的区别。
如下图所示：
![normal_non_block](C:\Users\28184\Desktop\km\follow_asio_ex\read_normal_nonblock.jpg)
![actual_non_block](C:\Users\28184\Desktop\km\follow_asio_ex\read_actual_nonblock.jpg)
知乎上一个很形象生动的例子：
阻塞 I/O 好比，你去饭堂吃饭，但是饭堂的菜还没做好，然后你就一直在那里等啊等，等了好长一段时间终于等到饭堂阿姨把菜端了出来（数据准备的过程），但是你还得继续等阿姨把菜（内核空间）打到你的饭盒里（用户空间），经历完这两个过程，你才可以离开。
非阻塞 I/O 好比，你去了饭堂，问阿姨菜做好了没有，阿姨告诉你没，你就离开了，过几十分钟，你又来饭堂问阿姨，阿姨说做好了，于是阿姨帮你把菜打到你的饭盒里，这个过程你是得等待的。
异步 I/O 好比，你让饭堂阿姨将菜做好并把菜打到饭盒里后，把饭盒送到你面前，整个过程你都不需要任何等待。
asio使用的就是这个模型，即数据真正完成了读写后才通知。所以称之为异步网络模型。如下图所示：
![proactors](C:\Users\28184\Desktop\km\follow_asio_ex\proactor_handlers.jpg)
Proactor 是异步网络模式， 感知的是已完成的读写事件。在发起异步读写请求时，需要传入数据缓冲区的地址（用来存放结果数据）等信息，这样系统内核才可以自动帮我们把数据的读写工作完成，这里的读写工作全程由操作系统来做，并不需要像 Reactor 那样还需要应用进程主动发起 read/write 来读写数据，操作系统完成读写工作后，就会通知应用进程直接处理数据。
## scheduler下使用的模型
遗憾的是，linux下的异步I/O支持不完善，aio系列函数是在用户空间下模拟出来的异步，而不是真正的操作系统级别的支持，并且还只支持本地文件的aio异步操作，网络socket行不通。。所以在linux下的高性能的网络库使用的是reactor模型，而不是proactor，在windows下则是可以使用这个效率更高的proactor模型。
### scheduler的get_default_task函数
get_default_task如函数所示，	如果未定义ASIO_HAS_IO_URING_AS_DEFAULT宏，将返回reactor的实例

	scheduler_task* scheduler::get_default_task(asio::execution_context& ctx)
	{
	#if defined(ASIO_HAS_IO_URING_AS_DEFAULT)
		return &use_service<io_uring_service>(ctx);
	#else // defined(ASIO_HAS_IO_URING_AS_DEFAULT)
		return &use_service<reactor>(ctx);
	#endif // defined(ASIO_HAS_IO_URING_AS_DEFAULT)
	}
### scheduler的init_task
可以看到，在init_task的时候，将会创建默认的后台任务，即上面的reactor

	void scheduler::init_task()
	{
		mutex::scoped_lock lock(mutex_);
		if (!shutdown_ && !task_)
		{
    		task_ = get_task_(this->context());
    		op_queue_.push(&task_operation_);
    		wake_one_thread_and_unlock(lock);
		}
	}
默认情况下，scheduler是不会调用init_task的，即默认的io_context/io_service是只有投递的任务，而没有默认的任务执行的。
先版本的io_service就是io_context。typedef了一下
### reactor的多种选择
当include reactor.hpp时，会根据宏的定义，来使用不同的api创建reactor模型

	#if defined(ASIO_HAS_IOCP) || defined(ASIO_WINDOWS_RUNTIME)
		typedef null_reactor reactor;
	#elif defined(ASIO_HAS_IO_URING_AS_DEFAULT)
		typedef null_reactor reactor;
	#elif defined(ASIO_HAS_EPOLL)
		typedef epoll_reactor reactor;
	#elif defined(ASIO_HAS_KQUEUE)
		typedef kqueue_reactor reactor;
	#elif defined(ASIO_HAS_DEV_POLL)
		typedef dev_poll_reactor reactor;
	#else
		typedef select_reactor reactor;
	#endif
