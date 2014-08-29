/*****************************************************************************
 *                                McPAT
 *                      SOFTWARE LICENSE AGREEMENT
 *            Copyright 2012 Hewlett-Packard Development Company, L.P.
 *                          All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.”
 *
 ***************************************************************************/
#include "io.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "xmlParser.h"
#include "XML_Parse.h"
#include "processor.h"
#include "globalvar.h"
#include "version.h"

// For Socket Operation
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#define UNIX_PATH_MAX 108
#define SOCKET_CMD_MAX_LEN 256


using namespace std;

void print_usage(char * argv0);

int socket_fd = 0;
int pid = 1;
struct sockaddr_un address;

void close_socket()
{
	// don't close socket if I am a child process
	if (socket_fd && pid)
	{
		cout << "Closing socket...\n"; 
		close(socket_fd);
		unlink(address.sun_path);
		socket_fd = 0;
	}
}

void sigint_handler(int s)
{
	close_socket();
}

int main(int argc,char *argv[])
{
	char * fb ;
	bool use_socket = false;
	bool infile_specified     = false;
	int  plevel               = 2;
	opt_for_clk	=true;
	//cout.precision(10);
	if (argc <= 1 || argv[1] == string("-h") || argv[1] == string("--help"))
	{
		print_usage(argv[0]);
	}

	for (int32_t i = 0; i < argc; i++)
	{
		if (argv[i] == string("-infile"))
		{
			infile_specified = true;
			i++;
			fb = argv[ i];
		}

		else if (argv[i] == string("-print_level"))
		{
			i++;
			plevel = atoi(argv[i]);
		}

		else if (argv[i] == string("-opt_for_clk"))
		{
			i++;
			opt_for_clk = (bool)atoi(argv[i]);
		}
		
		else if (argv[i] == string("-socket"))
		{
			infile_specified = true;
			use_socket = true;
		}
		
		else if (i > 0)
		{
			cerr << "Warning: unrecognized argument " << argv[i] << endl;
		}
	}
	if (infile_specified == false)
	{
		print_usage(argv[0]);
	}


	cout<<"McPAT (version "<< VER_MAJOR <<"."<< VER_MINOR
		<< " of " << VER_UPDATE << ") is computing the target processor...\n "<<endl;
	
	// Open an Unix domain socket to listen requests
	if (use_socket)
	{
		int connection_fd;
 		socklen_t address_length;
		
		// Creat an Unix domain socket
		socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
		if(socket_fd < 0)
		{
			cerr << "socket() failed\n";
			return 1;
		}
		
		// generate an uniqe socket name and bind() to it!		
		memset(&address, 0, sizeof(struct sockaddr_un));
		address.sun_family = AF_UNIX;
 		snprintf(address.sun_path, UNIX_PATH_MAX, "/tmp/mcpat-%d", getpid());
 		unlink(address.sun_path);
 		cout << "Creating socket at " << address.sun_path << endl;
 		if(bind(socket_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0)
		{
			cerr << "bind() failed\n";
			close(socket_fd);
			return 1;
		}
		
		if(listen(socket_fd, 5) != 0)
		{
			cerr << "listen() failed\n";
			close(socket_fd);
			unlink(address.sun_path);
			return 1;
		}
		
		atexit(close_socket);
		struct sigaction sigIntHandler;
		sigIntHandler.sa_handler = sigint_handler;
		sigemptyset(&sigIntHandler.sa_mask);
		sigIntHandler.sa_flags = 0;
		sigaction(SIGINT, &sigIntHandler, NULL);
		// handle SIGCHLD and avoid zombies
		signal(SIGCHLD, SIG_IGN);
		
		while((connection_fd = accept(socket_fd, (struct sockaddr *) &address, &address_length)) > -1)
		{
			if ((pid = fork()) == -1)
			{
				cerr << "Cannot fork\n";
				close(connection_fd);
				continue;
			}
			else if(pid > 0)
			{
				// parent process
				close(connection_fd);
				continue;
			}
			else if(pid == 0)
			{
				// child process
				char buffer[SOCKET_CMD_MAX_LEN];
			
				memset(&buffer, 0, SOCKET_CMD_MAX_LEN * sizeof(char));
				read(connection_fd, buffer, SOCKET_CMD_MAX_LEN);
			
				cout << "Received: " << buffer << endl;
				string if_socket;
				string of_socket(buffer);
				ofstream output_file;
			
				// first argument: input file name
				if_socket = of_socket.substr(0, of_socket.find(","));
				of_socket.erase(0, of_socket.find(",") + 1);
				// second argument: print_level
				plevel = atoi(of_socket.substr(0, of_socket.find(",")).c_str());
				of_socket.erase(0, of_socket.find(",") + 1);
				// third argument: opt_clk
				opt_for_clk = (bool)atoi(of_socket.substr(0, of_socket.find(",")).c_str());
				of_socket.erase(0, of_socket.find(",") + 1);
				of_socket.erase(of_socket.find("\n") == -1 ? of_socket.length() : of_socket.find("\n"), of_socket.length());
				// the rest string is output file name
				cout << "Processing file "  << if_socket << ", output file is " << of_socket
				 << " (Options: print_level = " << plevel << ", opt_for_clk = " << opt_for_clk << ")" << endl;
			
				//parse XML-based interface
				ParseXML *p1= new ParseXML();
				fb = new char [if_socket.length()+1];
				strcpy (fb, if_socket.c_str());
				p1->parse(fb);
				Processor proc(p1);
			
				// redirect cout and run displayEnergy()
				cout.flush();
				ostringstream new_cout;
				streambuf* orig_cout_rdbuf = cout.rdbuf();
				cout.rdbuf( new_cout.rdbuf() );
				proc.displayEnergy(2, plevel);
			
				// write this streambuf to file
				output_file.open(of_socket.c_str(), ofstream::out);
				if(output_file.is_open())
				{
					output_file << new_cout.str();
					output_file.flush();
					output_file.close();
				}
				else
				{
					cerr << "Cannot write output to " << of_socket;
				}
				cout.rdbuf( orig_cout_rdbuf );
				write(connection_fd, "Done.\n", 6);
				cout << "Done." << endl;
			
				delete p1;
				delete fb;
			
				close(connection_fd);
				// exit child process
				return 0;
			}
			
		}
		close_socket();
	}
	else
	{
		//parse XML-based interface
		ParseXML *p1= new ParseXML();
		p1->parse(fb);
		Processor proc(p1);
		proc.displayEnergy(2, plevel);
		delete p1;
	}
	return 0;
}

void print_usage(char * argv0)
{
    cerr << "How to use McPAT:" << endl;
    cerr << "  mcpat -infile <input file name>  -print_level < level of details 0~5 >  -opt_for_clk < 0 (optimize for ED^2P only)/1 (optimzed for target clock rate)>"<< endl;
    //cerr << "    Note:default print level is at processor level, please increase it to see the details" << endl;
    exit(1);
}
