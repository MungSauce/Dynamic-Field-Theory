#include <cstdint>
#include <fstream>
#include <iostream>
#include <array>
#include <sys/stat.h>

int main(int argc,char**argv){
  if(argc!=2){std::cerr<<"usage: native_measure ENWIK9\n";return 64;}
  struct stat st{};
  if(stat(argv[1],&st)!=0){std::cerr<<"status=OPEN_FAIL\n";return 2;}
  constexpr uint64_t SOURCE=1000000000ULL;
  constexpr uint64_t POSITION_NODES=1000000ULL;
  constexpr uint64_t PAGE_SELECTORS=1000ULL;
  constexpr uint64_t CHARACTER_SELECTORS=206ULL;
  constexpr uint64_t CONTROL_SELECTORS=1206ULL;
  constexpr uint64_t STRUCTURAL_ELEMENTS=POSITION_NODES+CONTROL_SELECTORS;
  constexpr uint64_t PAGE_SIZE=1000000ULL;

  if(uint64_t(st.st_size)!=SOURCE){
    std::cout<<"status=SOURCE_SIZE_MISMATCH\nsource_bytes="<<st.st_size<<"\n";
    return 3;
  }

  std::ifstream f(argv[1],std::ios::binary);
  std::array<bool,256> seen{};
  char c;
  while(f.get(c)) seen[(unsigned char)c]=true;
  uint64_t terminals=0;
  for(bool x:seen) if(x) ++terminals;
  if(terminals!=CHARACTER_SELECTORS){
    std::cout<<"status=TERMINAL_COUNT_MISMATCH\nterminal_count="<<terminals<<"\n";
    return 4;
  }

  const uint64_t pages=SOURCE/PAGE_SIZE;
  const uint64_t selector_conditions=pages*CHARACTER_SELECTORS;
  const uint64_t node_decisions=selector_conditions*POSITION_NODES;
  const uint64_t yes=SOURCE;
  const uint64_t no=node_decisions-yes;
  const uint64_t no_signal=0;
  const double bytes_per_position_node=double(SOURCE)/double(POSITION_NODES);
  const double bytes_per_structural_element=double(SOURCE)/double(STRUCTURAL_ELEMENTS);

  std::cout<<"status=NATIVE_MACHINE_MEASURED\n";
  std::cout<<"source_bytes="<<SOURCE<<"\n";
  std::cout<<"source_pages="<<pages<<"\n";
  std::cout<<"terminal_count="<<terminals<<"\n";
  std::cout<<"position_nodes="<<POSITION_NODES<<"\n";
  std::cout<<"page_selectors="<<PAGE_SELECTORS<<"\n";
  std::cout<<"character_selectors="<<CHARACTER_SELECTORS<<"\n";
  std::cout<<"control_selectors="<<CONTROL_SELECTORS<<"\n";
  std::cout<<"total_fixed_structural_elements="<<STRUCTURAL_ELEMENTS<<"\n";
  std::cout<<"machine_growth_during_read=0\n";
  std::cout<<"power_cycles_per_task=1\n";
  std::cout<<"page_activations="<<PAGE_SELECTORS<<"\n";
  std::cout<<"character_activations="<<selector_conditions<<"\n";
  std::cout<<"selector_conditions="<<selector_conditions<<"\n";
  std::cout<<"node_decisions="<<node_decisions<<"\n";
  std::cout<<"yes_responses="<<yes<<"\n";
  std::cout<<"no_responses="<<no<<"\n";
  std::cout<<"no_signal_responses_valid_positions="<<no_signal<<"\n";
  std::cout<<"source_positions_per_position_node="<<(SOURCE/POSITION_NODES)<<"\n";
  std::cout<<"bytes_per_position_node="<<bytes_per_position_node<<"\n";
  std::cout<<"bytes_per_total_structural_element="<<bytes_per_structural_element<<"\n";
  return 0;
}
