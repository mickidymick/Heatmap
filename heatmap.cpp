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

    //for calculating correctness per interval
    vector<uint64_t> cache_hits; //number of cache hits
    vector<uint64_t> cache_misses; //number of cache misses
    vector<uint64_t> counter_inc; //number of time a counter in the cache was incremented
    vector<uint64_t> counter_dec; //number of time a counter in the cache was not incremented
    
    //for calculating correctness overall
    vector<uint64_t> total_cache_hits; //number of cache hits
    vector<uint64_t> total_cache_misses; //number of cache misses
    vector<uint64_t> total_counter_inc; //number of time a counter in the cache was incremented
    vector<uint64_t> total_counter_dec; //number of time a counter in the cache was not incremented

    //datastructures for cache
    vector<int> num_region_bits; //number of bits needed to address all addresses in region
    vector<int> counter_size; //size of the number of bits in for the counter in the cache
    vector<uint64_t> cache_size; //size of the cache in bits
    vector<uint64_t> total_data_size; //size of the total amount of data
    vector<uint64_t> region_size; //size of each region
    vector<uint64_t> num_cache_regions; //number of regions
    vector<vector<uint64_t>> cache; //the cache for phase 1, 2, 3

    //datastructures for memory map
    vector<int> mmap_cache_bits; //number of bits needed in the mmap to offset into the cache
    vector<int> mmap_region_bits; //number of bits needed in the mmap to figure out which region this beuint64_ts to
    vector<int> mmap_region_zeros; //number of bits needed in the mmap to pad the address with zeros
    vector<map<uint64_t, uint64_t>> mmap; //the memory map for phase 2, 3
    map<uint64_t, uint64_t>::iterator mmap_itter; //iterator for the mmap

    //##### helper functions #####

    /* get_log2_size: 
     * Parameters: uint64_t the number to figure out the size of
     * Returns: pair<string, string> the number reduced by the type, the type
     */
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

    /* string_to_uint64_t: Converts an address as a string to a uint64_t
     * Parameters: string the address to convert
     * Returns: uint64_t the address as a number
     */
    uint64_t string_to_uint64_t(string address) {
      stringstream ss;
      uint64_t address_uint64_t;

      ss << hex << address;
      ss >> address_uint64_t;
      return address_uint64_t;
    }

    /* increment: increment the counter by one if it is not maxed
     * Parameters: int the phase you are on 
     *             uint64_t the index in the cache to increment
     * Returns: int 1 if incremented 0 if full
     */
    int increment(int phase, uint64_t offset) {
      uint64_t value = cache[phase][offset];

      if(value < (pow(2, counter_size[phase])-1)){
        value++;
        cache[phase][offset] = value;
        if(debug)cout << "Counter: " << GREEN << cache[phase][offset] << RESET << endl;
        return 1;
      }else{
        return 0;
      }
    }

    /* find_offset: find the offset at the specified address for the phase
     * Parameters: int the phase you are on
     *             uint64_t the address of the index we need to find
     * Returns: uint64_t the index
     */
    uint64_t find_offset(int phase, uint64_t address_uint64_t) {
      uint64_t offset = 0;

      if(phase == 0) {
        //return address_uint64_t >> num_region_bits_phase_1;
        return ceil(address_uint64_t/region_size[phase]);
      }else{
        //convert address to bitset for lookup in mmap
        if(debug) cout << "Address uint64_t: " << address_uint64_t << "  Address Hex: " 
          << hex << address_uint64_t << dec << endl;
        address_uint64_t = address_uint64_t >> mmap_region_zeros[phase];

        //find in mmap and convert from bitset to uint64_t
        mmap_itter = mmap[phase].find(address_uint64_t);
        if(mmap_itter == mmap[phase].end()) {
          if(debug) cout << RED << "Address not found in mmap" << RESET << endl;
          return -1;
        }else{
          offset = mmap_itter->second;
          if(debug) cout << GREEN << "Address found--  Index" << GREEN << offset << RESET << endl;
          return offset;
        }
      }
      return -1;
    }

    //##### main functions #####

    /* init: initilize the maps and vectors
     * Parameters: None
     * Returns: None
     */
    void init() {
      //for calculating correctness per interval
      cache_hits.resize(3);
      cache_misses.resize(3);
      counter_inc.resize(3);
      counter_dec.resize(3);
      
      //for calculating total correctness
      total_cache_hits.resize(3);
      total_cache_misses.resize(3);
      total_counter_inc.resize(3);
      total_counter_dec.resize(3);

      //datastructures for cache
      num_region_bits.resize(3);
      counter_size.resize(3);
      cache_size.resize(3);
      total_data_size.resize(3);
      region_size.resize(3);
      num_cache_regions.resize(3);
      cache.resize(3);

      //datastructures for memory map
      mmap_cache_bits.resize(3);
      mmap_region_bits.resize(3);
      mmap_region_zeros.resize(3);
      mmap.resize(3);
    }

    /* parse: parse the L1, L2, L3 args
     * Parameters: char* layer1
     *             char* layer2
     *             char* layer3
     * Returns: None
     */
    void parse(char* l1, char* l2, char* l3) {
      int i;
      string tmp_str;
      string l1_str(l1);
      string l2_str(l2);
      string l3_str(l3);
      stringstream l1_s_str(l1_str);
      stringstream l2_s_str(l2_str);
      stringstream l3_s_str(l3_str);
      vector<stringstream *> l;

      l.push_back(&l1_s_str);
      l.push_back(&l2_s_str);
      l.push_back(&l3_s_str);

      for(i=0; i<3; i++) {
        getline((*(l[i])), tmp_str, ',');
        if(i==0) num_bits_addressable = stol(tmp_str);
        num_region_bits[i] = stol(tmp_str);
        total_data_size[i] = pow(2,stol(tmp_str));
        getline((*(l[i])), tmp_str, ',');
        region_size[i] = pow(2, stol(tmp_str));
        getline((*(l[i])), tmp_str, ',');
        counter_size[i] = stol(tmp_str);
        num_cache_regions[i] = total_data_size[i]/region_size[i];
        cache_size[i] = num_cache_regions[i]*((float)counter_size[i]/8);
      }
    }

    /* change_counter increment the counter in the cache of the desired phase
     * Parameters: int the phase to be incremented
     *             uint64_t the index into the cache
     * Returns: bool if counter was incremented ret 1 if full ret 0
     */
    bool change_counter(int phase, uint64_t offset) {
      uint64_t counter;

      if(increment(phase, offset)) {
        counter = cache[phase][offset];
        if(debug) cout << "Phase " << phase << "  Offset: " << hex << offset 
          << "  Counter: " << dec << counter << endl;
        return true;
      }else{
        counter = cache[phase][offset];
        if(debug) cout << "Phase " << phase << "  Offset: " << hex << offset 
          << "  Counter: " << dec << counter << RED <<"  --FULL--" 
            << RESET << endl;
        return false;
      }
      return false;
    }

    /* heatmap: runs through and moves counters to next phase
     * Parameters: uint64_t what iteration we are on
     * Returns: None
     */
    void heatmap(uint64_t iteration) {
      int p, i, j, k; //for looping
      bool done = false;
      typedef pair<uint64_t, uint64_t> pairs; //to add to sets (the number, the index)
      vector<pairs> max; //maximum number found
      uint64_t index = 0;
      uint64_t region = 0;
      uint64_t num; //number to check size of 
      uint64_t num_regions_needed; //number of regions needed to fill the next phase
      uint64_t num_regions_per; //number of subregions per over region
      uint64_t start_addr; //starting address for region

      for(p=2; p>-1; p--) {
        //only run phase1 on first interval
        if(iteration==0) {
          p=0;
          //only run phase 1, 2 on second interval
        }else if(iteration==1 && p==2) {
          p=1;
        }

        //clear mmap and cache
        cache[p].clear();
        cache[p].resize(num_cache_regions[p]);
        if(p>0) mmap[p].clear();
        max.clear();
        done = false;

        if(p==0) {
          break;
        }

        //calculate number of regions to accuire
        num_regions_needed = total_data_size[p]/region_size[p-1];

        //set iterator to begining
        if(p>1) mmap_itter = (mmap[p-1]).begin();

        //find hot regions in the above phase
        for(i=0; i<num_cache_regions[p-1]; i++) {
          if(p==1) {
            region = (region_size[p-1])*i;
            index = i;
          }else if(p>1) {
            mmap_itter++;
            region = mmap_itter->first; 
            region = region << mmap_region_zeros[p-1];
            index = mmap_itter->second; 
          }
          num = cache[p-1][index];

          if(debug) cout << "iter: " << GREEN << i << RESET << "  addr: " << GREEN << region << RESET <<"  index: " 
            << GREEN << index << RESET << "  num: " << GREEN << num << RESET << endl;

          if(max.size() < num_regions_needed) {
            max.push_back(make_pair(num, region));
          }else{
            for(j=0; j<max.size(); j++) {
              if(max[j].first < num) {
                if(debug) cout << MAGENTA << "OLD" << RESET << "  index: " << GREEN << max[j].second 
                  << RESET << "  num: " << GREEN << max[j].first << endl;
                if(debug) cout << MAGENTA << "NEW" << RESET << "  index: " << GREEN << index << RESET 
                  << "  num: " << GREEN << num << RESET << endl;
                max[j] = make_pair(num, region);
                break;
              }
            }
          }
          if(max.size() == num_regions_needed) {
            for(j=0; j<max.size(); j++) {
              if(max[j].first != (pow(2, counter_size[p])-1)){
                if(debug) cout << "reg_bits: " << counter_size[p] << endl;
                if(debug) cout << "max: " << (pow(2, counter_size[p])-1) << endl;
                if(debug) cout << MAGENTA << "SMALL" << RESET << "  index: " << GREEN << max[j].second 
                  << RESET << "  num: " << GREEN << max[j].first << RESET << endl;
                if(debug) cout << endl;
                break;  
              }else if(j == max.size()-1){
                if(debug) cout << MAGENTA << "BIG" << RESET << "  index: " << GREEN << max[j].second 
                  << RESET << "  num: " << GREEN << max[j].first << RESET << endl;
                done = true;
                if(debug) cout << endl;
              }
            }
          }
          if(done){
            break;
          }
        }
        if(debug) {
          cout << "Size: " << GREEN << max.size() << RESET << endl;
          cout << "Max: ";
          for(i=0; i<max.size(); i++) {
            cout << "(" << GREEN << max[i].first << RESET << "," << MAGENTA << fixed << hex << max[i].second << RESET << ")  ";
          }
          cout << endl;
        }

        num_regions_per = region_size[p-1]/region_size[p];
        index = 0;

        //set up mmap
        for(i=0; i<max.size(); i++) {
          if(debug) cout << MAGENTA << "START: " << GREEN << fixed << hex << max[i].second << RESET << endl;
          start_addr = max[i].second;
          start_addr = start_addr>>mmap_region_zeros[p];
          if(debug) cout << MAGENTA << "BEGIN: " << GREEN << start_addr << RESET << "  Hex: " 
            << GREEN << fixed << hex << start_addr << RESET << endl;

          //add sub regions to phase_3_mmap from single region in phase 1
          for(k=0; k<num_regions_per; k++) {
            mmap[p].insert(pair<uint64_t, uint64_t>(start_addr, index));

            if(debug) cout << "Phase " << p << " mmap-> " << "Address: " << fixed << hex << start_addr
              << ", " << dec << start_addr<<mmap_region_zeros[p] << " -- Cache_Index: " << fixed << hex << index
                << ", " << dec << index << endl;

            index++;
            start_addr += 1;
          }
        }
      }
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
  int p_interval = 0;

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

  //initialize the cache
  Global G;
  G.init();	

  G.parse(l1, l2, l3);
  G.interval = inter;
  G.verbose = ver;
  string tmp_str(ds);
  G.dataset_name = tmp_str;
  p_interval = G.verbose;

  //set debugging to off
  G.debug = 0;

  //mmap calculations
  for(i=1; i<3; i++) {
    G.mmap_cache_bits[i] = ceil(log2(G.num_cache_regions[i])); 
    G.mmap_region_zeros[i] = ceil(log2(G.region_size[i]));
    G.mmap_region_bits[i] = G.num_bits_addressable-G.mmap_region_zeros[i];
  }

  //finish cache set up
  G.cache[0].resize(G.num_cache_regions[0]);
  G.cache[1].resize(G.num_cache_regions[1]);
  G.cache[2].resize(G.num_cache_regions[2]);

  //set begining of address range
  G.first_address_as_uint64_t = 0;

  //print variables
  if(G.verbose){
    pair<string, string> tmp;
    cout << endl;
    cout << RESET << fixed << "Time between heatmapings: " << GREEN << G.interval 
      << MAGENTA << " Seconds" << endl;
    cout << endl;

    for(i=0; i<3; i++) {
      cout << CYAN << "PHASE " << i+1 << endl;
      tmp = G.get_log2_size(G.total_data_size[i]);
      cout << RESET << "Total_data_size: " << GREEN << tmp.first << MAGENTA 
        << tmp.second << "   " << GREEN << G.total_data_size[i] << MAGENTA 
        << " Bytes" << endl;
      tmp = G.get_log2_size(G.cache_size[i]);
      cout << RESET << "Size_of_cache: " << GREEN << tmp.first << MAGENTA 
        << tmp.second << "   " << GREEN << G.cache_size[i] << MAGENTA
        << " Bytes" << endl;
      cout << RESET << "Num_regions_in_cache: " << GREEN << G.num_cache_regions[i]
        << endl;
      tmp = G.get_log2_size(G.region_size[i]);
      cout << RESET << "Region_size: " << GREEN << tmp.first << MAGENTA 
        << tmp.second << "   " << GREEN << G.region_size[i] << MAGENTA 
        << " Bytes" << endl;
      cout << RESET << "Counter_size: " << GREEN << G.counter_size[i] 
        << MAGENTA << " Bits" <<  endl;
      if(i>0) {
        cout << RESET << "Number of bits to address cache in mmap: " 
          << GREEN << G.mmap_cache_bits[i] << MAGENTA << " Bits" << endl;
        cout << RESET << "Number of zeros in address region in mmap: " 
          << GREEN << G.mmap_region_zeros[i] << MAGENTA << " Bits" << endl;
        cout << RESET << "Number of bits to address region in mmap: " 
          << GREEN << G.mmap_region_bits[i] << MAGENTA << " Bits" << endl;
      }
      cout << endl;
    }
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
  vector<float> percentage;
  const string sep = " |";
  const string sep_line = sep + string(100, '-') + '|';

  percentage.resize(4);

  //if(G.verbose) cout << endl << endl << GREEN << "Start Run" 
  //  << RESET << endl;
  if(p_interval) cout << RESET << sep_line << '\n' << sep
                               << setw(8) << "Interval" << sep
                               << setw(5) << "Phase" << sep
                               << setw(10) << "Cache_Hit" << sep
                               << setw(7) << "%" << sep
                               << setw(10) << "Cache_Mis" << sep
                               << setw(7) << "%" << sep
                               << setw(10) << "Count_Inc" <<sep
                               << setw(7) << "%" << sep
                               << setw(10) << "Cnt_Full" << sep
                               << setw(7) << "%" << sep
                               << '\n' << sep_line << '\n';

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
        //if(p_interval) cout << CYAN << "##########  Iteration: " << GREEN << iteration << RESET
        //  << "   Timestamp: " << GREEN << time << RESET << endl;
        for(i=0; i<3; i++) {
          //only print phase_1
          if(iteration==0 && i==1) break;

          //only print phase 1, 2
          if(iteration==1 && i==2) break;

          if(p_interval) {
            percentage[0] = ((float)G.cache_hits[i]/(G.cache_hits[i]+G.cache_misses[i]))*100;
            percentage[1] = ((float)G.cache_misses[i]/(G.cache_hits[i]+G.cache_misses[i]))*100;
            percentage[2] = ((float)G.counter_inc[i]/(G.counter_inc[i]+G.counter_dec[i]))*100;
            percentage[3] = ((float)G.counter_dec[i]/(G.counter_inc[i]+G.counter_dec[i]))*100;

            cout << RESET << sep;

            if(iteration<2) {
              if(i==0) {
                cout << setw(8) << iteration << sep;
              }else{
                cout << setw(8) << " " << sep;
              }
            }else {
              if(i==1) {
                cout << setw(8) << iteration << sep;
              }else{
                cout << setw(8) << " " << sep;
              }
            }
            
            cout << setw(5) << i << sep
                 << setw(10) << G.cache_hits[i] << sep;
            
            if(percentage[0]>50){
              cout << GREEN << setprecision(2) << setw(7) <<  percentage[0] << sep;
            }else{
              cout << RED << setprecision(2) << setw(7) << percentage[0] << sep;
            }
           
            cout << RESET << setw(10) << G.cache_misses[i] << sep;
            
            if(percentage[1]>50){
              cout << GREEN << setprecision(2) << setw(7) <<  percentage[1] << sep;
            }else{
              cout << RED << setprecision(2) << setw(7) << percentage[1] << sep;
            }     
              
            cout << RESET << setw(10) << G.counter_inc[i] <<sep;
            
            if(percentage[2]>50){
              cout << GREEN << setprecision(2) << setw(7) <<  percentage[2] << sep;
            }else{
              cout << RED << setprecision(2) << setw(7) << percentage[2] << sep;
            }
                 
            cout << RESET << setw(10) << G.counter_dec[i] << sep;
            
            if(percentage[3]>50){
              cout << GREEN << setprecision(2) << setw(7) <<  percentage[3] << sep;
            }else{
              cout << RED << setprecision(2) << setw(7) << percentage[3] << sep;
            }
            cout << RESET << '\n';
          }
          G.total_cache_hits[i] += G.cache_hits[i];
          G.total_cache_misses[i] += G.cache_misses[i];
          G.total_counter_inc[i] += G.counter_inc[i];
          G.total_counter_dec[i] += G.counter_dec[i];
        }
        if(p_interval) cout << sep_line << '\n';
        iteration++;

        //clear counters
        for(i=0; i<3; i++) {
          G.cache_hits[i] = 0;
          G.cache_misses[i] = 0;
          G.counter_inc[i] = 0;
          G.counter_dec[i] = 0;
        }

        //quit early
        if(G.debug) {
          if(iteration == 3) {
            exit(0);
          }
        }

        //do a heatmaping of the current caches and cascade
        G.heatmap(iteration);

      }

      //change counters for this access
      if(G.debug) cout << RESET << "Timestamp: " << GREEN << time << MAGENTA << " seconds" << RESET << endl;
      addr = G.string_to_uint64_t(phys_addr);
      if(G.debug) cout << RESET << "Address-- Hex:" << GREEN << fixed << hex << addr << RESET << "  uint64_t: " 
        << GREEN << dec << addr << RESET << endl;

      //add to phase_cache counter
      for(i=2; i>-1; i--) {
        //only run phase1 on first interval
        if(iteration==0) {
          i=0;
          //only run phase 1, 2 on second interval
        }else if(iteration==1 && i==2) {
          i=1;
        }

        if(G.debug) cout << MAGENTA << "Phase_" << i << " ->" << RESET << endl;
        index = G.find_offset(i, addr);
        if(index != (uint64_t)-1) {
          G.cache_hits[i]++;
          if(G.change_counter(i, index)){
            G.counter_inc[i]++;
          }else{
            G.counter_dec[i]++;
          }
        }else{
          G.cache_misses[i]++;
        }
      }
      if(G.debug) cout << endl;
      if(G.debug) cout << RESET << endl;
      iss.clear();
    }
  }
  myfile.close();	

  cout << endl;
  cout << CYAN << "Total Stats:" << RESET << endl;
  for(int i=0; i<3; i++) {
    cout << "Phase " << i << endl;
    cout << "Total_cache_hits: " << GREEN << G.total_cache_hits[i] << RESET << "  Percentage: " 
      << MAGENTA << ((float)G.total_cache_hits[i]/(G.total_cache_hits[i]+G.total_cache_misses[i]))*100 
      << "%" << RESET << endl;
    cout << "Total_cache_misses: " << GREEN << G.total_cache_misses[i] << RESET << "  Percentage: " 
      << MAGENTA << ((float)G.total_cache_misses[i]/(G.total_cache_hits[i]+G.total_cache_misses[i]))*100 
      << "%" << RESET << endl;
    cout << "Total_counter_inc: " << GREEN << G.total_counter_inc[i] << RESET << "  Percentage: " 
      << MAGENTA << ((float)G.total_counter_inc[i]/(G.total_counter_inc[i]+G.total_counter_dec[i]))*100 
      << "%" << RESET << endl;
    cout << "Total_counter_not_inc: " << GREEN << G.total_counter_dec[i] << RESET << "  Percentage: " 
      << MAGENTA << ((float)G.total_counter_dec[i]/(G.total_counter_inc[i]+G.total_counter_dec[i]))*100 
      << "%" << RESET << endl;
  }

  exit(0);
}
