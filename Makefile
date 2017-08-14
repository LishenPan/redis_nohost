CC=g++

CFLAGS	+=\
			-g\
			-std=c++11\
			-DCPP\
			-Wwrite-strings\
#			-DLIBLSM\

INCLUDES :=	-I$(PWD)\
		-I$(PWD)/../lsmtree_nohost\

LIBS 	:=\
			-L../lsmtree_nohost\
			-llsm\
			-lpthread\

SRCS	:=\
			$(PWD)/command.c\
			$(PWD)/request.c\
			$(PWD)/poll_server.cpp\
			$(PWD)/udp_server.cpp\
			$(PWD)/master_server.cpp\

OBJS	:=\
			$(SRCS:.c=.o) $(SRCS:.cpp=.o)
			
poll_server : 	poll_server.o command.o request.o
			$(CC) $(INCLUDES) -o poll_server poll_server.o command.o request.o $(LIBS) 

udp_server : 	udp_server.o command.o request.o
			$(CC) $(INCLUDES) -o udp_server udp_server.o command.o request.o $(LIBS)

master_server :	master_server.o command.o request.o
			$(CC) $(INCLUDES) -o master_server master_server.o command.o request.o $(LIBS)

.c.o	:
			$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

.cpp.o	:
			$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@


clean	:
			@$(RM) *.o
			@$(RM) poll_server udp_server master_server 
			
