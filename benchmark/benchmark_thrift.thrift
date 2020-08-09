service BenchmarkThrift
{
	void echo_thrift(1:string msg);
	void slow_thrift(1:string msg);
}

