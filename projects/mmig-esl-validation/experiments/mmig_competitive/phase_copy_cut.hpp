#pragma once
#include "phase_copies.hpp"
#include <queue>

namespace esl_phase_copy_cut
{
using network = esl_phase_copies::network;

// Integral directed minimum cut. x[n]=1 places n on the source side.
// Copying gate n costs sum_{uncomplemented s->n}(1-x[s]) and removes
// every original complemented use of n. Thus, for fixed original phases,
// I(x)=I(0)-sum_n neg_fanout[n]*x[n]+sum_{positive s->n} x[n]*(1-x[s]).
// Uncopyable PI/constant sources have x=0. Each pair term is one arc n->s.
// The cut exactly minimizes I(x)+lambda*sum(x). Cardinality is bounded by
// a deterministic multiplier search; we do not claim an exact budgeted optimum.
struct flow
{
  struct arc { uint32_t to, reverse; int64_t residual; };
  std::vector<std::vector<arc>> edges;
  std::vector<int> level;
  std::vector<size_t> current;
  explicit flow( size_t n ) : edges( n ), level( n ), current( n ) {}
  void add( uint32_t from, uint32_t to, int64_t capacity )
  {
    arc a{to, static_cast<uint32_t>(edges[to].size()), capacity};
    arc b{from, static_cast<uint32_t>(edges[from].size()), 0};
    edges[from].push_back(a); edges[to].push_back(b);
  }
  bool layers( uint32_t source, uint32_t sink )
  {
    std::fill(level.begin(),level.end(),-1);
    std::queue<uint32_t> q; q.push(source); level[source]=0;
    while(!q.empty())
    {
      auto n=q.front();q.pop();
      for(auto const& e:edges[n]) if(e.residual>0 && level[e.to]<0)
      {level[e.to]=level[n]+1;q.push(e.to);}
    }
    return level[sink]>=0;
  }
  int64_t augment( uint32_t n, uint32_t sink, int64_t amount )
  {
    if(n==sink)return amount;
    for(auto& i=current[n];i<edges[n].size();++i)
    {
      auto& e=edges[n][i];
      if(e.residual<=0 || level[e.to]!=level[n]+1)continue;
      auto pushed=augment(e.to,sink,std::min(amount,e.residual));
      if(pushed){e.residual-=pushed;edges[e.to][e.reverse].residual+=pushed;return pushed;}
    }
    return 0;
  }
  std::vector<uint8_t> cut( uint32_t source, uint32_t sink )
  {
    while(layers(source,sink))
    {
      std::fill(current.begin(),current.end(),0);
      while(augment(source,sink,std::numeric_limits<int64_t>::max()/4)){}
    }
    std::vector<uint8_t> selected(edges.size(),0);
    std::queue<uint32_t> q;q.push(source);selected[source]=1;
    while(!q.empty())
    {
      auto n=q.front();q.pop();
      for(auto const& e:edges[n])if(e.residual>0 && !selected[e.to])
      {selected[e.to]=1;q.push(e.to);}
    }
    return selected;
  }
};

inline std::vector<uint8_t> select( network const& input, uint32_t lambda_milli )
{
  uint32_t const source=input.size(), sink=source+1;
  flow solver(input.size()+2);
  std::vector<int64_t> unary(input.size(),lambda_milli);
  input.foreach_gate([&](auto n){
    if(input.is_min(n))throw std::runtime_error("cut planner requires pure MAJ");
    input.foreach_fanin(n,[&](auto f){
      auto s=input.get_node(f);
      bool gate=!input.is_ci(s)&&!input.is_constant(s);
      if(input.is_complemented(f)){if(gate)unary[s]-=1000;}
      else if(gate)solver.add(n,s,1000);
      else unary[n]+=1000;
    });
  });
  input.foreach_po([&](auto f){
    auto s=input.get_node(f);
    if(input.is_complemented(f)&&!input.is_ci(s)&&!input.is_constant(s))unary[s]-=1000;
  });
  input.foreach_gate([&](auto n){
    if(unary[n]<0)solver.add(source,n,-unary[n]);
    else if(unary[n]>0)solver.add(n,sink,unary[n]);
  });
  return solver.cut(source,sink);
}

inline esl_phase_copies::result materialize( network const& input,
                                            std::vector<uint8_t> const& selected,
                                            esl_phase_copies::stats& st )
{
  auto work=input.clone();
  uint32_t const none=std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> partner(input.size(),none);
  st.before_gates=input.num_gates();st.before_raw=esl_phase_copies::charged_raw(input);
  input.foreach_gate([&](auto n){
    if(!selected[n])return;
    auto copy=input._storage->nodes[n];
    copy.data[0].h1=copy.data[0].h2=copy.data[1].h1=0;copy.data[1].h2=2u;
    auto id=work.size();work._storage->nodes.push_back(copy);
    partner[n]=id;partner.push_back(n);++st.copies;
  });
  work.foreach_gate([&](auto n){
    for(auto& c:work._storage->nodes[n].children)
      if(bool(c.weight ^ work.is_min(n)) && partner[c.index]!=none)
      {c.index=partner[c.index];c.weight^=1;}
  });
  for(auto& o:work._storage->outputs)
    if(o.weight && partner[o.index]!=none){o.index=partner[o.index];o.weight^=1;}
  auto mixed=esl_phase_copies::compact(work);
  auto lowered=mixed.clone();
  lowered.foreach_gate([&](auto n){
    if(lowered.is_min(n))
    {for(auto& c:lowered._storage->nodes[n].children)c.weight^=1;lowered._storage->nodes[n].data[1].h2&=~2u;}
  });
  lowered=esl_phase_copies::compact(lowered);
  st.after_gates=lowered.num_gates();st.after_raw=esl_phase_copies::charged_raw(lowered);
  return {std::move(mixed),std::move(lowered)};
}

inline esl_phase_copies::result optimize( network const& input,
                                         esl_phase_copies::stats& st,
                                         uint32_t max_copies,
                                         uint32_t& lambda_milli )
{
  auto base=esl_phase_copies::compact(input);
  auto count=[&](auto const& chosen){uint32_t n=0;base.foreach_gate([&](auto g){n+=bool(chosen[g]);});return n;};
  auto chosen=select(base,0);lambda_milli=0;
  if(count(chosen)>max_copies)
  {
    uint32_t low=0,high=1000*(3*base.num_gates()+base.num_pos()+1);
    chosen=select(base,high);
    while(high-low>1)
    {
      auto mid=low+(high-low)/2;
      auto trial=select(base,mid);
      if(count(trial)>max_copies)low=mid;
      else{high=mid;chosen=std::move(trial);}
    }
    lambda_milli=high;
  }
  auto result=materialize(base,chosen,st);
  if(st.copies>max_copies || st.after_gates>st.before_gates+max_copies || st.after_raw>st.before_raw)
    throw std::runtime_error("cut selection violated the charged cost or gate bound");
  return result;
}
} // namespace esl_phase_copy_cut
