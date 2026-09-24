#!/usr/bin/env python3
"""Barnett Qompression closure kernel: frozen operator, exact reverse gate, complete cost."""
from dataclasses import dataclass
from typing import Callable,Iterable,Any
@dataclass(frozen=True)
class Counted:
 P:int;M:int;C0:int;F0:int;R:int;A:int
 @property
 def total(self):return self.P+self.M+self.C0+self.F0+self.R+self.A
@dataclass
class Candidate:
 name:str; retained:Any; reconstruct:Callable[[],bytes]; counted:Counted
@dataclass
class Record:
 name:str;exact:bool;smaller:bool;before:int;after:int;retained:bool;reason:str
class FrozenBarnettGate:
 def __init__(self,target:bytes,baseline:Counted):
  self._target=target;self.current=baseline;self.failures=[];self.accepted=[]
 def gate(self,c:Candidate):
  try:out=c.reconstruct()
  except Exception as e:
   r=Record(c.name,False,False,self.current.total,c.counted.total,False,"reconstruction exception:"+repr(e));self.failures.append(r);return r
  exact=out==self._target;smaller=c.counted.total<self.current.total;ok=exact and smaller
  reason="accepted" if ok else ("F1 exact reconstruction failed" if not exact else "cost did not strictly decrease")
  r=Record(c.name,exact,smaller,self.current.total,c.counted.total,ok,reason)
  (self.accepted if ok else self.failures).append(r)
  if ok:self.current=c.counted
  return r
def fixed_point(gate,generator):
 while True:
  retained=False
  for c in generator(gate.current):
   if gate.gate(c).retained:retained=True;break
  if not retained:return gate.current
def wrapper(phi,ncl,common,maxinv,delta,sigma,up,equiv,cost,current,*operands):
 minima=[ncl(phi(x)) for x in operands]
 J=maxinv(common(*minima));tau=delta(J,*minima);P=sigma(tau);hats=up(P)
 exact=len(hats)==len(operands) and all(equiv(h,x) for h,x in zip(hats,operands))
 newcost=cost(P)
 return P,exact and newcost<current,{"J":J,"tau":tau,"cost":newcost}
