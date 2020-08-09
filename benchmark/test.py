import os
import time

#serverlist = [("srpc", "pb"), ("brpc", "pb"), ("thrift", "thrift")]
serverlist = [("thrift", "thrift")]
#reqlist = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
parlist = [1, 2, 4, 8, 16, 32, 64, 128, 256]

for server, idl in serverlist:
	#os.system("nohup ./server 8811 %s &" % server)
	for par in parlist:
	#for reqsize in reqlist:
		#cmd = "./echo_client %s" % reqsize
		#cmd = "./client 127.0.0.1 8811 %s %s 100 %s" % (server, idl, reqsize)
		cmd = "./client 127.0.0.1 8811 %s %s %s 1024" % (server, idl, par)
		print cmd
		os.system(cmd);
		time.sleep(1);

	#os.system("killall server")

