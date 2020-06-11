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
#include <getopt.h>

#define RESET   "\033[0m"     
#define RED     "\033[31m" 
#define GREEN   "\033[32m" 
#define MAGENTA "\033[35m" 
#define CYAN    "\033[36m"

using namespace std;

class Global {

	public:
    //hard coded vars
		int debug; //for extra prints for debugging
    int num_bits_addressable; //the number of bits needed to address the entire mem space

		//passed in parameters
		float interval; //ammount of time between heatmaping the data
		int verbose; //extra prints
    string dataset_name; //name of the dataset

		//for creating the map from the dataset
		uint64_t first_address_as_uint64_t;

		//datastructures for cache
    int num_region_bits_phase_1; //number of bits needed to address all addresses in region
    int num_region_bits_phase_2; //number of bits needed to address all addresses in region
    int num_region_bits_phase_3; //number of bits needed to address all addresses in region
    int counter_size_phase_1; //size of the number of bits in for the counter in the phase 1 cache
    int counter_size_phase_2; //size of the number of bits in for the counter in the phase 2 cache
    int counter_size_phase_3; //size of the number of bits in for the counter in the phase 3 cache
    uint64_t cache_size_phase_1; //size of the cache in bits for phase 1
    uint64_t cache_size_phase_2; //size of the cache in bits for phase 2
    uint64_t cache_size_phase_3; //size of the cache in bits for phase 3
		uint64_t total_data_size_phase_1; //size of the total amount of data in phase 1
		uint64_t total_data_size_phase_2; //size of the total amount of data in phase 2
		uint64_t total_data_size_phase_3; //size of the total amount of data in phase 3
		uint64_t region_size_phase_1; //size of each region in phase 1
		uint64_t region_size_phase_2; //size of each region in phase 2
		uint64_t region_size_phase_3; //size of each region in phase 3
    uint64_t num_cache_regions_phase_1; //number of regions in the phase 1 cache
    uint64_t num_cache_regions_phase_2; //number of regions in the phase 2 cache
    uint64_t num_cache_regions_phase_3; //number of regions in the phase 3 cache
		vector<boost::dynamic_bitset<>> phase_1_cache; //the cache for phase 1
		vector<boost::dynamic_bitset<>> phase_2_cache; //the cache for phase 2
		vector<boost::dynamic_bitset<>> phase_3_cache; //the cache for phase 3
		
		//datastructures for memory map
		int phase_2_mmap_cache_bits; //number of bits needed in the mmap to offset into the cache
		int phase_3_mmap_cache_bits; //number of bits needed in the mmap to offset into the cache
		int phase_2_mmap_region_bits; //number of bits needed in the mmap to figure out which region this beuint64_ts to
		int phase_3_mmap_region_bits; //number of bits needed in the mmap to figure out which region this beuint64_ts to
		int phase_2_mmap_region_zeros; //number of bits needed in the mmap to pad the address with zeros
		int phase_3_mmap_region_zeros; //number of bits needed in the mmap to pad the address with zeros
		typedef decltype(boost::dynamic_bitset<>(1)) cache_offset;
		typedef decltype(boost::dynamic_bitset<>(1)) mmu_offset;
		map<mmu_offset, cache_offset> phase_2_mmap; //the memory map for phase 2
		map<mmu_offset, cache_offset> phase_3_mmap; //the memory map for phase 3

	//##### helper functions #####
  pair<string, string> get_log2_size(uint64_t num) {
    string s_num;
    string s_type;
    if(num > pow(2,49)) {
      cout << "Size is invalid" << endl;
      exit(1);
    }else if(num > pow(2, 39)) {
      s_num = to_string(num/1000000000000);
      s_type = " Terabytes";
      return make_pair(s_num, s_type);
    }else if(num > pow(2, 29)) {
      s_num = to_string(num/1000000000);
      s_type = " Gigabytes";
      return make_pair(s_num, s_type);
    }else if(num > pow(2, 19)) {
      s_num = to_string(num/1000000);
      s_type = " Megabytes";
      return make_pair(s_num, s_type);
    }else if(num > pow(2, 9)) {
      s_num = to_string(num/1000);
      s_type = " Kilobytes";
      return make_pair(s_num, s_type);
    }else {
      s_num = to_string(num);
      s_type = " Bytes";
      return make_pair(s_num, s_type);
    }
  }
  /*
  //Finds the highest power of 2
	uint64_t round_next_pwr(uint64_t v){
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}
	
	//converts an address as a uint64_t to a string
	string uint64_t_to_string(uint64_t address) {
		stringstream ss;
		string address_str;

		ss << hex << address;
		ss >> address_str;
		return address_str;
	}
  */

	//Converts an address as a string to a uint64_t
	uint64_t string_to_uint64_t(string address) {
		stringstream ss;
		uint64_t address_uint64_t;

		ss << hex << address;
		ss >> address_uint64_t;
		return address_uint64_t;
	}

  /*
	//calculates the total amount of bytes
	uint64_t calc_size_space(string first_address, string last_address) {
		uint64_t first_address_uint64_t;
		uint64_t last_address_uint64_t;
		
		first_address_uint64_t = string_to_uint64_t(first_address);
		last_address_uint64_t = string_to_uint64_t(last_address);
		
		return  last_address_uint64_t - first_address_uint64_t;
	}

	//calculate the last address with the added rounding
	string calc_final_address(uint64_t space_size, string first_address) {
		uint64_t first_address_int;
		uint64_t last_address_int;
		char hex_string[17];
		int calc = 0;
		int loc = 0;
		bool first = true;
		int end = 15;
		stringstream ss;
		
		ss << hex << first_address;
		ss >> first_address_int;
		ss.clear();
	
		first_address_as_uint64_t = first_address_int;

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

  */
	//convert the counter to a uint64_t
	int to_uint64_t(int phase, uint64_t offset) {
		uint64_t ret = 0;
    uint64_t counter_size;
    if(phase == 1) {
      counter_size = counter_size_phase_1;
    }else if(phase == 2) {
      counter_size = counter_size_phase_2;
    }else{
      counter_size = counter_size_phase_3;
    }

    for(int i=0; i<counter_size; i++) {
			if(phase == 1) {
				ret |= ((phase_1_cache[offset][counter_size-i-1])<<i); 
			}else if(phase == 2) {
				ret |= ((phase_2_cache[offset][counter_size-i-1])<<i); 
			}else if(phase == 3) {
				ret |= ((phase_3_cache[offset][counter_size-i-1])<<i); 
			}else{
				cout << "Error: phase must be 1-3" << endl;
				exit(1);
			}
		}
		return ret;
	}
	
	//increment the counter by one if it is not maxed
	int increment(int phase, uint64_t offset) {
		uint64_t value = to_uint64_t(phase, offset);
    uint64_t counter_size;
    if(phase == 1) {
      counter_size = counter_size_phase_1;
    }else if(phase == 2) {
      counter_size = counter_size_phase_2;
    }else{
      counter_size = counter_size_phase_3;
    }
		
    if(value < (pow(2, counter_size)-1)){
			value++;
			for(int i=0; i<counter_size; i++) {
				if(phase == 1) {
					phase_1_cache[offset][counter_size-i-1] = ((value>>i)&1);
				}else if(phase == 2) {
					phase_2_cache[offset][counter_size-i-1] = ((value>>i)&1);
				}else if(phase == 3) {
					phase_3_cache[offset][counter_size-i-1] = ((value>>i)&1);
				}
			}   
			//if(debug)cout << "Counter: " << GREEN << to_uint64_t(1, offset) << RESET << endl;
			return 1;
		}else{
			return 0;
		}
	}

	//find the offset at the specified address for the phase
	uint64_t find_offset(int phase, uint64_t address_uint64_t) {
		int i;
		uint64_t offset = 0;
		stringstream ss;
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_region_bits)) p2_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_region_bits)) p3_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_cache_bits)) p2_cache_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_cache_bits)) p3_cache_offset_size;
		map<p2_mmu_offset_size, p2_cache_offset_size>::iterator p2_mmap_itter; //the memory map itterator
		map<p3_mmu_offset_size, p3_cache_offset_size>::iterator p3_mmap_itter; //the memory map itterator
		uint64_t test_a = 0;
		
		if(phase == 1) {
      //return address_uint64_t >> num_region_bits_phase_1;
			return ceil(address_uint64_t/region_size_phase_1);
		}else if(phase == 2) {
			//convert address to bitset for lookup in mmap
			if(1) cout << "Address uint64_t: " << address_uint64_t << "  Address Hex: " 
				 << hex << address_uint64_t << dec << endl;
			address_uint64_t = address_uint64_t >> phase_2_mmap_region_zeros;
			boost::dynamic_bitset<> tmp_bitset_r(phase_2_mmap_region_bits);
			for(i=0; i<phase_2_mmap_region_bits; i++) {
				tmp_bitset_r[phase_2_mmap_region_bits-i-1] = ((address_uint64_t)>>i)&1;
				test_a |= tmp_bitset_r[phase_2_mmap_region_bits-i-1]<<i;
			}
			//for printing
			if(1) cout << "tmp_bitset_r: " << fixed << test_a << "  Hex: " << hex << test_a 
				 << dec << endl;

			//find in mmap and convert from bitset to uint64_t
			boost::dynamic_bitset<> tmp_bitset_c(phase_2_mmap_cache_bits);
			p2_mmap_itter = phase_2_mmap.find(tmp_bitset_r);
			if(p2_mmap_itter == phase_2_mmap.end()) {
				cout << RED << "Address not found in phase_2_mmap" << RESET << endl;
        return -1;
      }else{
				tmp_bitset_c = p2_mmap_itter->second;
        for(i=0; i<phase_2_mmap_cache_bits; i++) {
          offset |= (tmp_bitset_c[phase_2_mmap_cache_bits-i-1]<<i); 
        }
        ////////////////////
        /*
          boost::dynamic_bitset<> tmp_cache(phase_2_mmap_cache_bits);
          for(j=0; j<phase_2_mmap_cache_bits; j++) {
            print_l_c |= (tmp_cache[phase_2_mmap_cache_bits-j-1]<<j);
          }
        */
        ////////////////////
				cout << GREEN << "Address found--  Index" << MAGENTA << tmp_bitset_c << RESET "  Long: " << GREEN << offset << RESET << endl;
        return offset;
			}

		}else if(phase == 3) {

		}
	}

  /*
	//calculate if the counter is maxed out
	bool is_not_maxed(int* c_ptr) {
		int i; //for looping
		for(i=0; i<counter_size; i++) {
			if(!((*c_ptr)&(1UL<<i))) {
				return true;
			}
		}
		return false;
	}
  */

	//##### main functions #####
	//initilize the maps and vectors
	void init() {
    //initialize phase_1_cache
		vector<boost::dynamic_bitset<>> tmp_1(num_cache_regions_phase_1, boost::dynamic_bitset<>(counter_size_phase_1));
        phase_1_cache = tmp_1;
		
		//initialize phase_2_cache
		vector<boost::dynamic_bitset<>> tmp_2(num_cache_regions_phase_2, boost::dynamic_bitset<>(counter_size_phase_2));
        phase_2_cache = tmp_2;
		
		//initialize phase_3_cache
		vector<boost::dynamic_bitset<>> tmp_3(num_cache_regions_phase_3, boost::dynamic_bitset<>(counter_size_phase_3));
        phase_3_cache = tmp_3;

    /*

    */
	}

  /*
	//iterate over the data set for a single interval amount of time and increment 
	//the counters for all three runs
	void run() {
		double finish_here = last_time_looked_at + interval;
		uint64_t offset;
		uint64_t p1_add_offset;
		int count = 0;
		uint64_t counter;
		boost::dynamic_bitset<> counter_bitset(counter_size);

		//TEST#####################
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_region_bits)) p2_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_cache_bits)) p2_cache_offset_size;
		map<p2_mmu_offset_size, p2_cache_offset_size>::iterator p2_mmap_itter; //the memory map itterator
		p2_mmu_offset_size first_b;
		p2_cache_offset_size second_b;
		uint64_t first = 0;
		uint64_t second = 0;
		p2_mmap_itter = phase_2_mmap.begin();
		if(p2_mmap_itter != phase_2_mmap.end()){
			cout << GREEN << "GOOD" << RESET << endl;
		}else{
			cout << RED << "BAD" << RESET << endl;
		}
		first_b = p2_mmap_itter->first;
		second_b = p2_mmap_itter->second;
		for(int i=0; i<phase_2_mmap_region_bits; i++) {
			first |= (first_b[phase_2_mmap_region_bits-i-1])<<i;
			cout << "i=" << i << " First: " << first_b[i] << endl;
		}
		for(int i=0; i<phase_2_mmap_cache_bits; i++) {
			second |= (second_b[phase_2_mmap_cache_bits-i-1])<<i;
			cout << "i=" << i << " Second: " << second_b[i] << endl;
		}
		cout << "First in map: " << first << ", " << hex << first << "  Second: " << dec << second << ", " <<  hex << second << endl;
		exit(0);
		//TEST#####################
		
		while(loc->first <= finish_here) {
			//phase_1
			offset = find_offset(1, loc->second);
			counter_bitset = phase_1_cache[offset];
			counter = to_uint64_t(1, offset);
			if(increment(1, offset)) {
				if(verbose) cout << "Phase 1" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << endl;
			}else{
				if(verbose) cout << "Phase 1" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << RED <<"  --FULL--" 
					<< RESET << endl;
			}

			//phase_2
			offset = find_offset(2, loc->second);
			if(offset != -1) {
				cout << "P2 Offset: " << GREEN << offset << RESET << endl;
				counter_bitset = phase_2_cache[offset];
				counter = to_uint64_t(2, offset);
				if(increment(2, offset)) {
					if(verbose) cout << GREEN << "Phase 2" << "  Offset: " << fixed << hex << offset 
						<< "  Counter: " << dec << counter << endl;
				}else{
					if(verbose) cout << GREEN << "Phase 2" << "  Offset: " << fixed << hex << offset 
						<< "  Counter: " << dec << counter << RED <<"  --FULL--" 
						<< RESET << endl;
				}
			}else{
				if(verbose) cout << "Phase 2" << "  Offset: " << fixed << hex << 0 
					<< "  Counter: " << dec << 0 << CYAN <<"  --Not In mmap--" 
					<< RESET << endl;
			}
			cout << endl;
			count++;
			loc++;
		}
		last_time_looked_at = finish_here;
	}
  */
  //parse the L1, L2, L3 args
  void parse(char* l1, char* l2, char* l3) {
    string tmp_str;
    string l1_str(l1);
    string l2_str(l2);
    string l3_str(l3);
    stringstream l1_s_str(l1_str);
    stringstream l2_s_str(l2_str);
    stringstream l3_s_str(l3_str);

    //phase_1
    getline(l1_s_str, tmp_str, ',');
    num_bits_addressable = stol(tmp_str);
    num_region_bits_phase_1 = stol(tmp_str);
    total_data_size_phase_1 = pow(2,stol(tmp_str));
    getline(l1_s_str, tmp_str, ',');
    region_size_phase_1 = pow(2, stol(tmp_str));
    getline(l1_s_str, tmp_str, ',');
    counter_size_phase_1 = stol(tmp_str);
    num_cache_regions_phase_1 = total_data_size_phase_1/region_size_phase_1;
    cache_size_phase_1 = num_cache_regions_phase_1*((float)counter_size_phase_1/8);
    
    //phase_2
    getline(l2_s_str, tmp_str, ',');
    num_region_bits_phase_2 = stol(tmp_str);
    total_data_size_phase_2 = pow(2, stol(tmp_str));
    getline(l2_s_str, tmp_str, ',');
    region_size_phase_2 = pow(2, stol(tmp_str));
    getline(l2_s_str, tmp_str, ',');
    counter_size_phase_2 = stol(tmp_str);
    num_cache_regions_phase_2 = total_data_size_phase_2/region_size_phase_2;
    cache_size_phase_2 = num_cache_regions_phase_2*((float)counter_size_phase_2/8);
    
    //phase_3
    getline(l3_s_str, tmp_str, ',');
    num_region_bits_phase_3 = stol(tmp_str);
    total_data_size_phase_3 = pow(2, stol(tmp_str));
    getline(l3_s_str, tmp_str, ',');
    region_size_phase_3 = pow(2, stol(tmp_str));
    getline(l3_s_str, tmp_str, ',');
    counter_size_phase_3 = stol(tmp_str);
    num_cache_regions_phase_3 = total_data_size_phase_3/region_size_phase_3;
    cache_size_phase_3 = num_cache_regions_phase_3*((float)counter_size_phase_3/8);
  }

  //increment the counter in the cache of the desired phase
  void change_counter(int phase, uint64_t offset) {
    uint64_t counter;

    //phase 1
    if(phase == 1) {		
		  boost::dynamic_bitset<> counter_bitset(counter_size_phase_1);
      counter_bitset = phase_1_cache[offset];
			if(increment(1, offset)) {
        counter = to_uint64_t(1, offset);
				if(verbose) cout << "Phase 1" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << endl;
			}else{
        counter = to_uint64_t(1, offset);
				if(verbose) cout << "Phase 1" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << RED <<"  --FULL--" 
					<< RESET << endl;
			}
    //phase 2
    }else if(phase == 2) {
		  boost::dynamic_bitset<> counter_bitset(counter_size_phase_2);
      counter_bitset = phase_2_cache[offset];
			if(increment(2, offset)) {
        counter = to_uint64_t(2, offset);
				if(1) cout << "Phase 2" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << endl;
			}else{
        counter = to_uint64_t(2, offset);
				if(1) cout << "Phase 2" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << RED <<"  --FULL--" 
					<< RESET << endl;
      }
    //phase 3
    }else {
		  boost::dynamic_bitset<> counter_bitset(counter_size_phase_3);
      counter_bitset = phase_3_cache[offset];
			if(increment(3, offset)) {
        counter = to_uint64_t(3, offset);
				if(verbose) cout << "Phase 3" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << endl;
			}else{
        counter = to_uint64_t(3, offset);
				if(verbose) cout << "Phase 3" << "  Offset: " << hex << offset 
					<< "  Counter: " << dec << counter << RED <<"  --FULL--" 
					<< RESET << endl;
      }
    }
  }

  //run the heatmap
  void heatmap(uint64_t iteration) {
		int i, j, k; //for looping
    bool done = false;
    typedef pair<uint64_t, uint64_t> pairs; //to add to sets (the number, the index)
    vector<pairs> max; //maximum number found
    uint64_t index = 0;
    uint64_t num; //number to check size of 
    uint64_t num_regions_needed_phase_2; //number of regions of phase1 to fill phase2
    uint64_t num_regions_needed_phase_3; //number of regions of phase2 to fill phase3
    uint64_t num_regions_per_phase_2; //number of regions of phase1 to fill phase2
    uint64_t num_regions_per_phase_3; //number of regions of phase2 to fill phase3
    uint64_t start_addr; //starting address for region
		uint64_t print_l_r; //to print the bits as a uint64_t
		uint64_t print_l_c; //to print the bits as a uint64_t
		uint64_t address; //the address for setting up mmaps
		uint64_t add_address; //the address for setting up mmaps
		uint64_t incr_address; //the amount to add to the address to increment by region
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_region_bits)) p2_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_region_bits)) p3_mmu_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_2_mmap_cache_bits)) p2_cache_offset_size;
		typedef decltype(boost::dynamic_bitset<>(phase_3_mmap_cache_bits)) p3_cache_offset_size;
    /*
    //phase 3
    if(iteration >= 2) {
      //initialize phase_3_mmap
      incr_address = pow(2, phase_3_mmap_region_zeros);
      add_address = (first_address_as_uint64_t >> phase_3_mmap_region_zeros);
      
      for(i=0; i<num_cache_regions_phase_3; i++) {
        boost::dynamic_bitset<> tmp_offset(phase_3_mmap_region_bits);
        print_l_r = 0;
        print_l_c = 0;
        for(j=0; j<phase_3_mmap_region_bits; j++) {
          tmp_offset[phase_3_mmap_region_bits-j-1] = ((add_address>>j)&1);
          print_l_r |= (tmp_offset[phase_3_mmap_region_bits-j-1]<<j);
        }

        boost::dynamic_bitset<> tmp_cache(phase_3_mmap_cache_bits);
        for(j=0; j<phase_3_mmap_cache_bits; j++) {
          tmp_cache[phase_3_mmap_cache_bits-j-1] = ((i>>j)&1);
          print_l_c |= (tmp_cache[phase_3_mmap_cache_bits-j-1]<<j);
        }
        if(debug) cout << "Phase 3 mmap-> " << "Offset: " << fixed << hex << print_l_r 
          << ", " << dec << print_l_r << " -- Cache: " << fixed << hex << print_l_c 
          << ", " << dec << print_l_c << endl;
        phase_3_mmap.insert(pair<p3_mmu_offset_size, p3_cache_offset_size>(tmp_offset, tmp_cache));
        address += incr_address; 
        add_address = (address >> phase_3_mmap_region_zeros);
      }
    }
    */
    //phase 2
    if(iteration >= 1) {
      //clear mmap and cache
      phase_2_mmap.clear();


      //calculate number of regions to accuire
      num_regions_needed_phase_2 = total_data_size_phase_2/region_size_phase_1;
      //find hot regions in phase 1
      for(i=0; i<num_cache_regions_phase_1; i++) {
        num = to_uint64_t(1, i);
        //cout << "iter: " << GREEN << i << RESET << "  num: " << GREEN << num << RESET << endl;
        if(max.size() < num_regions_needed_phase_2) {
          max.push_back(make_pair(num, i));
        }else{
          for(j=0; j<max.size(); j++) {
            if(max[j].first < num) {
              //cout << MAGENTA << "OLD" << RESET << "  iter: " << GREEN << max[j].second 
              //     << RESET << "  num: " << GREEN << max[j].first << endl;
              //cout << MAGENTA << "NEW" << RESET << "  iter: " << GREEN << i << RESET 
              //     << "  num: " << GREEN << num << RESET << endl;
              max[j] = make_pair(num, i);
              break;
            }
          }
        }
        if(max.size() == num_regions_needed_phase_2) {
          for(j=0; j<max.size(); j++) {
            if(max[j].first != (pow(2, counter_size_phase_1)-1)){
              //cout << "reg_bits: " << counter_size_phase_1 << endl;
              //cout << "max: " << (pow(2, counter_size_phase_1)-1) << endl;
              //cout << MAGENTA << "SMALL" << RESET << "  iter: " << GREEN << max[j].second 
              //     << RESET << "  num: " << GREEN << max[j].first << RESET << endl;
              break;  
            }else if(j == max.size()-1){
              //cout << MAGENTA << "BIG" << RESET << "  iter: " << GREEN << max[j].second 
              //     << RESET << "  num: " << GREEN << max[j].first << RESET << endl;
              done = true;
            }
          }
        }
        if(done){
          break;
        }
      }
   
      cout << "Size: " << GREEN << max.size() << RESET << endl;
      cout << "Max: ";
      for(i=0; i<max.size(); i++) {
        cout << "(" << GREEN << max[i].first << RESET << "," << MAGENTA << max[i].second << RESET << ")  ";
      }
      cout << endl;

      num_regions_per_phase_2 = region_size_phase_1/region_size_phase_2;
      index = 0;

      //set up phase_2_mmap
      for(i=0; i<max.size(); i++) {
        cout << MAGENTA << "Offset: " << GREEN << max[i].second << RESET << endl;
			  //return ceil(address_uint64_t/region_size_phase_1);
        start_addr = (max[i].second)*region_size_phase_1;
        start_addr = start_addr>>phase_2_mmap_region_zeros;
        cout << MAGENTA << "BEGIN: " << GREEN << start_addr << RESET << "  Hex: " 
          << GREEN << fixed << hex << start_addr << RESET << endl;
        
        //add sub regions to phase_2_mmap from single region in phase 1
        for(k=0; k<num_regions_per_phase_2; k++) {
          boost::dynamic_bitset<> tmp_offset(phase_2_mmap_region_bits);
          print_l_r = 0;
          print_l_c = 0;
          for(j=0; j<phase_2_mmap_region_bits; j++) {
            tmp_offset[phase_2_mmap_region_bits-j-1] = ((start_addr>>j)&1);
            print_l_r |= (tmp_offset[phase_2_mmap_region_bits-j-1]<<j);
          }

          boost::dynamic_bitset<> tmp_cache(phase_2_mmap_cache_bits);
          for(j=0; j<phase_2_mmap_cache_bits; j++) {
            tmp_cache[phase_2_mmap_cache_bits-j-1] = ((index>>j)&1);
            print_l_c |= (tmp_cache[phase_2_mmap_cache_bits-j-1]<<j);
          }
          
          phase_2_mmap.insert(pair<p2_mmu_offset_size, p2_cache_offset_size>(tmp_offset, tmp_cache));

          if(1) cout << "Phase 2 mmap-> " << "Address: " << fixed << hex << print_l_r 
            << ", " << dec << print_l_r << " -- Cache_Index: " << fixed << hex << print_l_c 
            << ", " << dec << print_l_c << endl;
          
          index++;
          start_addr += 1;
        }
      }
    }

    //phase 1
    init();    
  }
};

int main(int argc, char* argv[]) {
	int i;  //for looping
	int opt; 
	int ver = 0;
	int uint64_t_index = 0;
  float inter;
  char* l1;
  char* l2;
  char* l3;
  char* ds;

	static struct option uint64_t_options[] = {
		{      "L1", 	required_argument,  0,  'a' },
		{      "L2", 	required_argument,  0,  'b' },
		{      "L3",  required_argument,  0,  'c' },
    { "dataset",  required_argument,  0,  'd' },
		{"interval", 	required_argument,  0,  'i' },
		{ "verbose", 	      no_argument,  0,  'v' },
		{         0,                  0,  0,   0  }
	};
	
  /*
  //grab the command line arguments
	if(argc != 7){
		printf("Usage: cache_size(Bytes), counter_size(Bits), keep_percentage(100%), interval(s), verbose(0=N/1=Y), dataset_name(.csv)\n");
		exit(0);
	}
  */
      
	// put ':' in the starting of the 
	// string so that program can  
	//distinguish between '?' and ':'  
	while((opt = getopt_long(argc, argv, ":a:b:c:d:i:v", uint64_t_options, &uint64_t_index)) != -1)  
	{  
		switch(opt)  
		{  
			case 'v':
        ver = 1;
        break;
			case 'i':  
				inter = atof(optarg);  
				break;  
			case 'a':  
        l1 = optarg;
				break;  
			case 'b':  
        l2 = optarg;
				break;  
			case 'c':  
        l3 = optarg;
				break;  
			case 'd':  
        ds = optarg;
				break;  
			case ':':  
				printf("option needs a value\n");  
				break;  
			case '?':  
				printf("unknown option: %c\n", optopt); 
				break;  
		}  
	}  
		
	// optind is for the extra arguments 
	// which are not parsed 
	for(; optind < argc; optind++){      
		printf("extra arguments: %s\n", argv[optind]);  
	} 
		
	Global G;
  G.parse(l1, l2, l3);
  G.interval = inter;
  G.verbose = ver;
  string tmp_str(ds);
  G.dataset_name = tmp_str;
	
	//set debugging to off
	G.debug = 0;
  //G.num_bits_addressable = 64;
	 
	//mmap calculations	
	//phase 2
	G.phase_2_mmap_cache_bits = ceil(log2(G.num_cache_regions_phase_2)); 
	G.phase_2_mmap_region_zeros = ceil(log2(G.region_size_phase_2));
	G.phase_2_mmap_region_bits = G.num_bits_addressable-G.phase_2_mmap_region_zeros;
	
	//phase 3
	G.phase_3_mmap_cache_bits = ceil(log2(G.num_cache_regions_phase_3)); 
	G.phase_3_mmap_region_zeros = ceil(log2(G.region_size_phase_3));
	G.phase_3_mmap_region_bits = G.num_bits_addressable-G.phase_3_mmap_region_zeros;
  
  G.first_address_as_uint64_t = 0;

	//initialize the cache
	G.init();	

	//print variables
	if(G.verbose){
    pair<string, string> tmp;
		cout << endl;
		cout << RESET << fixed << "Time between heatmapings: " << GREEN << G.interval 
			 << MAGENTA << " Seconds" << endl;
    cout << endl;

		cout << CYAN << "PHASE 1" << endl;
    tmp = G.get_log2_size(G.total_data_size_phase_1);
		cout << RESET << "Total_data_size_phase_1: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.total_data_size_phase_1 << MAGENTA 
       << " Bytes" << endl;
    tmp = G.get_log2_size(G.cache_size_phase_1);
    cout << RESET << "Size_of_phase_1_cache: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.cache_size_phase_1 << MAGENTA
       << " Bytes" << endl;
    cout << RESET << "Num_regions_in_cache: " << GREEN << G.num_cache_regions_phase_1
       << endl;
    tmp = G.get_log2_size(G.region_size_phase_1);
		cout << RESET << "Region_size_phase_1: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.region_size_phase_1 << MAGENTA 
       << " Bytes" << endl;
    cout << RESET << "Counter_size_phase_1: " << GREEN << G.counter_size_phase_1 
       << MAGENTA << " Bits" <<  endl;
		cout << endl;

		cout << CYAN << "PHASE 2" << endl;
    tmp = G.get_log2_size(G.total_data_size_phase_2);
		cout << RESET << "Total_data_size_phase_2: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.total_data_size_phase_2 << MAGENTA 
       << " Bytes" << endl;
    tmp = G.get_log2_size(G.cache_size_phase_2);
    cout << RESET << "Size_of_phase_2_cache: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.cache_size_phase_2 << MAGENTA
       << " Bytes" << endl;
    cout << RESET << "Num_regions_in_cache: " << GREEN << G.num_cache_regions_phase_2
       << endl;
    tmp = G.get_log2_size(G.region_size_phase_2);
		cout << RESET << "Region_size_phase_2: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.region_size_phase_2 << MAGENTA 
       << " Bytes" << endl;
    cout << RESET << "Counter_size_phase_2: " << GREEN << G.counter_size_phase_2 
       << MAGENTA << " Bits" <<  endl;
		cout << RESET << "Number of bits to address cache in mmap for phase_2: " 
			 << GREEN << G.phase_2_mmap_cache_bits << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of zeros in address region in mmap for phase_2: " 
			 << GREEN << G.phase_2_mmap_region_zeros << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of bits to address region in mmap for phase_2: " 
			 << GREEN << G.phase_2_mmap_region_bits << MAGENTA << " Bits" << endl;
		cout << endl;

		cout << CYAN << "PHASE 3" << endl;
    tmp = G.get_log2_size(G.total_data_size_phase_3);
		cout << RESET << "Total_data_size_phase_3: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.total_data_size_phase_3 << MAGENTA 
       << " Bytes" << endl;
    tmp = G.get_log2_size(G.cache_size_phase_3);
    cout << RESET << "Size_of_phase_3_cache: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.cache_size_phase_3 << MAGENTA
       << " Bytes" << endl;
    cout << RESET << "Num_regions_in_cache: " << GREEN << G.num_cache_regions_phase_3
       << endl;
    tmp = G.get_log2_size(G.region_size_phase_3);
		cout << RESET << "Region_size_phase_3: " << GREEN << tmp.first << MAGENTA 
       << tmp.second << "   " << GREEN << G.region_size_phase_3 << MAGENTA << " Bytes" 
       <<  endl;
    cout << RESET << "Counter_size_phase_3: " << GREEN << G.counter_size_phase_3 
       << MAGENTA << " Bits" <<  endl;
		cout << RESET << "Number of bits to address cache in mmap for phase_3: " 
			 << GREEN << G.phase_3_mmap_cache_bits << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of zeros in address region in mmap for phase_3: " 
			 << GREEN << G.phase_3_mmap_region_zeros << MAGENTA << " Bits" << endl;
		cout << RESET << "Number of bits to address region in mmap for phase_3: " 
			 << GREEN << G.phase_3_mmap_region_bits << MAGENTA << " Bits" << endl;
		cout << RESET << endl;
	}


	//read dataset in from dataset file and run 
  string line;
  uint64_t addr;
  uint64_t index;
  double time;
  string phys_addr;
  stringstream iss;
  string token;
  int where;
  double pause_time = 0;
  bool first_time = true;
  uint64_t iteration = 0;

	if(G.verbose) cout << endl << endl << GREEN << "Start Run" 
	  << RESET << endl;
  
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
          
          if(first_time){
            first_time = false;
            pause_time = time + G.interval;
          }else if(pause_time < time){
            pause_time = time + G.interval;
            cout << CYAN << "##########  Iteration: " << GREEN << iteration << RESET
              << "   Timestamp: " << GREEN << time << RESET << endl << endl;
            iteration++;

            //do a heatmaping of the current caches and cascade
            G.heatmap(iteration);
            
            //quit early
            if(iteration == 3) exit(0);
          }

          //change counters for this access
          if(1) cout << RESET << "Timestamp: " << GREEN << time << MAGENTA << " seconds" << RESET << endl;
          addr = G.string_to_uint64_t(phys_addr);
          if(1) cout << RESET << "Address-- Hex:" << GREEN << fixed << hex << addr << RESET << "  uint64_t: " 
            << GREEN << dec << addr << RESET << endl;
          
          //add to phase_1_cache counter
          index = G.find_offset(1, addr);
          G.change_counter(1, index);

          //add to phase_2_cache counter
          if(iteration >= 1) {
            index = G.find_offset(2, addr);
            cout << "Index: " << GREEN << index << RESET << endl;
            if(index != (uint64_t)-1) {
              G.change_counter(2, index);
            }
            cout << endl;
          }

          //add to phase_3_cache counter
          if(iteration >= 2) {
            index = G.find_offset(3, addr);
            cout << "Index 3: " << GREEN << index << RESET << endl;
            if(index > -1) {
              G.change_counter(3, index);
            }
          }

          if(G.debug) cout << RESET << endl;
          iss.clear();
      }
  }
	myfile.close();	
	exit(0);
}
