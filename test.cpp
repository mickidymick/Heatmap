/* File:
 * Author: Zach McMichael
 * Description:
 */
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <sstream>
#include <utility>
#include <functional>
#include <bitset>
#include <boost/dynamic_bitset.hpp>

using namespace std;
	
struct bit_f{
	unsigned int b : 3;
};

//vector<boost::dynamic_bitset<>> d_arr(10, boost::dynamic_bitset<>(3));
	
class me {
	public:
		typedef decltype(boost::dynamic_bitset<>(1)) Bitset4;
		typedef decltype(boost::dynamic_bitset<>(1)) Bitset8;
		int n;
		vector<boost::dynamic_bitset<>> d_arr;
		map<Bitset4, Bitset8> mmap;
		map<Bitset4, Bitset8>::iterator m_iter;

		me(int num, int size) {
			n = num;
			init(size, num);
		}
		
		void init(int size, int num_bits) {
			vector<boost::dynamic_bitset<>> tmp(10, boost::dynamic_bitset<>(num_bits));
			d_arr = tmp;
		}

		int to_int(int index, int num_bits) {
			int ret = 0;
			for(int i=0; i<num_bits; i++) {
				ret |= ((d_arr[index][i])<<i);
			}
			return ret;
		}

		void insert(int index, int value, int num_bits) {
			for(int i=0; i<num_bits; i++) {
				d_arr[index][i] = ((value>>i)&1);
			}
		}

		void increment(int index, int num_bits) {
			int value = to_int(index, num_bits);
			value++;
			for(int i=0; i<num_bits; i++) {
				d_arr[index][i] = ((value>>i)&1);
			}
		}

		int to_int_f(int num_bits) {
			int ret = 0;
			for(int i=0; i<num_bits; i++) {
				ret |= (((mmap.begin()->first)[i])<<i);
			}
			return ret;
		}
		
		int to_int_s(int num_bits) {
			int ret = 0;
			for(int i=0; i<num_bits; i++) {
				ret |= (((mmap.begin()->second)[i])<<i);
			}
			return ret;
		}
};

void init(int var) {
	typedef decltype(boost::dynamic_bitset<>(var)) Bitset4;
}

int main(int argc, char* argv[]) {
	int num = atoi(argv[1]);
	int num1 = atoi(argv[2]);
	
	typedef decltype(boost::dynamic_bitset<>(4)) Bitset4;
	typedef decltype(boost::dynamic_bitset<>(num1)) Bitset8;
	
	init(num);	
	
	me m(num, 10);
		
	cout << "Basic version" << endl;
	bit_f arr[10] = {};
	cout << arr[0].b << endl;
	cout << arr[5].b << endl;
	cout << arr[9].b << endl;
	arr[9].b = 6;
	cout << arr[9].b << endl;
	(arr[9].b)++;
	cout << arr[9].b << endl;
	(arr[9].b)++;
	cout << arr[9].b << endl;

	cout << endl << "Dynamic version" << endl; 
	cout << m.d_arr[0] << endl;
	cout << m.d_arr[5] << endl;
	cout << m.d_arr[9] << endl;
	m.insert(9, 6, num);
	cout << m.d_arr[9] << endl;
	int t = m.to_int(9, num);
	cout << t << endl;
	m.increment(9, num);
	cout << m.d_arr[9] << endl;
	m.increment(9, num);
	cout << m.d_arr[9] << endl;

	cout << endl <<  "Map Dynamic" << endl;
	boost::dynamic_bitset<> offset(4);
	offset[1] = 1;
	boost::dynamic_bitset<> page_map(8);
	page_map[4] = 1;
	m.mmap.insert(pair<Bitset4, Bitset8>(offset, page_map));
	offset.reset();
	offset[3] = 1;
	page_map.reset();
	page_map[2] = 1;
	page_map[6] = 1;
	m.mmap.insert(pair<Bitset4, Bitset8>(offset, page_map));
	cout << m.to_int_f(4) << endl;
	cout << m.to_int_s(8) << endl;

	int ret = 0;
	m.m_iter = m.mmap.begin();
	m.m_iter++;
	for(int i=0; i<4; i++) {
		ret |= (((m.m_iter->first)[i])<<i);
	}
	cout << ret << endl;
	
	for(int i=0; i<8; i++) {
		ret |= (((m.m_iter->second)[i])<<i);
	}
	cout << ret << endl;
	
	
	
	
	exit(0);
}
