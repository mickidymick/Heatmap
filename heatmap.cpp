/* File: heatmap.cpp
 * Author: Zach McMichael
 * Description: makes a heatmap of the used memory 
 *				based on a prerecorded dataset
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
#include <math.h>
#include <boost/dynamic_bitset.hpp>

#define RESET   "\033[0m"     
#define GREEN   "\033[32m" 
#define MAGENTA "\033[35m" 
#define CYAN    "\033[36m"

using namespace std;

class Global {

	public:
		//passed in parameters
		long cache_size; //size of each region for the first phase in the address space
		int counter_size; //size of each counter in the cache
		float keep_percentage; //percentage of total data to keep between phase 1 and phase 2 caches
		float interval; //ammount of time between heatmaping the data
		int verbose; //extra prints
		string dataset_name; //name of the dataset to use

		//for creating the map from the dataset
		map<double, string> ds; //the data set
		map<double, string>::iterator itr; //itterator for the dataset's creation
		map<double, string>::iterator loc; //itterator for the dataset to hold current position
		double last_time_looked_at;
		long total_address_space; //total amount of address space for the dataset
		long rounded_total_address_space; //total amount of address space for the dataset rounded up to the neares kilobyte
		long first_address_as_long;
		string first_address = "FFFFFFFFFFFFFFFF";       //first address in the address space
		string last_address = "0000000000000000";		 //last address in the address space
		string f_last_address = "0000000000000000";		 //final last address in the address space

		//for doing the heatmaping
		long num_cache_regions; //the number of regions in the cache
		double first_time; //the first time in the dataset
		double last_time; //the last time in the dataset
		double timespan; //the total time of the dataset
		long num_itter; //the number of itterations needed to run through the dataset

		//datastructures for cache
		long total_data_size_phase_1; //size of the total amount of data in phase 1
		long total_data_size_phase_2; //size of the total amount of data in phase 2
		long total_data_size_phase_3; //size of the total amount of data in phase 3
		long region_size_phase_1; //size of each region in phase 1
		long region_size_phase_2; //size of each region in phase 2
		long region_size_phase_3; //size of each region in phase 3
		vector<boost::dynamic_bitset<>> phase_1_cache; //the cache for phase 1
		vector<boost::dynamic_bitset<>> phase_2_cache; //the cache for phase 2
		vector<boost::dynamic_bitset<>> phase_3_cache; //the cache for phase 3
		
		//datastructures for memory map
		int phase_2_mmap_cache_bits; //number of bits needed in the mmap to offset into the cache
		int phase_3_mmap_cache_bits; //number of bits needed in the mmap to offset into the cache
		int phase_2_mmap_region_bits; //number of bits needed in the mmap to figure out which region this belongs to
		int phase_3_mmap_region_bits; //number of bits needed in the mmap to figure out which region this belongs to
		int phase_2_mmap_region_zeros; //number of bits needed in the mmap to pad the address with zeros
		int phase_3_mmap_region_zeros; //number of bits needed in the mmap to pad the address with zeros
		typedef decltype(boost::dynamic_bitset<>(1)) cache_offset;
		typedef decltype(boost::dynamic_bitset<>(1)) mmu_offset;
		map<mmu_offset, cache_offset> phase_2_mmap; //the memory map for phase 2
		map<mmu_offset, cache_offset> phase_3_mmap; //the memory map for phase 3
		map<mmu_offset, cache_offset> mmap_itter; //the memory map itterator

	//helper functions	
	long round_next_pwr(long v){
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}
	
	string long_to_string(long address) {
		stringstream ss;
		string address_str;

		ss << hex << address;
		ss >> address_str;
		return address_str;
	}

	long string_to_long(string address) {
		stringstream ss;
		long address_long;

		ss << hex << address;
		ss >> address_long;
		return address_long;
	}

	long calc_size_space(string first_address, string last_address) {
		long first_address_long;
		long last_address_long;
		
		first_address_long = string_to_long(first_address);
		last_address_long = string_to_long(last_address);
		
		return  last_address_long - first_address_long;
	}

	string calc_final_address(long space_size, string first_address) {
		long first_address_int;
		long last_address_int;
		char hex_string[17];
		int calc = 0;
		int loc = 0;
		bool first = true;
		int end = 15;
		stringstream ss;
		
		ss << hex << first_address;
		ss >> first_address_int;
		ss.clear();
	
		first_address_as_long = first_address_int;

		for(int i=0; i<16; i++) {
			hex_string[i] = -1;
		}

		last_address_int = first_address_int + space_size;
		ss << hex << last_address_int;
		ss >> hex_string;

		for(int i=0; i<16; i++) {
			if(hex_string[i] == '\0'){
				if(first){
					first = false;
					loc = i;
				}
				calc++;
			}
		}
		if(calc == 1){
			for(int i=loc-1; i>=0; i--){
				hex_string[end] = tolower(hex_string[i]);
				end--;
			}
			for(int i=0; i<end+1; i++){
				hex_string[i] = '0';
			}
		}
		hex_string[16] = '\0';

		return  hex_string;
	}

	int to_int(int phase, long offset) {
		int ret = 0;
		for(int i=0; i<counter_size; i++) {
			if(phase == 1) {
				ret |= ((phase_1_cache[offset][i])<<i); 
			}else if(phase == 2) {
				ret |= ((phase_2_cache[offset][i])<<i); 
			}else if(phase == 3) {
				ret |= ((phase_3_cache[offset][i])<<i); 
			}else{
				cout << "Error: phase must be 1-3" << endl;
				exit(1);
			}
		}
		return ret;
	}
	
	int increment(int phase, long offset) {
		int value = to_int(phase, offset);
		if(value < (pow(2, counter_size)-1)){
			value++;
			for(int i=0; i<counter_size; i++) {
				if(phase == 1) {
					phase_1_cache[offset][i] = ((value>>i)&1);
				}else if(phase == 2) {
					phase_2_cache[offset][i] = ((value>>i)&1);
				}else if(phase == 3) {
					phase_3_cache[offset][i] = ((value>>i)&1);
				}else{
					cout << "Error: phase must be 1-3" << endl;
					exit(1);
				}
			}
		}
	}

	long find_phase_1_offset(string addr) {
		long address_long;
		stringstream ss;
		
		ss << hex << addr;
		ss >> address_long;
		ss.clear();
		
		return (ceil((address_long-first_address_as_long)/region_size_phase_1)+1);
	}

	void phase_1_increment(long offset) {
		phase_1_cache[offset];
	}

	bool is_not_maxed(int* c_ptr) {
		int i; //for looping
		for(i=0; i<counter_size; i++) {
			if(!((*c_ptr)&(1UL<<i))) {
				return true;
			}
		}
		return false;
	}
	
	//main functions
	void set_mmap_pipeline() {
				
	}

	void init() {
		int i, j; //for looping
		long address; //the address for setting up mmaps
		long add_address; //the address for setting up mmaps
		long incr_address; //the amount to add to the address to increment by region
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_region_bits)) p2_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_region_bits)) p3_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_cache_bits)) p2_cache_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_cache_bits)) p3_cache_offset_size;

		//initialize phase_1_cache
		vector<boost::dynamic_bitset<>> tmp_1(num_cache_regions, boost::dynamic_bitset<>(counter_size));
        phase_1_cache = tmp_1;
		
		//initialize phase_2_cache
		vector<boost::dynamic_bitset<>> tmp_2(num_cache_regions, boost::dynamic_bitset<>(counter_size));
        phase_2_cache = tmp_2;
		
		//initialize phase_3_cache
		vector<boost::dynamic_bitset<>> tmp_3(num_cache_regions, boost::dynamic_bitset<>(counter_size));
        phase_3_cache = tmp_3;
	

		//initialize phase_2_mmap
		incr_address = pow(2, phase_2_mmap_region_zeros);
		address = string_to_long(first_address);
		add_address = (address >> phase_2_mmap_region_zeros);
		
		cout << "Region bits: " << phase_2_mmap_region_bits << endl;
		cout << "Increment by: " << incr_address << endl;
		cout << "First_address: " << long_to_string(address) << endl;
		for(i=0; i<num_cache_regions; i++) {
			boost::dynamic_bitset<> tmp_offset(phase_2_mmap_region_bits);
			for(j=0; j<phase_2_mmap_region_bits; j++) {
				tmp_offset[j] = ((add_address >> (phase_2_mmap_region_bits-1-j))&1);
			}
			boost::dynamic_bitset<> tmp_cache(phase_2_mmap_cache_bits);
			for(j=0; j<phase_2_mmap_cache_bits; j++) {
				tmp_cache[j] = ((i >> (phase_2_mmap_cache_bits-1-j))&1);
			}
			cout << "Offset: " << tmp_offset << "  Cache: " << tmp_cache << endl;
			phase_2_mmap.insert(pair<p2_mmu_offset_size, p2_cache_offset_size>());
			address += incr_address; 
			add_address = (address >> phase_2_mmap_region_zeros);
		}

		//initialize phase_3_mmap
	}

	void run() {
		double finish_here = last_time_looked_at + interval;
		long p1_offset;
		long p1_add_offset;
		int count = 0;
		float mult;
		int* counter_ptr;
		while(loc->first <= finish_here) {
			//phase_1
			p1_offset = find_phase_1_offset(loc->second);
			p1_add_offset = p1_offset*counter_size;
			//counter_ptr = phase_1_cache+p1_add_offset;
			//if(is_not_maxed(counter_ptr)){
			//	(*counter_ptr)++;
			//}else{
			//	if(verbose) cout << "Counter Full" << endl;
			//}

			count++;
			loc++;
		}
		last_time_looked_at = finish_here;
	}

	void heat_map() {

	}
};

int main(int argc, char* argv[]) {
	int i; //for looping
	
	//grab the command line arguments
	if(argc != 7){
		printf("Usage: cache_size(Bytes), counter_size(Bits), keep_percentage(100%), interval(s), verbose(0=N/1=Y), dataset_name(.csv)\n");
		exit(0);
	}
	Global G;
	G.cache_size  = atol(argv[1]); //size of each region for the first phase in the address space
	G.counter_size = atoi(argv[2]); //size of each counter in the cache
	G.keep_percentage = atof(argv[3]); //percentage of total data to keep between phase 1 and phase 2 caches
	G.interval = atof(argv[4]); //ammount of time between heatmaping the data
	G.verbose = atoi(argv[5]); //extra prints
	G.dataset_name = argv[6]; //name of the dataset to use
	

	//create the map from the dataset 
	cout << "Convert Dataset and Calculate Parameters" << endl;
    string line;
    double time;
    string phys_addr;
    stringstream iss;
    string token;
    int where;

    //read in dataset
    ifstream myfile(G.dataset_name);
    if(myfile.is_open()){
        getline(myfile, line); //remove column names

		//grab line
        while(getline(myfile, line)){
            iss << line;
            where = 0;

			//parse for ","
            while(getline(iss, token, ',')){
                if(where == 0){
                    time = stod(token, nullptr);
                }else if(where == 1){
					phys_addr = token;
                }
                where++;
            }
			//add to map
			if(phys_addr < G.first_address) G.first_address = phys_addr;
			if(phys_addr > G.last_address) G.last_address = phys_addr;
            G.ds.insert(pair<double, string>(time, phys_addr));
            iss.clear();
        }
    }
	myfile.close();	

	//calculations
	G.total_address_space = G.calc_size_space(G.first_address, G.last_address);
	G.rounded_total_address_space = G.round_next_pwr(G.total_address_space);
	G.f_last_address = G.calc_final_address(G.rounded_total_address_space, G.first_address);
	G.num_cache_regions = ceil(G.cache_size*(8/(double)G.counter_size));
	
	//phase 1
	G.total_data_size_phase_1 = G.rounded_total_address_space;
	G.region_size_phase_1 = ceil(G.total_data_size_phase_1/G.num_cache_regions);

	//phase 2
	G.total_data_size_phase_2 = ceil(G.total_data_size_phase_1*(G.keep_percentage/100));
	G.region_size_phase_2 = ceil(G.total_data_size_phase_2/G.num_cache_regions);

	//phase 3
	G.total_data_size_phase_3 = ceil(4096*G.num_cache_regions);
	G.region_size_phase_3 = 4096;

	G.first_time = (G.ds.begin())->first;
	G.last_time = (G.ds.rbegin())->first;
	G.timespan = G.last_time-G.first_time;
	G.num_itter = ceil(G.timespan/G.interval);

	//mmap calculations	
	//phase 1
	G.phase_2_mmap_cache_bits = ceil(log2(G.cache_size)); 
	G.phase_2_mmap_region_zeros = ceil(log2(G.region_size_phase_2));
	G.phase_2_mmap_region_bits = ceil(log2(G.total_data_size_phase_1))-G.phase_2_mmap_region_zeros;
	
	//phase 2
	G.phase_3_mmap_cache_bits = ceil(log2(G.cache_size)); 
	G.phase_3_mmap_region_zeros = ceil(log2(G.region_size_phase_3));
	G.phase_3_mmap_region_bits = ceil(log2(G.total_data_size_phase_1))-G.phase_3_mmap_region_zeros;
	
	//initialize the cache
	G.init();	
	
	//initialize the loop vars
	G.loc = G.ds.begin();
	G.last_time_looked_at = G.first_time;
	
	//print variables
	if(G.verbose){
		cout << endl;
		cout << CYAN << "Program Startup Variables" << endl;
		cout << RESET << "Initial First_addr: " << GREEN << G.first_address << endl;	
		cout << RESET << "Initial Last_addr: " << GREEN << G.last_address << endl;	
		cout << RESET << "Total_addr_size: " << GREEN << G.total_address_space << MAGENTA << " Bytes" << endl;	
		cout << endl;
		cout << CYAN << "SETUP CALCULATIONS" << endl;
		cout << RESET << fixed << "First_time: " << GREEN << G.first_time << endl;
		cout << RESET << fixed << "Last_time: " << GREEN << G.last_time << endl;
		cout << RESET << fixed << "Total_timespan: " << GREEN << G.timespan << MAGENTA << " Seconds" << endl;
		cout << RESET << fixed << "Time between heatmapings: " << GREEN << G.interval << MAGENTA << " Seconds" << endl;
		cout << RESET << fixed << "Number of heatmapings to perform: " << GREEN << G.num_itter << endl;
		cout << RESET << "Final First_addr: " << GREEN << G.first_address << endl;	
		cout << RESET << "Final Last_addr: " << GREEN << G.f_last_address << endl;	
		cout << RESET << "Rounded total_addr_size: " << GREEN << G.rounded_total_address_space << MAGENTA << " Bytes" << endl;	
		cout << endl;
		cout << CYAN << "CACHE PARAMS" << endl;
		cout << RESET << "Size of cache: " << GREEN << G.cache_size << MAGENTA << " Bytes" << endl;
		cout << RESET << "Size of each counter: " << GREEN << G.counter_size << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of regions in cache: " << GREEN << G.num_cache_regions << endl;
		cout << endl;
		cout << CYAN << "PHASE 1" << endl;
		cout << RESET << "Total_data_size_phase_1: " << GREEN << G.total_data_size_phase_1 << MAGENTA << " Bytes" << endl;
		cout << RESET << "Region_size_phase_1: " << GREEN << G.region_size_phase_1 << MAGENTA << " Bytes" << endl;
		cout << endl;
		cout << CYAN << "PHASE 2" << endl;
		cout << RESET << "Total_data_size_phase_2: " << GREEN << G.total_data_size_phase_2 << MAGENTA << " Bytes" << endl;
		cout << RESET << "Region_size_phase_2: " << GREEN << G.region_size_phase_2 << MAGENTA << " Bytes" << endl;
		cout << RESET << "Number of bits to address cache in mmap for phase_2: " << GREEN << G.phase_2_mmap_cache_bits << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of zeros in address region in mmap for phase_2: " << GREEN << G.phase_2_mmap_region_zeros << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of bits to address region in mmap for phase_2: " << GREEN << G.phase_2_mmap_region_bits << MAGENTA << " Bits" << endl;
		cout << endl;
		cout << CYAN << "PHASE 3" << endl;
		cout << RESET << "Total_data_size_phase_3: " << GREEN << G.total_data_size_phase_3 << MAGENTA << " Bytes" << endl;
		cout << RESET << "Region_size_phase_3: " << GREEN << G.region_size_phase_3 << MAGENTA << " Bytes" <<  endl;
		cout << RESET << "Number of bits to address cache in mmap for phase_3: " << GREEN << G.phase_3_mmap_cache_bits << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of zeros in address region in mmap for phase_3: " << GREEN << G.phase_3_mmap_region_zeros << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of bits to address region in mmap for phase_3: " << GREEN << G.phase_3_mmap_region_bits << MAGENTA << " Bits" << endl;
		cout << RESET << endl;
	}

	//cout << "Power of two: " << G.round_next_pwr(64000) << endl;

	//start heatmapping
	//for(i=0; i<G.num_itter; i++){
	for(i=1; i<2; i++){
		G.run();
		G.heat_map();
		G.set_mmap_pipeline();
	}
	exit(0);
}
