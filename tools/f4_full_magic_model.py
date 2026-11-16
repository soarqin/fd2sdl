"""Finite integer source recurrences, lifted from independently checked join04.
No C/original execution, decoder, filesystem access or shipping state.
"""
import struct

class IntegerFX:
 def __init__(self,id,side,seed,tables,wheel_duration):
  assert id in (0,1,2,3,4,5,7,8,9) and side in (0,2) and not(id==9 and side)
  self.id=id;self.side=side;self.seed=seed;self.hash=14695981039346656037;self.trace=[];self.rng_count=0;self.phase=0
  self.state={};self.context={};self.tables=tables;self.wheel_duration=wheel_duration
 def record(self,kind,value,delta=0):
  for w in (kind,self.phase,value,delta):
   for b in struct.pack('<I',w&0xffffffff):self.hash=((self.hash^b)*1099511628211)&0xffffffffffffffff
  self.trace.append((kind,self.phase,value,delta))
 def draw(self,part,x=0,y=0):self.record(0,part,x+640*y)
 def cue(self,kind,sample):self.record(kind,sample)
 def random(self,length):
  s=(self.seed+0x9014)&65535;self.seed=((s<<3)|(s>>13))&65535;self.rng_count+=1
  self.record(3,self.seed);return (self.seed%2)*length
 def step(self,p):
  self.phase=p;s=self.state;k=self.id;out=0;bias=not self.side
  if k in (0,1):
   if p==3:s['age']=[-2*i for i in range(8)];out=31 if k else 28
   if p in (4,5):
    for i,age in enumerate(s['age'][:8 if k else 7]):
     if not k and p==4 and age==3:self.cue(1,1)
     if 0<=age<(15 if k else 16):
      if not k:
       if self.tables['rain0_front'][i]==(p==4):self.draw(age,self.tables['rain0_x'][i]+148*bias,self.tables['rain0_y'][i])
      else:
       opposite=i>=4 if p==4 else i<4;at=(i+4)%8 if opposite else i
       self.draw(age+15*opposite,self.tables['rain1_x'][at]+80+148*bias,self.tables['rain1_y'][at])
    if p==5:
     for i in range(8 if k else 7):
      s['age'][i]+=1
      if s['age'][i]==9:out=1
      if k and s['age'][i]==5:self.cue(1,1)
  elif k==2:
   if p==0:s.update(frame=0,timer=0);out=29
   elif p==3:s['frame']=16;out=12
   elif p==6:self.cue(1,3);s['frame']=10;out=10
   elif p in (1,2,7,8):
    if p in (2,8) and s['frame']==7:self.cue(1,1)
    if bool(self.side)==(p in (1,7)):
     if s['frame']==10 and p in (1,2):s['frame']=15
     self.draw(s['frame']);s['timer']=(s['timer']+1)&255
     if s['timer']==self.wheel_duration[s['frame']]:s['timer']=0;s['frame']=(s['frame']+1)&255
    if p in (2,8) and s['frame']==16:self.draw(16)
   elif p==4:
    if self.side:self.draw(15,1,-1)
   elif p==5:
    if not self.side:self.draw(15,-1,-1)
    self.draw(s['frame'],1,-1);s['frame']=(s['frame']+1)&255
    if s['frame']==17:self.cue(1,2);out=1
    elif s['frame']==18:s['frame']=16
  elif k in (3,4,5,7):
   if p==0:
    n=12 if k==3 else 4 if k==7 else 6
    s.update(age=[-(3 if k==7 else 2)*i for i in range(n)],slot=list(range(n)),next=n,stop=0,flip=0)
    if k in (4,5):s['variant']=[self.random(6 if k==5 else 7) for _ in range(6)]
    out=1 if k==5 else 2
   elif p==3:out=40 if k==3 else 32 if k==7 else 12
   elif p==6:s['stop']=1;out=20 if k==3 else 16 if k==7 else 8
   elif p in (2,5,8):
    if k in (3,7):s['flip']^=1
    n=12 if k==3 else 3 if k==7 else 6
    for i in range(n):
     age=s['age'][i];slot=s['slot'][i]
     limit=11 if k==3 else 5 if k==7 else 6 if k==5 else 7
     if 0<=age<limit:
      if k==3:self.draw(age+self.tables['shower_parts'][slot],self.tables['shower_x'][slot]+20*bias,-self.tables['shower_y'][slot])
      elif k==7:self.draw(age,self.tables['surge_x'][slot]+130*bias)
      else:self.draw(age+s['variant'][i],self.tables['dust4_x'][slot]+143*bias)
     if k in (3,7) and s['flip']:continue
     if k==3 and age==0 and self.tables['shower_parts'][slot]:self.cue(1,2)
     elif k==7 and age==1:
      if i<2:self.cue(1 if i==0 else 2,1)
     elif k in (4,5) and age==0:
      if k==4 or i==0:self.cue(1,1)
      elif i==3:self.cue(2,1)
     age+=1;s['age'][i]=age
     if age==(2 if k in (5,7) else 3):
      if k==3 and not self.tables['shower_parts'][slot]:self.cue(2,1)
      out=1
     stop_age=11 if k==3 else 7 if k in (5,7) else 8
     if age==stop_age and not s['stop']:
      s['next']=(s['next']+1)%(12 if k==3 else 10);s['slot'][i]=s['next'];s['age'][i]=0
      if k in (4,5):s['variant'][i]=self.random(6 if k==5 else 7)
  elif k==8:
   if p==0:s['age']=[-2*i for i in range(16)];out=3
   elif p==3:out=34
   elif p==6:out=2
   elif p in (2,5):
    for i,age in enumerate(s['age']):
     if 0<=age<8:self.draw(age+self.tables['burst_parts'][i])
     if age==0:self.cue(1,1)
     if age==4:self.cue(2,2)
     s['age'][i]+=1
     if s['age'][i]==4:out=1
  elif k==9:
   if p==0:s.update(frame=1,blink=0);out=20
   elif p in (1,7):
    if not s['blink']:self.draw(0)
    s['blink']^=1
   elif p==3:out=60
   elif p==4:self.draw(0)
   elif p==5:
    self.draw(s['frame']>>1)
    if s['frame']==6:self.cue(1,1)
    elif s['frame']==36:self.cue(2,2)
    s['frame']=(s['frame']+1)&255;out=int(16<s['frame']<44)
   elif p==6:out=20
  self.record(4,out);return out
 def cast(self,targets):
  for _ in range(self.step(0)):self.step(1);self.step(2)
  impacts=[]
  for target in range(targets):
   impacts.append(0)
   for _ in range(self.step(3)):self.step(4);impacts[-1]+=self.step(5)==1
   if target+1<targets:
    for _ in range(9):self.step(4);self.step(5)
  for _ in range(self.step(6)):self.step(7);self.step(8)
  return impacts
