# asio的概念--io_context
## io_context
我们随便打开一个asio的官方例子里，可以看到在main函数中，都是io_context.run()函数启动服务。所以io_context这个类是驱动整个异步io的类。它负责提供核心异步I/O功能。包括这些异步I/O对象： asio::ip::tcp::socket asio::ip::tcp::acceptor asio::ip::udp::socket asio::deadline_timer。io_context类还为开发自定义异步服务提供了工具。
io_context是线程安全的，但是restart() notify_fork()函数除外，即这两个函数是非线程安全的。在run() run_one() run_for() run_until() poll() poll_one()函数未结束的时候，调用restart()函数，将导致未定义的行为。notify_fork()函数应该在另一个线程中调用，不能在任何io_conetxt函数或者与io_context关联起来的异步object的函数里调用notify_fork，想调用notify_fork应该另起一个线程。简单来说io_context就是负责执行你投递的任务，当执行到时，调用你的回调函数。
iocontext可以进行同步或者异步操作，I/O对象上的同步操作隐式的运行io_context对象来完成单个操作。io_context的实例调用函数run() run_one() run_for() run_until() poll() poll_one()来执行异步操作。异步操作的完成由关联的handler来传递。handler允许抛出异常，并且允许该异常传播到调用run() run_one() run_for() run_until() poll() poll_one()函数的线程上，其它线程将不受影响，但是该线程应当处理这个异常，上面的run函数和poll函数可以自动重启io_context，而不用显示的调用restart函数。比如下面这个例子：

	asio::io_context io_context;
	for(;;) {
		try {
			io_context.run();
			break; // 正常退出
		}
		catch(my_exception& e) { // 处理异常后，io_context将重新run，不需要手动的调用restart函数。
			std::cout << e.what() << std::endl;
		}
	}
如果上面的是多个线程运行这个for，将不会影响到其它线程。因为io_context自动重启了。
可以向io_context投递任意的任务，函数asio::dispatch asio::post asio::defer都可以投递任务。
当你的io_context是运行在后台的线程，你不希望在run的时候，所有任务都执行完的时候就立马返回了，那么你需要创建一个跟踪io_context的执行器：

	auto work = asio::require(io_context.get_executor(), asio::execution::outstanding_work.tracked);
当你想停止的时候，需要调用io_context的stop函数，来停止，这会放弃其未执行完的任务，快速返回。慎重，假如你有落地的数据任务。
### io_conetext类的成员
#### executor_type
这个executor是用来向io_context投递任务的。
#### explicit io_context(int concurrency_hint)
多少个线程来并发执行io_context里面的任务。
#### io_context的析构函数
顺序是先所有service object svc->shutdown()，再所有handler销毁，再销毁service，，最后调用stop函数。
#### run
run函数是让io_context进入事件处理循环中，run函数将会阻塞，直到所有任务都被执行完毕，或者io_context调用了stop。当io_context的构造函数添加了线程数量，那么这个run将会并发执行任务。而在多个线程中，io_context.run()函数，支持运行在多个线程中，当run结束后，再次调用restart函数，再次post或dispatch，该io_context可以接着调用run运行。
#### poll poll_one
poll和run一样，也是用于处理handle，但它不会阻塞，poll_one只执行一个handler。
#### run_for和run_until
run_for一段时间内处理handle，和epoll_wait加时间一样的意思。run_until绝对时间。
#### run_one run_one_for run_one_until
只要有一个handler被处理了，就返回。for相对时间，until绝对时间。
#### stop
stop函数并不是阻塞的，只是简单的发送一个信号给io_context，告诉其要停止了，当stop调用了的时候，再次调用run或者poll都会立马返回，除非进行了restart(表示要再次启动io_context)。
#### stopped
检测io_context是否停止了，true停止，false没有。
#### restart
restart函数不能在未调用run或者poll就直接调用。
#### io_context.basic_executor_type
看起来io_context是由executor_type来执行的？看一看，不确定。

	executor_work_guard<io_context::excutor_type> w = make_work_guard(ioc); // 为io_context创建一个guard
	w.reset();
#### io_context::service
所有io_context的服务的基类。service也是可以参与io_context的任务的，每个service有一个service_id。
#### service的类成员
##### get_io_context
获取这个service运行在哪个io_context上
##### shutdown
service停止。
### post任务与io_context的run
在c++中，可以将模板的声明放在.h文件中，而模板的实现则放在.ipp中。在detail这个namespace里，我们看到io_context_impl被定义成这样

	#if defined(ASIO_HAS_IOCP)
		typedef win_iocp_io_context io_context_impl;
		class win_iocp_overlapped_ptr;
	#else
		typedef scheduler io_context_impl;
	#endif
io_context.ipp文件中，是对io_context.hpp的实现，看下构造函数：

	io_context::io_context()
		: impl_(add_impl(new impl_type(*this,
          ASIO_CONCURRENCY_HINT_DEFAULT, false)))
	{
	}
为了实现跨平台，屏蔽用户对asio不同系统下的使用不方便性，于是就使用了impl_这个代理。
impl_type就是上面定义的如果有iocp，是win_iocp_io_context，其余则是scheduler，我们学习的是linux下的，所以也就只看scheduler部分。
run函数：

	io_context::count_type io_context::run()
	{
		asio::error_code ec;
		count_type s = impl_.run(ec);
		asio::detail::throw_error(ec);
		return s;
	}
可以看到各种run函数，poll函数都是代理给了impl_实现，所以我们研究io_context在linux下的背后运行就只需要看scheduler的实现就可以了。
#### scheduler的工作流程
我们使用asio::post或者asio::dispatch往一个iocontext里丢任务，这个任务就是一个函数。即往iocontext里丢入要异步执行的函数，具体什么时候执行，由iocontext来决定。
比如丢一个要递增count的函数。post是将任务投递到iocontext，无法确定啥时候执行，执行顺序也不确定，dispatch是如果当前的iocontext已经在run了，那么该任务会被直接执行，否则也是投递进去。

	asio::post(ioc, bindns::bind(increment, &count));
	asio::dispatch(ioc,
      bindns::bind(nested_decrement_to_zero, &ioc, &count));
post函数是这一大串：

	template <typename ExecutionContext,
    ASIO_COMPLETION_TOKEN_FOR(void()) NullaryToken>
	inline ASIO_INITFN_AUTO_RESULT_TYPE(NullaryToken, void()) post(
    	ExecutionContext& ctx, ASIO_MOVE_ARG(NullaryToken) token,
    	typename constraint<is_convertible<
      	ExecutionContext&, execution_context&>::value>::type)
	{
		return async_initiate<NullaryToken, void()>(
      		detail::initiate_post_with_executor<
        	typename ExecutionContext::executor_type>(
          	ctx.get_executor()), token);
	}
每个io_context是都有一个executor，token则是上面绑定传过来的函数，在async_initiate中最后会调用函数post_immediate_completions，将任务插入到scheduler的op_queue_队列中

	work_started();
	mutex::scoped_lock lock(mutex_);
	op_queue_.push(op);
	wake_one_thread_and_unlock(lock);
这个op_queue的定义：

	op_queue<operation> op_queue_;
所有投递的任务都会被包装成scheduler_operation。

	typedef void (*func_type)(void*, scheduler_operation*, const asio::error_code&, std::size_t);
    scheduler_operation(func_type func) : next_(0),
      	func_(func),
      	task_result_(0)
	{
	}
最后在io_context.run()函数的时候，就是取op_queue_队列中的任务来进行运行。
	
	io_context::count_type io_context::run()
    {
    	asio::error_code ec;
        count_type s = impl_.run(ec);
        asio::detail::throw_error(ec);
        return s;
    }
impl_在linux中就是scheduler。scheduler的run函数：

	std::size_t scheduler::run(asio::error_code& ec)
	{
		ec = asio::error_code();
		if (outstanding_work_ == 0)
		{
			stop();
			return 0;
		}
		thread_info this_thread;
		this_thread.private_outstanding_work = 0;
		thread_call_stack::context ctx(this, this_thread);
		mutex::scoped_lock lock(mutex_);
		std::size_t n = 0;
		for (; do_run_one(lock, this_thread, ec); lock.lock())
    		if (n != (std::numeric_limits<std::size_t>::max)())
      			++n;
		return n;
	}
thread_call_stack::context ctx(this, this_thread); 记录当前调用栈的上下文。
在for循环中执行所有任务，do_run_one函数：
	
	operation* o = op_queue_.front();
    op_queue_.pop();
    if (o == &task_operation_)
    {
    	...
    }
    else{
    	if (more_handlers && !one_thread_)
          wake_one_thread_and_unlock(lock);
        else
          lock.unlock();
    	...
    	o->complete(this, ec, task_result);
    }
可以看到在有多个任务未执行的时候，它会调用wake_one_thread_and_unlock()，该函数将会打断task_operation_类型的task，然后唤醒多余的线程来执行剩余的任务。以当前投递的任务都执行完后，才会接着执行task_operation_，
task_operation_下节再分析。可以认为它是一个幕后一直运行的任务，优先级最低。当前投递的所有任务优先级最高，所以需要task_operation_把cpu让出来。

	void scheduler::wake_one_thread_and_unlock(
    	mutex::scoped_lock& lock)
	{
		if (!wakeup_event_.maybe_unlock_and_signal_one(lock))
		{
			if (!task_interrupted_ && task_)
			{
    			task_interrupted_ = true;
      			task_->interrupt();
    		}
    		lock.unlock();
		}
	}

普通的任务投递走的是下面的else，即最终调用complete函数来执行投递时传入的函数。
	
	void complete(void* owner, const asio::error_code& ec,
      std::size_t bytes_transferred)
	{
    	func_(owner, this, ec, bytes_transferred);
	}
调用complete函数就执行完了投递的任务。
#### scheduler的类成员
##### 构造函数
ASIO_DECL scheduler(asio::execution_context& ctx,
      int concurrency_hint = 0, bool own_thread = true,
      get_task_func_type get_task = &scheduler::get_default_task);
ctx就是上面的io_context，concurrency_hint线程数，own_thread表示初始化时是否创建线程，执行run函数

	if (own_thread)
	{
		++outstanding_work_;
    	asio::detail::signal_blocker sb;
    	thread_ = new asio::detail::thread(thread_function(this));
	}
thread_function执行的就是run函数。
##### outstanding_work_
表示当前未执行完的任务，当own_thread为true的时候，outstanding_work_不会被减到0，因为默认有一个。outstanding_work_为0的时候，scheduler则stop
##### post_immediate_completion
将任务推进scheduler的op_queue_中，不应该主动调用，而应该使用asio::post或asio::dispatch
##### get_deault_task 默认运行的后台任务函数。
	scheduler_task* scheduler::get_default_task(asio::execution_context& ctx)
	{
	#if defined(ASIO_HAS_IO_URING_AS_DEFAULT)
		return &use_service<io_uring_service>(ctx);
	#else // defined(ASIO_HAS_IO_URING_AS_DEFAULT)
		return &use_service<reactor>(ctx);
	#endif // defined(ASIO_HAS_IO_URING_AS_DEFAULT)
	}
默认使用reactor，因为linux不支持proactor。
##### scheduler_task* task_
这个是上面的get_deault_task返回的指针，在init_task的时候初始化，然后唤醒一个线程，执行该默认的任务。

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
##### task_operation_
表示scheduler_task的实例，当run的时候，如果o判断是tasl_operation_，调用task的run函数：
	
	if (o == &task_operation_){
		...
		task_->run(more_handlers ? 0 : -1, this_thread.private_op_queue);
	}
schedule_task的run函数：
  
	// Run the task once until interrupted or events are ready to be dispatched.
	virtual void run(long usec, op_queue<scheduler_operation>& ops) = 0;
很明显，reactor或io_uring_service就是这种scheduler_task
##### op_queue<operation> op_queue_
任务队列
## io_context一直run下去
当对io_context没有做额外处理的时候，在执行完task后，run就会退出，这是因为下面这段代码

	work_cleanup on_exit = { this, &lock, &this_thread };
    (void)on_exit;
work_cleanup的析构函数有这样一段

	else if (this_thread_->private_outstanding_work < 1)
    {
      scheduler_->work_finished();
    }
因为在run的函数中，private_outstanding_work赋值是0，所以会执行scheduler_->work_finished();

	void work_finished()
	{
    	if (--outstanding_work_ == 0)
      		stop();
	}
所以显然要让run一直运行下去，就需要让outstanding_work始终大于0，这样do_run_one就会进入wait状态，因为stopped_一直都是false

	while (!stopped_)
	{
		...
		wakeup_event_.wait(lock);
	}
### asio::io_context::work
这个类做的事很简单，就是让io_context的scheduler的outstanding_work_初始值为1，又没有pushtask，所以一直会进入wait状态

	inline io_context::work::work(asio::io_context& io_context)
		: io_context_impl_(io_context.impl_)
	{
		io_context_impl_.work_started();
	}
	void work_started()
	{
    	++outstanding_work_;
	}
所以要让一个io_context一直运行下去，需要work包装下：
	
	asio::io_context ioc;
	asio::io_context::work work(ioc);
	ioc.run();







