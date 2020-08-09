#include "gen-cpp/BenchmarkThrift.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/server/TNonblockingServer.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/transport/TBufferTransports.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

using boost::shared_ptr;

class BenchmarkThriftHandler : virtual public BenchmarkThriftIf {
public:
	void echo_thrift(const std::string& msg) { }
	void slow_thrift(const std::string& msg) {
		usleep(15000);
	}
};

int main(int argc, char **argv) {
	int port = 8811;
	shared_ptr<BenchmarkThriftHandler> handler(new BenchmarkThriftHandler());
	shared_ptr<TProcessor> processor(new BenchmarkThriftProcessor(handler));
	shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
	boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(16);
	boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());

	TNonblockingServer server(processor, protocolFactory, port, threadManager);
	server.setMaxConnections(2048);
	server.setNumIOThreads(16);
	threadManager->threadFactory(threadFactory);
	threadManager->start();
	server.serve();
	return 0;
}
