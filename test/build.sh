./clear.sh

#g++ -std=c++11 -O3 -o echo_server echo_server.cc -lrt -lpthread
#g++ -std=c++11 -O3 -o echo_client echo_client.cc -lrt -lpthread

g++ -std=c++11 -O3 -o camera_server camera_server.cc -lrt -lpthread
g++ -std=c++11 -O3 -o camera_client camera_client.cc -lrt -lpthread
