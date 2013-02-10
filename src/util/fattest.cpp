#include "library/filesys.hpp"
#include <cstring>
#include <sstream>
#include <iostream>

int main(int argc, char** argv)
{
	filesystem x(argv[1]);
	if(!strcmp(argv[2], "allocate")) {
		uint32_t c = x.allocate_cluster();
		std::cerr << "Allocated cluster " << c << std::endl;
	}
	if(!strcmp(argv[2], "delete")) {
		std::istringstream s(argv[3]);
		uint32_t c;
		s >> c;
		x.free_cluster_chain(c);
	}
	if(!strcmp(argv[2], "skip")) {
		std::istringstream s1(argv[3]);
		std::istringstream s2(argv[4]);
		std::istringstream s3(argv[5]);
		uint32_t c1, c2, c3;
		s1 >> c1;
		s2 >> c2;
		s3 >> c3;
		size_t r = x.skip_data(c1, c2, c3);
		std::cerr << "Skip_data: cluster=" << c1 << " ptr=" << c2 << " amount=" << r << std::endl;
	}
	if(!strcmp(argv[2], "read")) {
		std::istringstream s1(argv[3]);
		std::istringstream s2(argv[4]);
		std::istringstream s3(argv[5]);
		uint32_t c1, c2, c3;
		s1 >> c1;
		s2 >> c2;
		s3 >> c3;
		char* buf = (char*)calloc(c3 + 1, 1);
		size_t r = x.read_data(c1, c2, buf, c3);
		std::cerr << "Read_data: cluster=" << c1 << " ptr=" << c2 << " amount=" << r << std::endl;
		std::cerr << buf << std::endl;
	}
	if(!strcmp(argv[2], "write")) {
		std::istringstream s1(argv[3]);
		std::istringstream s2(argv[4]);
		uint32_t c1, c2, c4, c5;
		s1 >> c1;
		s2 >> c2;
		x.write_data(c1, c2, argv[5], strlen(argv[5]), c4, c5);
		std::cerr << "Write_data: cluster=" << c1 << " ptr=" << c2 << " rcluster=" << c4
			<< " rptr=" << c5 << std::endl;
	}
}
