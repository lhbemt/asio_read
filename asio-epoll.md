# asio中的epoll
asio在linux上使用的是epoll_reactor。它的头文件是epoll_reactor.hpp和epoll_reactor.ipp。
## epoll_reactor的成员
### op_types
网络数据有读，写，异常状态，这里使用了op_types来表示这几种状态
	
	enum op_types { read_op = 0, write_op = 1,
    	connect_op = 1, except_op = 2, max_ops = 3 };
### typedef descriptor_state* per_descriptor_data;
在epoll_event这个结构体中，除了有fd，events两个成员，还有个data字段，这个data字段中有个成员就是ptr。可以理解为和fd绑定的用户自定义数据，而在epoll_reactor中，就是下面这个per_descriptor_data。
per_descriptor_data这是一个双向链表，表示每个网络描述符上的操作和数据，它是一个descriptor_state类指针，它继承自operation，当operation完成的时候，调用complete函数，里面就是调用其回调函数。
	
	class scheduler_operation ASIO_INHERIT_TRACKED_HANDLER
	{
	public:
		typedef scheduler_operation operation_type;
		void complete(void* owner, const asio::error_code& ec,
      		std::size_t bytes_transferred)
		{
    		func_(owner, this, ec, bytes_transferred);
		}
	...
在descriptor_state这个类中，有个成员是未完成的操作队列：

	op_queue<reactor_op> op_queue_[max_ops];
当epoll_wait有事件发生时，会调用description_data来注册事件
	
	void set_ready_events(uint32_t events) { task_result_ = events; }
    void add_ready_events(uint32_t events) { task_result_ |= events; }
可见这个op_queue就是上面的read_op write_op等。当然，为了支持多线程，它也有一个mutex
	
	mutex mutex_;
perform_io就是执行符合op_queue_队列中所有符合events事件的操作符，先mutexlock住，再perform
	
	operation* epoll_reactor::descriptor_state::perform_io(uint32_t events)
	{
		mutex_.lock();
		...
		static const int flag[max_ops] = { EPOLLIN, EPOLLOUT, EPOLLPRI };
		for (int j = max_ops - 1; j >= 0; --j)
		{
			if (events & (flag[j] | EPOLLERR | EPOLLHUP))
    		{
      			try_speculative_[j] = true;
      			while (reactor_op* op = op_queue_[j].front())
      			{
        			if (reactor_op::status status = op->perform())
        			{
        				...
        			}
      			}
      		}
      	}
    }
### register_internal_descriptor
上面说到，在注册描述符到epoll中，会绑定这个网络描述符的自定义数据，per_descriptor_data。就是在这个函数中完成的，并且使用了对象池(通过object_poll，它是一个free_list的方案，来构建对象池。)，因为per_descriptor_data是经常被使用的。
	
	int epoll_reactor::register_internal_descriptor(
    	int op_type, socket_type descriptor,
    	epoll_reactor::per_descriptor_data& descriptor_data, reactor_op* op)
	{
		descriptor_data = allocate_descriptor_state();
		ASIO_HANDLER_REACTOR_REGISTRATION((
        	context(), static_cast<uintmax_t>(descriptor),
        reinterpret_cast<uintmax_t>(descriptor_data)));
		{
    		mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);
    		...
		}
		epoll_event ev = { 0, { 0 } };
		ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
		descriptor_data->registered_events_ = ev.events;
		ev.data.ptr = descriptor_data;
		int result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, descriptor, &ev);
		...
	}
### deregister_descriptor
显然就是取消epollfd上的描述符了。
### start_op
这种就是立即执行的事件了，accept和write操作，它是调用post_immediate_completion函数。

	start_op(impl, io_uring_service::write_op, p.p, is_continuation, false);

	void epoll_reactor::start_op(int op_type, socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data, reactor_op* op,
    bool is_continuation, bool allow_speculative) {
    	...
    	descriptor_data->op_queue_[op_type].push(op);
		scheduler_.work_started();
    }
前面分析过scheduler_.work_started()会使的work+1，即scheduler的run_for_one执行。当执行完毕时，调用op的回调函数，告知已发送多少数据。或者accept到了socket的连接。
### post_immediate_completion
这个是scheduler的post_immediate_completion函数。
### add_timer_queue
在linux中，使用timerfd_create可以创建一个定时器的fd，它可以加入到epoll_wait中，当定时完成的时候，触发可读事件，执行相应的回调函数。

	fd = timerfd_create(CLOCK_MONOTONIC, 0);

	void epoll_reactor::do_add_timer_queue(timer_queue_base& queue)
	{
		mutex::scoped_lock lock(mutex_);
		timer_queues_.insert(&queue);
	}
从而将定时器也与epoll统一起来管理。
	
	  if (check_timers)
	  {
    	mutex::scoped_lock common_lock(mutex_);
    	timer_queues_.get_ready_timers(ops);
		#if defined(ASIO_HAS_TIMERFD)
    	if (timer_fd_ != -1)
    	{
      		itimerspec new_timeout;
      		itimerspec old_timeout;
      		int flags = get_timeout(new_timeout);
      		timerfd_settime(timer_fd_, flags, &new_timeout, &old_timeout);
    	}
	  #endif // defined(ASIO_HAS_TIMERFD)
	  }
timer_queues_.get_ready_timers(ops);有不同的timer实现，这块先不研究，我知道的有红黑树管理和时间轮方案。
### run
run函数就是epoll_reactor在epoll_wait，在有事件触发的时候，通过set_ready_events告知descriptor_state，for循环的代码如下：

	for (int i = 0; i < num_events; ++i)
	{
    	void* ptr = events[i].data.ptr;
    	if (ptr == &interrupter_)
    	{
      	// No need to reset the interrupter since we're leaving the descriptor
      	// in a ready-to-read state and relying on edge-triggered notifications
      	// to make it so that we only get woken up when the descriptor's epoll
      	// registration is updated.
		#if defined(ASIO_HAS_TIMERFD)
      	if (timer_fd_ == -1)
        	check_timers = true;
		#else // defined(ASIO_HAS_TIMERFD)
      		check_timers = true;
		#endif // defined(ASIO_HAS_TIMERFD)
    	}
		#if defined(ASIO_HAS_TIMERFD)
    	else if (ptr == &timer_fd_)
    	{
      		check_timers = true;
    	}
		#endif // defined(ASIO_HAS_TIMERFD)
    	else
    	{
      	// The descriptor operation doesn't count as work in and of itself, so we
      	// don't call work_started() here. This still allows the scheduler to
      	// stop if the only remaining operations are descriptor operations.
      	descriptor_state* descriptor_data = static_cast<descriptor_state*>(ptr);
      	if (!ops.is_enqueued(descriptor_data))
      	{
        	descriptor_data->set_ready_events(events[i].events);
        	ops.push(descriptor_data);
      	}
      	else
      	{
        	descriptor_data->add_ready_events(events[i].events);
      	}
    }
set或者add_ready_evnets后，在do_complete函数中，会调用perform_io来执行事件对应的回调函数。asio::error_code& ec, std::size_t bytes_transferred错误码，以及读写的数据字节数。
最终是由scheduler_operation的complete函数，调用绑定的func，即do_complete，最终调用perform_io，完成回调。
看一个accept的例子：

	(gdb) bt
	#0  asio::detail::epoll_reactor::descriptor_state::perform_io (this=0x65d2a0, events=1) at /usr/local/include/asio/detail/impl/epoll_reactor.ipp:759
	#1  0x0000000000443ae5 in asio::detail::epoll_reactor::descriptor_state::do_complete (owner=0x65d060, base=0x65d2a0, ec=..., bytes_transferred=1) at /usr/local/include/asio/detail/impl/epoll_reactor.ipp:803
	#2  0x0000000000440bc8 in asio::detail::scheduler_operation::complete (this=0x65d2a0, owner=0x65d060, ec=..., bytes_transferred=1) at /usr/local/include/asio/detail/scheduler_operation.hpp:39
	#3  0x0000000000444a23 in asio::detail::scheduler::do_run_one (this=0x65d060, lock=..., this_thread=..., ec=...) at /usr/local/include/asio/detail/impl/scheduler.ipp:491
	#4  0x00000000004444e4 in asio::detail::scheduler::run (this=0x65d060, ec=...) at /usr/local/include/asio/detail/impl/scheduler.ipp:209
	#5  0x0000000000445205 in asio::io_context::run (this=0x7fffffffe340) at /usr/local/include/asio/impl/io_context.ipp:62
	#6  0x000000000043deea in main (argc=1, argv=0x7fffffffe478) at /home/li/bpsj_server/asio_test/src/test_acceptor.cpp:8
descriptor_data就是继承自operation，而operation的原型就是scheduler_operation，是其typedef过来的。这里有点绕epoll_wait返回后，通过这个ops.push，告知scheduler有新的任务处理，scheduler在do_run_one中，执行任务，这个任务就是descriptor_data的do_complete函数，从而根据events响应不同的读写事件。


