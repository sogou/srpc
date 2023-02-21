struct EchoResult {
	1:required string message;
	2:optional i32 state;
	3:optional i32 error;
}

service %s {
	EchoResult Echo(1:string message);
}

