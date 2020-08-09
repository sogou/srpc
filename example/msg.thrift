namespace cpp example

struct MessageResult
{
	1:required string message;
	2:optional i32 state;
	3:optional i32 error;
}

service ExampleThrift
{
	MessageResult Message(1:string message, 2:string name);
}

