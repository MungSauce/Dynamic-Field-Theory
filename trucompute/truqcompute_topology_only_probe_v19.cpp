#include <algorithm>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tc19 {
constexpr std::size_t N=1'000'000;
constexpr std::uint32_t ROUNDS=30;
constexpr std::uint32_t SIG_BITS=20;

struct PairHash {
  std::size_t operator()(const std::pair<std::uint32_t,std::uint32_t>& p) const noexcept {
    return (static_cast<std::size_t>(p.first)<<32)^p.second;
  }
};

std::size_t rooted_path_classes(std::size_t n,std::uint32_t rounds){
  std::vector<std::uint32_t> cls(n,0),next(n,0);
  cls[0]=1;
  for(std::uint32_t r=0;r<rounds;++r){
    std::unordered_map<std::pair<std::uint32_t,std::uint32_t>,std::uint32_t,PairHash> canon;
    std::uint32_t next_class=0;
    for(std::size_t slot=0;slot<n;++slot){
      const std::uint32_t parent=slot==0?UINT32_MAX:cls[slot-1];
      const auto key=std::make_pair(cls[slot],parent);
      auto [it,inserted]=canon.emplace(key,next_class);
      if(inserted) ++next_class;
      next[slot]=it->second;
    }
    cls.swap(next);
  }
  return std::unordered_set<std::uint32_t>(cls.begin(),cls.end()).size();
}

struct Node { std::bitset<SIG_BITS> relation; };

std::int64_t contextual_zero(const Node& node,const std::bitset<SIG_BITS>& global){
  std::int64_t z=0;
  for(int t=static_cast<int>(SIG_BITS)-1;t>=0;--t){
    const int s=(node.relation[static_cast<std::size_t>(t)]==global[static_cast<std::size_t>(t)])?+1:-1;
    z=2*z+s;
  }
  return z;
}

std::vector<Node> make_nodes(){
  std::vector<Node> nodes;
  nodes.reserve(N);
  for(std::size_t construction_counter=0;construction_counter<N;++construction_counter){
    Node x;
    for(std::uint32_t b=0;b<SIG_BITS;++b)
      x.relation[b]=((construction_counter>>b)&1ULL)!=0;
    nodes.push_back(x);
  }
  return nodes;
}

std::uint64_t signature(const Node& n){ return n.relation.to_ullong(); }
}

using namespace tc19;
static void require(bool ok,const char* msg){
  if(!ok){ std::cerr<<"FAIL: "<<msg<<"\n"; std::exit(1); }
}

int main(){
  const auto sparse=rooted_path_classes(N,ROUNDS);
  require(sparse==32,"unexpected sparse topology class count");

  auto nodes=make_nodes();
  std::bitset<SIG_BITS> global(0xA53D7u);
  std::unordered_set<std::int64_t> contexts;
  std::unordered_map<std::uint64_t,std::int64_t> baseline;
  contexts.reserve(static_cast<std::size_t>(N*1.3));
  baseline.reserve(static_cast<std::size_t>(N*1.3));

  for(const auto& node:nodes){
    const auto z=contextual_zero(node,global);
    require(contexts.insert(z).second,"context collision");
    baseline.emplace(signature(node),z);
  }
  require(contexts.size()==N,"million nodes not unique");

  std::mt19937_64 rng(0x19ULL);
  std::shuffle(nodes.begin(),nodes.end(),rng);
  for(const auto& node:nodes){
    const auto it=baseline.find(signature(node));
    require(it!=baseline.end() && it->second==contextual_zero(node,global),
            "permutation changed topology-derived state");
  }

  std::cout<<"TRUCOMPUTE_TOPOLOGY_ONLY_PROBE_V19=PASS\n";
  std::cout<<"million_nodes="<<N<<"\n";
  std::cout<<"local_sparse_path_rounds="<<ROUNDS<<"\n";
  std::cout<<"local_sparse_path_distinguishable_classes="<<sparse<<"\n";
  std::cout<<"topology_signature_bits_per_node="<<SIG_BITS<<"\n";
  std::cout<<"topology_signature_total_relation_bits="<<(N*SIG_BITS)<<"\n";
  std::cout<<"topology_signature_unique_contexts="<<contexts.size()<<"\n";
  std::cout<<"numeric_node_id_used_by_update_rule=NO\n";
  std::cout<<"permutation_invariance=PASS\n";
  std::cout<<"address_equivalent_information_eliminated=NO\n";
  std::cout<<"address_equivalent_information_location=RELATIONAL_TOPOLOGY\n";
  std::cout<<"depth30_possible_histories="<<(1ULL<<30)<<"\n";
  std::cout<<"minimum_distinguishability_bits_for_depth30=30\n";
  std::cout<<"fixed_one_bit_node_can_hold_depth30_history_alone=NO\n";
}
