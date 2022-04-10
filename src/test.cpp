#include "asio.hpp"

void print() {
	return;
}

int main(int argc, char* argv[]) {
	
	asio::io_context ioc; // 无任何默认任务的io_context
	// asio::io_service ioc; // io_service现在就是io_context，新版本的asio应该使用这个
	asio::post(ioc, &print);

	asio::io_context::work work(ioc); // work会使outstanding_work_使用保持最少为1，run会一直运行下去
	ioc.run();

	return 0;
}


